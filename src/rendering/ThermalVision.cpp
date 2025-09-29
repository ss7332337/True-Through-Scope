#include "ThermalVision.h"
#include "rendering/TemperatureDetection.h"
#include <d3dcompiler.h>
#include <random>
#include "RE/Fallout.hpp"

#pragma comment(lib, "d3dcompiler.lib")

namespace ThroughScope
{
    ThermalVision* ThermalVision::s_instance = nullptr;

    ThermalVision* ThermalVision::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new ThermalVision();
        }
        return s_instance;
    }

    bool ThermalVision::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        m_device = device;
        m_context = context;

        if (!CreateShaders())
        {
            return false;
        }

        if (!CreateResources())
        {
            return false;
        }

        if (!CreatePaletteLookupTextures())
        {
            return false;
        }

        return true;
    }

    void ThermalVision::Shutdown()
    {
        m_vertexShader.Reset();
        m_thermalPixelShader.Reset();
        m_edgeDetectShader.Reset();
        m_temperatureAnalysisCS.Reset();
        m_constantBuffer.Reset();
        m_paletteLUT.Reset();
        m_paletteSRV.Reset();
        m_fixedPatternNoise.Reset();
        m_fixedPatternNoiseSRV.Reset();
        m_temporalNoise.Reset();
        m_temporalNoiseSRV.Reset();
        m_tempTexture.Reset();
        m_tempRTV.Reset();
        m_tempSRV.Reset();
        m_linearSampler.Reset();
        m_pointSampler.Reset();
    }

    bool ThermalVision::CreateShaders()
    {
        HRESULT hr;

        // 编译热成像像素着色器
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        const char* pixelShaderCode = R"(
#include "ThermalVision_PS.hlsl"
)";

        // 这里应该从文件加载或使用预编译的着色器
        // 为了简化，暂时使用内嵌的简化版本
        const char* simplifiedPS = R"(
cbuffer ThermalConstants : register(b0)
{
    float4 temperatureRange;
    float4 noiseParams;
    float4 edgeParams;
    float4 gainLevel;
    float4x4 noiseMatrix;
};

Texture2D<float4> sceneTexture : register(t0);
Texture2D<float4> paletteLUT : register(t2);
SamplerState linearSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.texCoord;
    float4 sceneColor = sceneTexture.Sample(linearSampler, uv);

    // 简单的亮度到温度映射
    float luminance = dot(sceneColor.rgb, float3(0.299, 0.587, 0.114));

    // 应用增益和电平
    float temperature = saturate((luminance - 0.5 + gainLevel.y) * gainLevel.x + 0.5);

    // 应用调色板
    int paletteIndex = (int)gainLevel.w;
    float2 lutCoord = float2(temperature, (paletteIndex + 0.5) / 8.0);
    float4 thermalColor = paletteLUT.Sample(linearSampler, lutCoord);

    return float4(thermalColor.rgb, 1.0);
}
)";

        hr = D3DCompile(simplifiedPS, strlen(simplifiedPS), "ThermalVision_PS", nullptr, nullptr,
                       "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &psBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                logger::error("Shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }
            return false;
        }

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                        nullptr, m_thermalPixelShader.GetAddressOf());
        if (psBlob) psBlob->Release();
        if (FAILED(hr)) return false;

        // 创建简单的全屏顶点着色器
        const char* vertexShaderCode = R"(
