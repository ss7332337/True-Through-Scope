#include "PCH.h"
#include "LuminancePass.h"
#include <d3dcompiler.h>
#include <d3d9.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d9.lib")

namespace ThroughScope
{
    LuminancePass* LuminancePass::s_instance = nullptr;

    LuminancePass* LuminancePass::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new LuminancePass();
        }
        return s_instance;
    }

    // 辅助函数: 从 .cso 或 .hlsl 文件加载 compute shader
    static HRESULT LoadComputeShaderFromFile(
        const WCHAR* csoPath,
        const WCHAR* hlslPath,
        LPCSTR entryPoint,
        ID3DBlob** ppBlobOut)
    {
        HRESULT hr = S_OK;

        // 首先尝试从 .cso 文件加载
        if (csoPath && D3DReadFileToBlob(csoPath, ppBlobOut) == S_OK)
        {
            return S_OK;
        }

        // 如果 .cso 不存在，从 .hlsl 编译
        if (hlslPath)
        {
            DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
            dwShaderFlags |= D3DCOMPILE_DEBUG;
            dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            ID3DBlob* errorBlob = nullptr;
            hr = D3DCompileFromFile(hlslPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entryPoint, "cs_5_0", dwShaderFlags, 0, ppBlobOut, &errorBlob);

            if (FAILED(hr))
            {
                if (errorBlob)
                {
                    logger::error("LuminancePass shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
                    errorBlob->Release();
                }
                return hr;
            }

            // 编译成功后，保存为 .cso 文件
            if (csoPath && *ppBlobOut)
            {
                D3DWriteBlobToFile(*ppBlobOut, csoPath, FALSE);
            }
        }

        return hr;
    }

    bool LuminancePass::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (m_initialized)
            return true;

        m_device = device;
        m_context = context;

        if (!CreateShaders())
        {
            logger::error("LuminancePass: Failed to create shaders");
            return false;
        }

        // 创建常量缓冲区
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(LuminanceDownsampleConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, m_downsampleCB.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create downsample constant buffer");
            return false;
        }

        cbDesc.ByteWidth = sizeof(LuminanceFinalConstants);
        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_finalCB.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create final constant buffer");
            return false;
        }

        m_initialized = true;
        logger::info("LuminancePass: Initialized successfully");
        return true;
    }

    void LuminancePass::Shutdown()
    {
        m_downsampleCS.Reset();
        m_finalCS.Reset();
        m_downsampleCB.Reset();
        m_finalCB.Reset();
        m_intermediateTexture.Reset();
        m_intermediateUAV.Reset();
        m_intermediateSRV.Reset();
        m_weightTexture.Reset();
        m_weightUAV.Reset();
        m_weightSRV.Reset();
        m_luminanceTexture.Reset();
        m_luminanceUAV.Reset();
        m_luminanceSRV.Reset();
        m_prevLuminanceTexture.Reset();
        m_prevLuminanceSRV.Reset();
        m_initialized = false;
    }

    bool LuminancePass::CreateShaders()
    {
        HRESULT hr;

        // ========== Downsample Compute Shader ==========
        ID3DBlob* downsampleBlob = nullptr;
        hr = LoadComputeShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\LuminanceDownsample_CS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\LuminanceDownsample_CS.hlsl",
            "main", &downsampleBlob);

        if (FAILED(hr) || !downsampleBlob)
        {
            logger::error("LuminancePass: Failed to load downsample compute shader");
            return false;
        }

        hr = m_device->CreateComputeShader(downsampleBlob->GetBufferPointer(), downsampleBlob->GetBufferSize(),
                                           nullptr, m_downsampleCS.GetAddressOf());
        downsampleBlob->Release();
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create downsample compute shader: 0x{:X}", hr);
            return false;
        }

        // ========== Final Reduction Compute Shader ==========
        ID3DBlob* finalBlob = nullptr;
        hr = LoadComputeShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\LuminanceFinal_CS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\LuminanceFinal_CS.hlsl",
            "main", &finalBlob);

        if (FAILED(hr) || !finalBlob)
        {
            logger::error("LuminancePass: Failed to load final compute shader");
            return false;
        }

        hr = m_device->CreateComputeShader(finalBlob->GetBufferPointer(), finalBlob->GetBufferSize(),
                                           nullptr, m_finalCS.GetAddressOf());
        finalBlob->Release();
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create final compute shader: 0x{:X}", hr);
            return false;
        }

        return true;
    }

    bool LuminancePass::CreateResources(uint32_t width, uint32_t height)
    {
        // 计算中间纹理大小 (8x8 归约)
        uint32_t intermediateW = (width + 7) / 8;
        uint32_t intermediateH = (height + 7) / 8;

        // 检查是否需要重建资源
        if (m_intermediateTexture &&
            m_intermediateWidth == intermediateW &&
            m_intermediateHeight == intermediateH)
        {
            return true;  // 资源已存在且大小匹配
        }

        HRESULT hr;

        // 释放旧资源
        m_intermediateTexture.Reset();
        m_intermediateUAV.Reset();
        m_intermediateSRV.Reset();
        m_weightTexture.Reset();
        m_weightUAV.Reset();
        m_weightSRV.Reset();

        // ========== 创建中间纹理 (log-luminance 和) ==========
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = intermediateW;
        texDesc.Height = intermediateH;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R32_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D(&texDesc, nullptr, m_intermediateTexture.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create intermediate texture");
            return false;
        }

        hr = m_device->CreateUnorderedAccessView(m_intermediateTexture.Get(), nullptr, m_intermediateUAV.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create intermediate UAV");
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_intermediateTexture.Get(), nullptr, m_intermediateSRV.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create intermediate SRV");
            return false;
        }

        // ========== 创建权重纹理 (中心加权的权重和) ==========
        hr = m_device->CreateTexture2D(&texDesc, nullptr, m_weightTexture.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create weight texture");
            return false;
        }

        hr = m_device->CreateUnorderedAccessView(m_weightTexture.Get(), nullptr, m_weightUAV.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create weight UAV");
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_weightTexture.Get(), nullptr, m_weightSRV.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LuminancePass: Failed to create weight SRV");
            return false;
        }

        m_intermediateWidth = intermediateW;
        m_intermediateHeight = intermediateH;

        // ========== 创建最终 1x1 亮度纹理 (如果不存在) ==========
        if (!m_luminanceTexture)
        {
            texDesc.Width = 1;
            texDesc.Height = 1;

            hr = m_device->CreateTexture2D(&texDesc, nullptr, m_luminanceTexture.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("LuminancePass: Failed to create luminance texture");
                return false;
            }

            hr = m_device->CreateUnorderedAccessView(m_luminanceTexture.Get(), nullptr, m_luminanceUAV.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("LuminancePass: Failed to create luminance UAV");
                return false;
            }

            hr = m_device->CreateShaderResourceView(m_luminanceTexture.Get(), nullptr, m_luminanceSRV.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("LuminancePass: Failed to create luminance SRV");
                return false;
            }

            // ========== 创建上一帧亮度纹理 (用于时域平滑) ==========
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            hr = m_device->CreateTexture2D(&texDesc, nullptr, m_prevLuminanceTexture.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("LuminancePass: Failed to create prev luminance texture");
                return false;
            }

            hr = m_device->CreateShaderResourceView(m_prevLuminanceTexture.Get(), nullptr, m_prevLuminanceSRV.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("LuminancePass: Failed to create prev luminance SRV");
                return false;
            }

            // 初始化为默认亮度值 (中间灰度)
            float defaultLum = 0.18f;
            m_context->UpdateSubresource(m_prevLuminanceTexture.Get(), 0, nullptr, &defaultLum, sizeof(float), sizeof(float));
        }

        m_lastWidth = width;
        m_lastHeight = height;

        logger::debug("LuminancePass: Created resources for {}x{} -> intermediate {}x{}",
                      width, height, intermediateW, intermediateH);

        return true;
    }

    void LuminancePass::UpdateConstantBuffers(uint32_t width, uint32_t height, float deltaTime)
    {
        // 更新 downsample 常量
        m_downsampleConstants.InputSizeX = width;
        m_downsampleConstants.InputSizeY = height;
        m_downsampleConstants.OutputSizeX = m_intermediateWidth;
        m_downsampleConstants.OutputSizeY = m_intermediateHeight;

        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_downsampleCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_downsampleConstants, sizeof(LuminanceDownsampleConstants));
            m_context->Unmap(m_downsampleCB.Get(), 0);
        }

        // 更新 final 常量
        m_finalConstants.IntermediateSizeX = m_intermediateWidth;
        m_finalConstants.IntermediateSizeY = m_intermediateHeight;
        m_finalConstants.DeltaTime = deltaTime;
        // TotalWeight 将在 shader 中计算

        hr = m_context->Map(m_finalCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_finalConstants, sizeof(LuminanceFinalConstants));
            m_context->Unmap(m_finalCB.Get(), 0);
        }
    }

    ID3D11ShaderResourceView* LuminancePass::Compute(
        ID3D11ShaderResourceView* sceneTexture,
        uint32_t width,
        uint32_t height,
        float deltaTime)
    {
        D3DPERF_BeginEvent(0xFF0000FF, L"LuminancePass::Compute");

        if (!m_initialized || !sceneTexture)
        {
            logger::warn("LuminancePass::Compute: Not initialized or null scene texture");
            D3DPERF_EndEvent();
            return nullptr;
        }

        // 创建或重建资源（如果尺寸变化）
        if (!CreateResources(width, height))
        {
            logger::error("LuminancePass::Compute: Failed to create resources");
            D3DPERF_EndEvent();
            return nullptr;
        }

        UpdateConstantBuffers(width, height, deltaTime);

        // ========== Pass 1: Downsample + Log-Luminance + Center Weight ==========
        D3DPERF_BeginEvent(0xFF00FF00, L"Luminance_Downsample");
        {
            m_context->CSSetShader(m_downsampleCS.Get(), nullptr, 0);

            ID3D11ShaderResourceView* srvs[] = { sceneTexture };
            m_context->CSSetShaderResources(0, 1, srvs);

            ID3D11UnorderedAccessView* uavs[] = { m_intermediateUAV.Get(), m_weightUAV.Get() };
            m_context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

            ID3D11Buffer* cbs[] = { m_downsampleCB.Get() };
            m_context->CSSetConstantBuffers(0, 1, cbs);

            // Dispatch: 一个线程组处理 8x8 像素
            uint32_t groupsX = (width + 7) / 8;
            uint32_t groupsY = (height + 7) / 8;
            m_context->Dispatch(groupsX, groupsY, 1);
        }
        D3DPERF_EndEvent();

        // 解绑 UAV
        ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
        m_context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);

        // ========== Pass 2: Final Reduction to 1x1 + Temporal Smoothing ==========
        D3DPERF_BeginEvent(0xFFFF0000, L"Luminance_Final");
        {
            m_context->CSSetShader(m_finalCS.Get(), nullptr, 0);

            ID3D11ShaderResourceView* srvs[] = { m_intermediateSRV.Get(), m_weightSRV.Get(), m_prevLuminanceSRV.Get() };
            m_context->CSSetShaderResources(0, 3, srvs);

            ID3D11UnorderedAccessView* uavs[] = { m_luminanceUAV.Get() };
            m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

            ID3D11Buffer* cbs[] = { m_finalCB.Get() };
            m_context->CSSetConstantBuffers(0, 1, cbs);

            // 单个线程组完成最终归约
            m_context->Dispatch(1, 1, 1);
        }
        D3DPERF_EndEvent();

        // 解绑资源
        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        m_context->CSSetShaderResources(0, 3, nullSRVs);
        m_context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
        m_context->CSSetShader(nullptr, nullptr, 0);

        // 复制当前亮度到上一帧纹理（用于下一帧的时域平滑）
        m_context->CopyResource(m_prevLuminanceTexture.Get(), m_luminanceTexture.Get());

        D3DPERF_EndEvent();

        return m_luminanceSRV.Get();
    }
}
