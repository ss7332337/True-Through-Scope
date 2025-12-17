#include "PCH.h"
#include "DOFPass.h"
#include <d3dcompiler.h>
#include <d3d9.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d9.lib")

namespace ThroughScope
{
    DOFPass* DOFPass::s_instance = nullptr;

    DOFPass* DOFPass::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new DOFPass();
        }
        return s_instance;
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
                    logger::error("DOFPass shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
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

    bool DOFPass::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (m_initialized)
            return true;

        m_device = device;
        m_context = context;

        if (!CreateShaders())
        {
            logger::error("DOFPass: Failed to create shaders");
            return false;
        }

        if (!CreateResources())
        {
            logger::error("DOFPass: Failed to create resources");
            return false;
        }

        m_initialized = true;
        logger::info("DOFPass: Initialized successfully");
        return true;
    }

    void DOFPass::Shutdown()
    {
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        m_constantBuffer.Reset();
        m_linearSampler.Reset();
        m_pointSampler.Reset();
        m_noBlendState.Reset();
        m_noDepthState.Reset();
        m_rasterizerState.Reset();
        m_initialized = false;
    }

    bool DOFPass::CreateShaders()
    {
        HRESULT hr;

        // ========== 顶点着色器 (复用 ScopeHDR 的全屏三角形 VS) ==========
        ID3DBlob* vsBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\ScopeHDR_VS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\ScopeHDR_VS.hlsl",
            "main", "vs_5_0", &vsBlob);

        if (FAILED(hr) || !vsBlob)
        {
            logger::error("DOFPass: Failed to load vertex shader");
            return false;
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         nullptr, m_vertexShader.GetAddressOf());
        vsBlob->Release();
        if (FAILED(hr))
        {
            logger::error("DOFPass: Failed to create vertex shader: 0x{:X}", hr);
            return false;
        }

        // ========== DOF 混合像素着色器 ==========
        ID3DBlob* psBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\DOFBlend_PS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\DOFBlend_PS.hlsl",
            "main", "ps_5_0", &psBlob);

        if (FAILED(hr) || !psBlob)
        {
            logger::error("DOFPass: Failed to load DOFBlend pixel shader");
            return false;
        }

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                        nullptr, m_pixelShader.GetAddressOf());
        psBlob->Release();
        if (FAILED(hr))
        {
            logger::error("DOFPass: Failed to create DOFBlend pixel shader: 0x{:X}", hr);
            return false;
        }

        return true;
    }

    bool DOFPass::CreateResources()
    {
        HRESULT hr;

        // ========== 常量缓冲区 ==========
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(DOFConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("DOFPass: Failed to create constant buffer");
            return false;
        }

        // ========== 采样器 ==========

        // 线性采样器 (用于场景纹理)
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
            logger::error("DOFPass: Failed to create linear sampler");
            return false;
        }

        // 点采样器 (用于深度纹理)
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        hr = m_device->CreateSamplerState(&samplerDesc, m_pointSampler.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("DOFPass: Failed to create point sampler");
            return false;
        }

        // ========== 渲染状态 ==========

        // 禁用混合的 Blend State
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&blendDesc, m_noBlendState.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("DOFPass: Failed to create blend state");
            return false;
        }

        // 禁用深度测试的 Depth Stencil State
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = FALSE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        dsDesc.StencilEnable = FALSE;

        hr = m_device->CreateDepthStencilState(&dsDesc, m_noDepthState.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("DOFPass: Failed to create depth stencil state");
            return false;
        }

        // 无剔除的光栅化状态
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
            logger::error("DOFPass: Failed to create rasterizer state");
            return false;
        }

        return true;
    }

    void DOFPass::UpdateConstantBuffer()
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_constants, sizeof(DOFConstants));
            m_context->Unmap(m_constantBuffer.Get(), 0);
        }
    }

    void DOFPass::Apply(
        ID3D11ShaderResourceView* sharpTexture,
        ID3D11ShaderResourceView* blurredTexture,
        ID3D11ShaderResourceView* depthTexture,
        ID3D11RenderTargetView* outputRTV,
        UINT width,
        UINT height)
    {
        D3DPERF_BeginEvent(0xFFFF0088, L"DOFPass::Apply");

        if (!m_initialized)
        {
            logger::warn("DOFPass::Apply: Not initialized");
            D3DPERF_EndEvent();
            return;
        }

        if (!sharpTexture || !blurredTexture || !outputRTV)
        {
            logger::warn("DOFPass::Apply: Invalid parameters - sharp={}, blurred={}, output={}",
                        (void*)sharpTexture, (void*)blurredTexture, (void*)outputRTV);
            D3DPERF_EndEvent();
            return;
        }

        // 如果没有深度纹理，跳过 DOF（但不报错，因为深度纹理可能不可用）
        if (!depthTexture)
        {
            logger::debug("DOFPass::Apply: No depth texture, skipping DOF");
            D3DPERF_EndEvent();
            return;
        }

        // 如果 DOF 被禁用，直接输出清晰图像
        if (!m_constants.EnableDOF)
        {
            // 可以考虑直接 blit，这里简化处理
            D3DPERF_EndEvent();
            return;
        }

        // 更新常量缓冲区
        UpdateConstantBuffer();

        // 清除所有可能冲突的绑定
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        m_context->PSSetShaderResources(0, 8, nullSRVs);

        // 设置渲染目标
        m_context->OMSetRenderTargets(1, &outputRTV, nullptr);

        // 设置 viewport
        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        // 设置 shaders
        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        // 设置常量缓冲区
        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        m_context->PSSetConstantBuffers(0, 1, cbs);

        // 设置纹理
        // t0 = 清晰场景纹理
        // t1 = 模糊场景纹理
        // t2 = 深度纹理
        ID3D11ShaderResourceView* srvs[3] = { sharpTexture, blurredTexture, depthTexture };
        m_context->PSSetShaderResources(0, 3, srvs);

        // 设置采样器
        // s0 = 线性采样器 (场景纹理)
        // s1 = 线性采样器 (模糊纹理)
        // s2 = 点采样器 (深度纹理)
        if (!m_linearSampler || !m_pointSampler)
        {
            logger::error("DOFPass::Apply: Sampler states are null! linear={}, point={}",
                         (void*)m_linearSampler.Get(), (void*)m_pointSampler.Get());
            D3DPERF_EndEvent();
            return;
        }
        ID3D11SamplerState* samplers[3] = { m_linearSampler.Get(), m_linearSampler.Get(), m_pointSampler.Get() };
        m_context->PSSetSamplers(0, 3, samplers);

        // 设置输入装配
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 设置渲染状态
        static const FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_context->OMSetBlendState(m_noBlendState.Get(), blendFactor, 0xFFFFFFFF);
        m_context->OMSetDepthStencilState(m_noDepthState.Get(), 0);
        m_context->RSSetState(m_rasterizerState.Get());

        // 绘制全屏三角形
        m_context->Draw(3, 0);

        // 清理绑定
        m_context->PSSetShaderResources(0, 3, nullSRVs);

        D3DPERF_EndEvent();
    }
}