struct VSInput
{
    uint id : SV_VertexID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // 生成全屏三角形
    float2 texcoord = float2((input.id << 1) & 2, input.id & 2);
    output.texCoord = texcoord;
    output.position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    return output;
}
)";

        ID3DBlob* vsBlob = nullptr;
        hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), "FullscreenVS", nullptr, nullptr,
                       "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vsBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                logger::error("Vertex shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }
            return false;
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         nullptr, m_vertexShader.GetAddressOf());
        if (vsBlob) vsBlob->Release();
        if (FAILED(hr)) return false;

        return true;
    }

    bool ThermalVision::CreateResources()
    {
        HRESULT hr;

        // 创建常量缓冲区
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(ThermalConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
        if (FAILED(hr)) return false;

        // 创建采样器
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

        hr = m_device->CreateSamplerState(&samplerDesc, m_linearSampler.GetAddressOf());
        if (FAILED(hr)) return false;

        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        hr = m_device->CreateSamplerState(&samplerDesc, m_pointSampler.GetAddressOf());
        if (FAILED(hr)) return false;

        // 创建噪声纹理
        D3D11_TEXTURE2D_DESC noiseDesc = {};
        noiseDesc.Width = 512;
        noiseDesc.Height = 512;
        noiseDesc.MipLevels = 1;
        noiseDesc.ArraySize = 1;
        noiseDesc.Format = DXGI_FORMAT_R32_FLOAT;
        noiseDesc.SampleDesc.Count = 1;
        noiseDesc.Usage = D3D11_USAGE_DEFAULT;
        noiseDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // 生成固定模式噪声
        std::vector<float> fixedNoiseData(512 * 512);
        std::mt19937 gen(12345);
        std::normal_distribution<float> dist(0.0f, 0.01f);

        // 创建带有列条纹的固定模式噪声（模拟真实传感器特性）
        for (int y = 0; y < 512; ++y)
        {
            float columnNoise = dist(gen) * 0.5f; // 列噪声
            for (int x = 0; x < 512; ++x)
            {
                float pixelNoise = dist(gen);
                fixedNoiseData[y * 512 + x] = columnNoise + pixelNoise;
            }
        }

        D3D11_SUBRESOURCE_DATA noiseInitData = {};
        noiseInitData.pSysMem = fixedNoiseData.data();
        noiseInitData.SysMemPitch = 512 * sizeof(float);

        hr = m_device->CreateTexture2D(&noiseDesc, &noiseInitData, m_fixedPatternNoise.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = noiseDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = m_device->CreateShaderResourceView(m_fixedPatternNoise.Get(), &srvDesc, m_fixedPatternNoiseSRV.GetAddressOf());
        if (FAILED(hr)) return false;

        // 创建时间相关噪声纹理
        std::vector<float> temporalNoiseData(512 * 512);
        for (auto& val : temporalNoiseData)
        {
            val = dist(gen);
        }

        noiseInitData.pSysMem = temporalNoiseData.data();
        hr = m_device->CreateTexture2D(&noiseDesc, &noiseInitData, m_temporalNoise.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = m_device->CreateShaderResourceView(m_temporalNoise.Get(), &srvDesc, m_temporalNoiseSRV.GetAddressOf());
        if (FAILED(hr)) return false;

        return true;
    }

    bool ThermalVision::CreatePaletteLookupTextures()
    {
        // 创建调色板查找纹理（256x8，每行一个调色板）
        D3D11_TEXTURE2D_DESC lutDesc = {};
        lutDesc.Width = 256;
        lutDesc.Height = 8; // 8种调色板
        lutDesc.MipLevels = 1;
        lutDesc.ArraySize = 1;
        lutDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        lutDesc.SampleDesc.Count = 1;
        lutDesc.Usage = D3D11_USAGE_DEFAULT;
        lutDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        std::vector<uint32_t> lutData(256 * 8);

        auto generatePalette = [](uint32_t* row, int palette)
        {
            for (int i = 0; i < 256; ++i)
            {
                float t = i / 255.0f;
                uint8_t r, g, b;

                switch (palette)
                {
                case 0: // WhiteHot
                    r = g = b = (uint8_t)(t * 255);
                    break;

                case 1: // BlackHot
                    r = g = b = (uint8_t)((1.0f - t) * 255);
                    break;

                case 2: // IronBow
                {
                    // 黑 -> 蓝 -> 紫 -> 红 -> 橙 -> 黄 -> 白
                    if (t < 0.2f)
                    {
                        float s = t / 0.2f;
                        r = 0;
                        g = 0;
                        b = (uint8_t)(s * 139); // Dark blue
                    }
                    else if (t < 0.4f)
                    {
                        float s = (t - 0.2f) / 0.2f;
                        r = (uint8_t)(s * 199); // To magenta
                        g = 0;
                        b = (uint8_t)(139 - s * 28);
                    }
                    else if (t < 0.6f)
                    {
                        float s = (t - 0.4f) / 0.2f;
                        r = (uint8_t)(199 + s * 56); // To orange
                        g = (uint8_t)(s * 165);
                        b = (uint8_t)(111 - s * 111);
                    }
                    else if (t < 0.8f)
                    {
                        float s = (t - 0.6f) / 0.2f;
                        r = 255;
                        g = (uint8_t)(165 + s * 90); // To yellow-white
                        b = (uint8_t)(s * 255);
                    }
                    else
                    {
                        r = g = b = 255; // White
                    }
                    break;
                }

                case 3: // RainbowHC
                {
                    // 高对比度彩虹
                    float h = t * 300.0f; // 色相 0-300度（蓝到红）
                    float s = 1.0f;
                    float v = 0.5f + t * 0.5f;

                    // HSV to RGB
                    float c = v * s;
                    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
                    float m = v - c;

                    float rf, gf, bf;
                    if (h < 60) { rf = c; gf = x; bf = 0; }
                    else if (h < 120) { rf = x; gf = c; bf = 0; }
                    else if (h < 180) { rf = 0; gf = c; bf = x; }
                    else if (h < 240) { rf = 0; gf = x; bf = c; }
                    else if (h < 300) { rf = x; gf = 0; bf = c; }
                    else { rf = c; gf = 0; bf = x; }

                    r = (uint8_t)((rf + m) * 255);
                    g = (uint8_t)((gf + m) * 255);
                    b = (uint8_t)((bf + m) * 255);
                    break;
                }

                case 4: // Arctic
                {
                    // 冷蓝到热金
                    if (t < 0.5f)
                    {
                        float s = t / 0.5f;
                        r = (uint8_t)(s * 255);
                        g = (uint8_t)(s * 200);
                        b = (uint8_t)(200 - s * 100);
                    }
                    else
                    {
                        float s = (t - 0.5f) / 0.5f;
                        r = 255;
                        g = (uint8_t)(200 + s * 55);
                        b = (uint8_t)(100 - s * 100);
                    }
                    break;
                }

                case 5: // Lava
                {
                    // 冷蓝到热红
                    if (t < 0.33f)
                    {
                        float s = t / 0.33f;
                        r = (uint8_t)(s * 100);
                        g = 0;
                        b = (uint8_t)(100 + s * 155);
                    }
                    else if (t < 0.66f)
                    {
                        float s = (t - 0.33f) / 0.33f;
                        r = (uint8_t)(100 + s * 155);
                        g = (uint8_t)(s * 100);
                        b = (uint8_t)(255 - s * 255);
                    }
                    else
                    {
                        float s = (t - 0.66f) / 0.34f;
                        r = 255;
                        g = (uint8_t)(100 + s * 155);
                        b = (uint8_t)(s * 100);
                    }
                    break;
                }

                case 6: // Isothermal（等温线）
                {
                    // 创建条带效果
                    int band = (int)(t * 10);
                    if (band % 2 == 0)
                    {
                        r = g = b = (uint8_t)(t * 255);
                    }
                    else
                    {
                        // 彩色条带
                        float hue = (band / 10.0f) * 360.0f;
                        // 简化的HSV到RGB
                        r = (uint8_t)(sin(hue * 0.0174533f) * 127 + 128);
                        g = (uint8_t)(sin((hue + 120) * 0.0174533f) * 127 + 128);
                        b = (uint8_t)(sin((hue + 240) * 0.0174533f) * 127 + 128);
                    }
                    break;
                }

                case 7: // Medical（医疗）
                {
                    // 优化人体温度范围（35-40°C）
                    // 使用红-黄-白渐变
                    if (t < 0.3f)
                    {
                        // 冷区域 - 蓝紫色
                        float s = t / 0.3f;
                        r = (uint8_t)(s * 100);
                        g = 0;
                        b = (uint8_t)(100 + s * 100);
                    }
                    else if (t < 0.7f)
                    {
                        // 正常体温范围 - 绿到黄
                        float s = (t - 0.3f) / 0.4f;
                        r = (uint8_t)(100 + s * 155);
                        g = (uint8_t)(200 - s * 50);
                        b = (uint8_t)(200 - s * 200);
                    }
                    else
                    {
                        // 发热区域 - 橙红色
                        float s = (t - 0.7f) / 0.3f;
                        r = 255;
                        g = (uint8_t)(150 - s * 50);
                        b = (uint8_t)(s * 50);
                    }
                    break;
                }

                default:
                    r = g = b = (uint8_t)(t * 255);
                    break;
                }

                row[i] = (r << 0) | (g << 8) | (b << 16) | (0xFF << 24);
            }
        };

        // 为每个调色板生成数据
        for (int p = 0; p < 8; ++p)
        {
            generatePalette(&lutData[p * 256], p);
        }

        D3D11_SUBRESOURCE_DATA lutInitData = {};
        lutInitData.pSysMem = lutData.data();
        lutInitData.SysMemPitch = 256 * sizeof(uint32_t);

        HRESULT hr = m_device->CreateTexture2D(&lutDesc, &lutInitData, m_paletteLUT.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = lutDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = m_device->CreateShaderResourceView(m_paletteLUT.Get(), &srvDesc, m_paletteSRV.GetAddressOf());
        if (FAILED(hr)) return false;

        return true;
    }

    void ThermalVision::SetTemperatureRange(float min, float max)
    {
        m_config.minTemperature = min;
        m_config.maxTemperature = max;
    }

    float ThermalVision::EstimateTemperature(uint32_t objectFormID, float ambientTemp)
    {
        // 基于对象类型估算温度
        // 这里需要根据Fallout 4的具体对象类型来实现
        // 暂时返回一个基础实现

        // 人类NPC - 正常体温范围
        if (objectFormID & 0x01000000) // 简化的NPC检测
        {
            return 36.5f + (rand() % 20 - 10) * 0.1f; // 35.5-37.5°C
        }

        // 机器人/机械 - 较高温度
        if (objectFormID & 0x02000000)
        {
            return 45.0f + (rand() % 200) * 0.1f; // 45-65°C
        }

        // 默认环境温度
        return ambientTemp + (rand() % 100 - 50) * 0.1f;
    }

    void ThermalVision::UpdateConstantBuffer()
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            ThermalConstants* constants = (ThermalConstants*)mapped.pData;

            constants->temperatureRange.x = m_config.minTemperature;
            constants->temperatureRange.y = m_config.maxTemperature;
            constants->temperatureRange.z = 1.0f / (m_config.maxTemperature - m_config.minTemperature);
            constants->temperatureRange.w = m_config.sensitivity;

            constants->noiseParams.x = m_config.noiseIntensity;
            constants->noiseParams.y = m_frameTime;
            constants->noiseParams.z = (float)(rand() % 1000) / 1000.0f;
            constants->noiseParams.w = 2.0f; // Pattern scale

            constants->edgeParams.x = m_config.edgeStrength;
            constants->edgeParams.y = 0.1f; // Edge threshold
            constants->edgeParams.z = 0.0f;
            constants->edgeParams.w = 0.0f;

            constants->gainLevel.x = m_config.gain;
            constants->gainLevel.y = m_config.level;
            constants->gainLevel.z = m_config.emissivity;
            constants->gainLevel.w = (float)m_config.palette;

            // 生成噪声矩阵用于固定模式噪声变换
            DirectX::XMMATRIX noiseMatrix = DirectX::XMMatrixIdentity();
            DirectX::XMStoreFloat4x4(&constants->noiseMatrix, noiseMatrix);

            m_context->Unmap(m_constantBuffer.Get(), 0);
        }
    }

    void ThermalVision::ApplyThermalEffect(ID3D11RenderTargetView* source,
                                          ID3D11RenderTargetView* destination,
                                          ID3D11ShaderResourceView* depthSRV)
    {
        // 更新帧时间
        m_frameTime += 0.016f; // 假设60fps

        // 更新常量缓冲区
        UpdateConstantBuffer();

        // 创建源纹理的SRV
        ID3D11Resource* sourceResource = nullptr;
        source->GetResource(&sourceResource);

        ID3D11ShaderResourceView* sourceSRV = nullptr;
        HRESULT hr = m_device->CreateShaderResourceView(sourceResource, nullptr, &sourceSRV);
        if (FAILED(hr)) {
            logger::error("Failed to create source SRV for thermal effect");
            if (sourceResource) sourceResource->Release();
            return;
        }

        // 保存当前的渲染状态
        ID3D11RenderTargetView* oldRTV = nullptr;
        ID3D11DepthStencilView* oldDSV = nullptr;
        m_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);

        D3D11_VIEWPORT oldViewport;
        UINT numViewports = 1;
        m_context->RSGetViewports(&numViewports, &oldViewport);

        // 设置新的渲染目标
        m_context->OMSetRenderTargets(1, &destination, nullptr);

        // 设置视口
        ID3D11Resource* destResource = nullptr;
        destination->GetResource(&destResource);
        ID3D11Texture2D* destTexture = nullptr;
        destResource->QueryInterface(&destTexture);

        D3D11_TEXTURE2D_DESC destDesc;
        destTexture->GetDesc(&destDesc);

        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)destDesc.Width;
        viewport.Height = (float)destDesc.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);

        // 设置着色器和资源
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_thermalPixelShader.Get(), nullptr, 0);

        // 设置常量缓冲区
        m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

        // 设置纹理资源
        ID3D11ShaderResourceView* srvs[] = {
            sourceSRV,                      // t0: 场景纹理
            depthSRV,                       // t1: 深度纹理
            m_paletteSRV.Get(),            // t2: 调色板
            m_fixedPatternNoiseSRV.Get(),   // t3: 固定噪声
            m_temporalNoiseSRV.Get()        // t4: 时间噪声
        };
        m_context->PSSetShaderResources(0, 5, srvs);

        // 设置采样器
        ID3D11SamplerState* samplers[] = {
            m_linearSampler.Get(),
            m_pointSampler.Get()
        };
        m_context->PSSetSamplers(0, 2, samplers);

        // 禁用输入布局（全屏三角形不需要）
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 绘制全屏三角形
        m_context->Draw(3, 0);

        // 清理着色器资源以避免警告
        ID3D11ShaderResourceView* nullSRVs[5] = { nullptr };
        m_context->PSSetShaderResources(0, 5, nullSRVs);

        // 恢复原始渲染状态
        m_context->OMSetRenderTargets(1, &oldRTV, oldDSV);
        m_context->RSSetViewports(1, &oldViewport);

        // 释放资源
        if (sourceSRV) sourceSRV->Release();
        if (sourceResource) sourceResource->Release();
        if (destResource) destResource->Release();
        if (destTexture) destTexture->Release();
        if (oldRTV) oldRTV->Release();
        if (oldDSV) oldDSV->Release();
    }

    void ThermalVision::AnalyzeSceneTemperatures(ID3D11ShaderResourceView* sceneSRV)
    {
        // 分析场景温度分布用于自动增益
        // 使用Compute Shader进行直方图分析
    }

    void ThermalVision::UpdateAutoGain()
    {
        if (!m_config.autoGain)
            return;

        // 自动调整温度范围以优化对比度
        // 基于场景分析结果
    }
}
