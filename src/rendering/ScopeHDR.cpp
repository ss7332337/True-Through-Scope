#include "PCH.h"
#include "ScopeHDR.h"
#include <d3dcompiler.h>
#include <d3d9.h>  // for D3DPERF_BeginEvent / D3DPERF_EndEvent

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d9.lib")

namespace ThroughScope
{
    ScopeHDR* ScopeHDR::s_instance = nullptr;

    ScopeHDR* ScopeHDR::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new ScopeHDR();
        }
        return s_instance;
    }

    bool ScopeHDR::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (m_initialized)
            return true;

        m_device = device;
        m_context = context;

        if (!CreateShaders())
        {
            logger::error("ScopeHDR: Failed to create shaders");
            return false;
        }

        if (!CreateResources())
        {
            logger::error("ScopeHDR: Failed to create resources");
            return false;
        }

        m_initialized = true;
        logger::info("ScopeHDR: Initialized successfully");
        return true;
    }

    void ScopeHDR::Shutdown()
    {
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        m_constantBuffer.Reset();
        m_linearSampler.Reset();
        m_pointSampler.Reset();
        m_defaultBloomTexture.Reset();
        m_defaultBloomSRV.Reset();
        m_defaultLuminanceTexture.Reset();
        m_defaultLuminanceSRV.Reset();
        m_defaultMaskTexture.Reset();
        m_defaultMaskSRV.Reset();

        // 清理渲染状态
        m_noBlendState.Reset();
        m_noDepthState.Reset();
        m_rasterizerState.Reset();

        m_initialized = false;
    }

    // 辅助函数: 从 .cso 或 .hlsl 文件加载 shader
    static HRESULT LoadShaderFromFile(
        const WCHAR* csoPath,
        const WCHAR* hlslPath,
        LPCSTR entryPoint,
        LPCSTR shaderModel,
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
                                    entryPoint, shaderModel, dwShaderFlags, 0, ppBlobOut, &errorBlob);

            if (FAILED(hr))
            {
                if (errorBlob)
                {
                    logger::error("ScopeHDR shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
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

    bool ScopeHDR::CreateShaders()
    {
        HRESULT hr;

        // ========== 顶点着色器 ==========
        ID3DBlob* vsBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\ScopeHDR_VS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\ScopeHDR_VS.hlsl",
            "main", "vs_5_0", &vsBlob);

        if (FAILED(hr) || !vsBlob)
        {
            logger::error("ScopeHDR: Failed to load vertex shader from file");
            return false;
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         nullptr, m_vertexShader.GetAddressOf());
        vsBlob->Release();
        if (FAILED(hr))
        {
            logger::error("ScopeHDR: Failed to create vertex shader: 0x{:X}", hr);
            return false;
        }

        // ========== 像素着色器 ==========
        ID3DBlob* psBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\ScopeHDR_PS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\ScopeHDR_PS.hlsl",
            "main", "ps_5_0", &psBlob);

        if (FAILED(hr) || !psBlob)
        {
            logger::error("ScopeHDR: Failed to load pixel shader from file");
            return false;
        }

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                        nullptr, m_pixelShader.GetAddressOf());
        psBlob->Release();
        if (FAILED(hr))
        {
            logger::error("ScopeHDR: Failed to create pixel shader: 0x{:X}", hr);
            return false;
        }

        return true;
    }

    bool ScopeHDR::CreateResources()
    {
        HRESULT hr;

        // ========== 常量缓冲区 ==========
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(ScopeHDRConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopeHDR: Failed to create constant buffer");
            return false;
        }

        // ========== 采样器 ==========
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&samplerDesc, m_linearSampler.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopeHDR: Failed to create linear sampler");
            return false;
        }

        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        hr = m_device->CreateSamplerState(&samplerDesc, m_pointSampler.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopeHDR: Failed to create point sampler");
            return false;
        }

        // ========== 默认纹理 ==========

        // 默认 Bloom 纹理 (1x1 黑色)
        {
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = 1;
            texDesc.Height = 1;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            float blackPixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = blackPixel;
            initData.SysMemPitch = sizeof(blackPixel);

            hr = m_device->CreateTexture2D(&texDesc, &initData, m_defaultBloomTexture.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                m_device->CreateShaderResourceView(m_defaultBloomTexture.Get(), nullptr, m_defaultBloomSRV.GetAddressOf());
            }
        }

        // 默认 Luminance 纹理 (1x1，固定亮度值)
        {
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = 1;
            texDesc.Height = 1;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R32_FLOAT;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            float luminanceValue = 0.18f;  // 中间灰度
            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = &luminanceValue;
            initData.SysMemPitch = sizeof(float);

            hr = m_device->CreateTexture2D(&texDesc, &initData, m_defaultLuminanceTexture.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                m_device->CreateShaderResourceView(m_defaultLuminanceTexture.Get(), nullptr, m_defaultLuminanceSRV.GetAddressOf());
            }
        }

        // 默认 Mask 纹理 (1x1，值为 0)
        {
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = 1;
            texDesc.Height = 1;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            uint8_t maskValue = 0;
            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = &maskValue;
            initData.SysMemPitch = 1;

            hr = m_device->CreateTexture2D(&texDesc, &initData, m_defaultMaskTexture.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                m_device->CreateShaderResourceView(m_defaultMaskTexture.Get(), nullptr, m_defaultMaskSRV.GetAddressOf());
            }
        }

        // ========== 渲染状态 ==========

        // 创建禁用混合的 Blend State
        {
            D3D11_BLEND_DESC blendDesc = {};
            blendDesc.AlphaToCoverageEnable = FALSE;
            blendDesc.IndependentBlendEnable = FALSE;
            blendDesc.RenderTarget[0].BlendEnable = FALSE;  // 关键: 禁用混合
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

            hr = m_device->CreateBlendState(&blendDesc, m_noBlendState.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("ScopeHDR: Failed to create no-blend state: 0x{:X}", hr);
                return false;
            }
        }

        // 创建禁用深度测试的 Depth Stencil State
        {
            D3D11_DEPTH_STENCIL_DESC dsDesc = {};
            dsDesc.DepthEnable = FALSE;
            dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            dsDesc.StencilEnable = FALSE;

            hr = m_device->CreateDepthStencilState(&dsDesc, m_noDepthState.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("ScopeHDR: Failed to create no-depth state: 0x{:X}", hr);
                return false;
            }
        }

        // 创建无剔除的光栅化状态
        {
            D3D11_RASTERIZER_DESC rsDesc = {};
            rsDesc.FillMode = D3D11_FILL_SOLID;
            rsDesc.CullMode = D3D11_CULL_NONE;
            rsDesc.FrontCounterClockwise = FALSE;
            rsDesc.DepthBias = 0;
            rsDesc.DepthBiasClamp = 0.0f;
            rsDesc.SlopeScaledDepthBias = 0.0f;
            rsDesc.DepthClipEnable = TRUE;
            rsDesc.ScissorEnable = FALSE;
            rsDesc.MultisampleEnable = FALSE;
            rsDesc.AntialiasedLineEnable = FALSE;

            hr = m_device->CreateRasterizerState(&rsDesc, m_rasterizerState.GetAddressOf());
            if (FAILED(hr))
            {
                logger::error("ScopeHDR: Failed to create rasterizer state: 0x{:X}", hr);
                return false;
            }
        }

        return true;
    }

    void ScopeHDR::UpdateConstantBuffer()
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_constants, sizeof(ScopeHDRConstants));
            m_context->Unmap(m_constantBuffer.Get(), 0);
        }
    }

    void ScopeHDR::Apply(
        ID3D11ShaderResourceView* sceneTexture,
        ID3D11ShaderResourceView* bloomTexture,
        ID3D11ShaderResourceView* luminanceTexture,
        ID3D11ShaderResourceView* maskTexture,
        ID3D11RenderTargetView* outputRTV)
    {
        D3DPERF_BeginEvent(0xFFFF0000, L"ScopeHDR::Apply");

        if (!m_initialized || !sceneTexture || !outputRTV)
        {
            logger::warn("ScopeHDR::Apply: Invalid parameters or not initialized");
            D3DPERF_EndEvent();
            return;
        }

        // 更新常量缓冲区
        UpdateConstantBuffer();

        // 清除所有可能冲突的绑定
        ID3D11ShaderResourceView* nullSRVs[16] = {};
        m_context->PSSetShaderResources(0, 16, nullSRVs);

        // 设置渲染目标
        m_context->OMSetRenderTargets(1, &outputRTV, nullptr);

        // 设置 viewport (从渲染目标获取尺寸)
        ID3D11Resource* rtvResource = nullptr;
        outputRTV->GetResource(&rtvResource);
        if (rtvResource)
        {
            ID3D11Texture2D* rtvTexture = nullptr;
            if (SUCCEEDED(rtvResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&rtvTexture)))
            {
                D3D11_TEXTURE2D_DESC desc;
                rtvTexture->GetDesc(&desc);

                D3D11_VIEWPORT vp = {};
                vp.Width = static_cast<float>(desc.Width);
                vp.Height = static_cast<float>(desc.Height);
                vp.MinDepth = 0.0f;
                vp.MaxDepth = 1.0f;
                m_context->RSSetViewports(1, &vp);

                rtvTexture->Release();
            }
            rtvResource->Release();
        }

        // 设置 shaders
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        // 设置常量缓冲区
        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        m_context->PSSetConstantBuffers(0, 1, cbs);

        // 设置纹理 (使用默认纹理作为 fallback)
        ID3D11ShaderResourceView* srvs[4] = {
            bloomTexture ? bloomTexture : m_defaultBloomSRV.Get(),
            sceneTexture,
            luminanceTexture ? luminanceTexture : m_defaultLuminanceSRV.Get(),
            maskTexture ? maskTexture : m_defaultMaskSRV.Get()
        };
        m_context->PSSetShaderResources(0, 4, srvs);

        // 设置采样器 (t0: Bloom linear, t1: Scene linear, t2: Luminance point, t3: Mask point)
        ID3D11SamplerState* samplers[4] = {
            m_linearSampler.Get(),
            m_linearSampler.Get(),
            m_pointSampler.Get(),
            m_pointSampler.Get()
        };
        m_context->PSSetSamplers(0, 4, samplers);

        // 设置输入装配 (无需顶点缓冲区，使用 SV_VertexID)
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 保存当前状态
        ID3D11BlendState* savedBlendState = nullptr;
        FLOAT savedBlendFactor[4] = { 0 };
        UINT savedSampleMask = 0;
        m_context->OMGetBlendState(&savedBlendState, savedBlendFactor, &savedSampleMask);

        ID3D11DepthStencilState* savedDepthState = nullptr;
        UINT savedStencilRef = 0;
        m_context->OMGetDepthStencilState(&savedDepthState, &savedStencilRef);

        ID3D11RasterizerState* savedRSState = nullptr;
        m_context->RSGetState(&savedRSState);

        // 设置显式渲染状态 (关键: 使用预创建的状态对象而不是 nullptr)
        static const FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_context->OMSetBlendState(m_noBlendState.Get(), blendFactor, 0xFFFFFFFF);
        m_context->OMSetDepthStencilState(m_noDepthState.Get(), 0);
        m_context->RSSetState(m_rasterizerState.Get());

        // 绘制全屏三角形
        D3DPERF_BeginEvent(0xFF00FF00, L"ScopeHDR_Draw");
        m_context->Draw(3, 0);
        D3DPERF_EndEvent();

        // 恢复状态
        m_context->OMSetBlendState(savedBlendState, savedBlendFactor, savedSampleMask);
        m_context->OMSetDepthStencilState(savedDepthState, savedStencilRef);
        m_context->RSSetState(savedRSState);

        // 释放保存的状态引用
        if (savedBlendState) savedBlendState->Release();
        if (savedDepthState) savedDepthState->Release();
        if (savedRSState) savedRSState->Release();

        // 清理绑定
        ID3D11ShaderResourceView* nullSRVsClear[4] = { nullptr };
        m_context->PSSetShaderResources(0, 4, nullSRVsClear);

        D3DPERF_EndEvent();
    }
}
