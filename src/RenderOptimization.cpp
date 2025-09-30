#include "RenderOptimization.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <spdlog/spdlog.h>


namespace ThroughScope {

    // 简单的上采样着色器
    static const char* g_UpsampleShaderCode = R"(
        Texture2D sourceTexture : register(t0);
        SamplerState linearSampler : register(s0);

        struct VS_OUTPUT {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
        };

        VS_OUTPUT VSMain(uint vertexID : SV_VertexID) {
            VS_OUTPUT output;

            // 生成全屏三角形
            float2 texCoord = float2((vertexID << 1) & 2, vertexID & 2);
            output.position = float4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);
            output.position.y *= -1.0f;
            output.texCoord = texCoord;

            return output;
        }

        float4 PSMain(VS_OUTPUT input) : SV_TARGET {
            // 双线性上采样
            return sourceTexture.Sample(linearSampler, input.texCoord);
        }

        // 高质量上采样版本（可选）
        float4 PSMainHQ(VS_OUTPUT input) : SV_TARGET {
            float2 texelSize = 1.0f / float2(1920, 1080); // 需要通过常量缓冲区传入
            float4 color = float4(0, 0, 0, 0);

            // 4-tap上采样
            color += sourceTexture.Sample(linearSampler, input.texCoord + float2(-0.5f, -0.5f) * texelSize);
            color += sourceTexture.Sample(linearSampler, input.texCoord + float2( 0.5f, -0.5f) * texelSize);
            color += sourceTexture.Sample(linearSampler, input.texCoord + float2(-0.5f,  0.5f) * texelSize);
            color += sourceTexture.Sample(linearSampler, input.texCoord + float2( 0.5f,  0.5f) * texelSize);

            return color * 0.25f;
        }
    )";

    RenderOptimization* RenderOptimization::GetSingleton() {
        static RenderOptimization instance;
        return &instance;
    }

    bool RenderOptimization::Initialize(ID3D11Device* device) {
        if (!device) return false;

        m_device = device;

        // 创建线性采样器
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        HRESULT hr = device->CreateSamplerState(&samplerDesc, m_linearSampler.GetAddressOf());
        if (FAILED(hr)) return false;

        // 编译上采样着色器
        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

        hr = D3DCompile(g_UpsampleShaderCode, strlen(g_UpsampleShaderCode),
                       "UpsampleShader", nullptr, nullptr,
                       "VSMain", "vs_5_0", 0, 0,
                       &vsBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                logger::error("VS compilation error: {}", (char*)errorBlob->GetBufferPointer());
            }
            return false;
        }

        hr = D3DCompile(g_UpsampleShaderCode, strlen(g_UpsampleShaderCode),
                       "UpsampleShader", nullptr, nullptr,
                       "PSMain", "ps_5_0", 0, 0,
                       &psBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                logger::error("PS compilation error: {}", (char*)errorBlob->GetBufferPointer());
            }
            return false;
        }

        // 创建着色器
        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                       nullptr, m_upsampleVS.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                      nullptr, m_upsamplePS.GetAddressOf());
        if (FAILED(hr)) return false;

        // 创建常量缓冲区（用于传递分辨率等参数）
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.ByteWidth = sizeof(float) * 16; // 预留空间
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = device->CreateBuffer(&cbDesc, nullptr, m_upsampleCB.GetAddressOf());
        if (FAILED(hr)) return false;

        // 设置默认质量级别
        SetQualityLevel(QualityLevel::Medium);

        logger::info("RenderOptimization initialized successfully");
        return true;
    }

    void RenderOptimization::Cleanup() {
        m_downsampledColorTexture.Reset();
        m_downsampledDepthTexture.Reset();
        m_downsampledRTV.Reset();
        m_downsampledDSV.Reset();
        m_downsampledSRV.Reset();
        m_downsampledDepthSRV.Reset();
        m_upsamplePS.Reset();
        m_upsampleVS.Reset();
        m_linearSampler.Reset();
        m_upsampleCB.Reset();
    }

    bool RenderOptimization::CreateDownsampledRenderTargets(UINT originalWidth, UINT originalHeight) {
        if (!m_device) return false;

        UINT scaledWidth, scaledHeight;
        GetScaledResolution(originalWidth, originalHeight, scaledWidth, scaledHeight);

        // 如果分辨率没有变化，保留现有资源
        if (scaledWidth == m_currentWidth && scaledHeight == m_currentHeight && m_downsampledRTV) {
            return true;
        }

        m_currentWidth = scaledWidth;
        m_currentHeight = scaledHeight;

        // 清理旧资源
        m_downsampledColorTexture.Reset();
        m_downsampledDepthTexture.Reset();
        m_downsampledRTV.Reset();
        m_downsampledDSV.Reset();
        m_downsampledSRV.Reset();
        m_downsampledDepthSRV.Reset();

        // 创建降采样的颜色纹理
        D3D11_TEXTURE2D_DESC colorDesc = {};
        colorDesc.Width = scaledWidth;
        colorDesc.Height = scaledHeight;
        colorDesc.MipLevels = 1;
        colorDesc.ArraySize = 1;
        colorDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT; // 使用更紧凑的格式节省带宽
        colorDesc.SampleDesc.Count = 1;
        colorDesc.Usage = D3D11_USAGE_DEFAULT;
        colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&colorDesc, nullptr, m_downsampledColorTexture.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create downsampled color texture: 0x{:X}", hr);
            return false;
        }

        // 创建RTV
        hr = m_device->CreateRenderTargetView(m_downsampledColorTexture.Get(), nullptr, m_downsampledRTV.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create downsampled RTV: 0x{:X}", hr);
            return false;
        }

        // 创建SRV
        hr = m_device->CreateShaderResourceView(m_downsampledColorTexture.Get(), nullptr, m_downsampledSRV.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create downsampled SRV: 0x{:X}", hr);
            return false;
        }

        // 创建降采样的深度纹理
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = scaledWidth;
        depthDesc.Height = scaledHeight;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        hr = m_device->CreateTexture2D(&depthDesc, nullptr, m_downsampledDepthTexture.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create downsampled depth texture: 0x{:X}", hr);
            return false;
        }

        // 创建DSV
        hr = m_device->CreateDepthStencilView(m_downsampledDepthTexture.Get(), nullptr, m_downsampledDSV.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create downsampled DSV: 0x{:X}", hr);
            return false;
        }

        logger::info("Created downsampled render targets: {}x{} (scale: {})",
                    scaledWidth, scaledHeight, m_settings.resolutionScale);

        return true;
    }

    void RenderOptimization::SetQualityLevel(QualityLevel level) {
        m_settings.qualityLevel = level;
        SetQualityPreset(level);

        logger::info("Quality level set to: {}", static_cast<int>(level));
    }

    void RenderOptimization::SetQualityPreset(QualityLevel level) {
        switch (level) {
            case QualityLevel::Ultra:
                m_settings.resolutionScale = 1.0f;
                m_settings.skipShadows = false;
                m_settings.skipReflections = false;
                m_settings.skipAO = false;
                m_settings.skipVolumetrics = false;
                m_settings.skipPostProcessing = false;
                break;

            case QualityLevel::High:
                m_settings.resolutionScale = 0.75f;
                m_settings.skipShadows = false;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = false;
                break;

            case QualityLevel::Medium:
                m_settings.resolutionScale = 0.5f;
                m_settings.skipShadows = true;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = true;
                break;

            case QualityLevel::Low:
                m_settings.resolutionScale = 0.35f;
                m_settings.skipShadows = true;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = true;
                break;

            case QualityLevel::Performance:
                m_settings.resolutionScale = 0.25f;
                m_settings.skipShadows = true;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = true;
                break;
        }
    }

    void RenderOptimization::GetScaledResolution(UINT originalWidth, UINT originalHeight,
                                                 UINT& scaledWidth, UINT& scaledHeight) const {
        float scale = m_settings.resolutionScale;

        // 动态质量调整
        if (m_settings.useDynamicQuality) {
            scale = m_currentQualityScale;
        }

        scaledWidth = static_cast<UINT>(originalWidth * scale);
        scaledHeight = static_cast<UINT>(originalHeight * scale);

        // 确保是偶数（某些GPU需要）
        scaledWidth = (scaledWidth + 1) & ~1;
        scaledHeight = (scaledHeight + 1) & ~1;

        // 设置最小分辨率
        scaledWidth = std::max(scaledWidth, 320u);
        scaledHeight = std::max(scaledHeight, 180u);
    }

    void RenderOptimization::Upsample(ID3D11DeviceContext* context,
                                      ID3D11ShaderResourceView* sourceSRV,
                                      ID3D11RenderTargetView* destRTV,
                                      UINT destWidth, UINT destHeight) {
        if (!context || !sourceSRV || !destRTV) return;

        // 保存当前状态
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTV;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
        context->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());

        D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        context->RSGetViewports(&numViewports, oldViewports);

        // 设置新的渲染目标
        context->OMSetRenderTargets(1, &destRTV, nullptr);

        // 设置视口
        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<FLOAT>(destWidth);
        vp.Height = static_cast<FLOAT>(destHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        // 设置着色器和资源
        context->VSSetShader(m_upsampleVS.Get(), nullptr, 0);
        context->PSSetShader(m_upsamplePS.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, &sourceSRV);
        context->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());

        // 绘制全屏三角形
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(nullptr);
        context->Draw(3, 0);

        // 清理
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(0, 1, &nullSRV);

        // 恢复原始状态
        context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
        context->RSSetViewports(numViewports, oldViewports);
    }

    void RenderOptimization::UpdateDynamicQuality(float frameTime) {
        if (!m_settings.useDynamicQuality) return;

        const float targetDiff = frameTime - m_targetFrameTime;

        if (std::abs(targetDiff) > m_qualityAdjustThreshold) {
            if (targetDiff > 0) {
                // 降低质量
                m_currentQualityScale = std::max(0.25f, m_currentQualityScale - 0.05f);
            } else {
                // 提高质量
                m_currentQualityScale = std::min(1.0f, m_currentQualityScale + 0.05f);
            }

            // logger::debug("Dynamic quality adjusted to: {}", m_currentQualityScale);
        }
    }
}
