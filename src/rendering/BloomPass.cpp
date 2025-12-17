#include "PCH.h"
#include "BloomPass.h"
#include <d3dcompiler.h>
#include <d3d9.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d9.lib")

namespace ThroughScope
{
    BloomPass* BloomPass::s_instance = nullptr;

    BloomPass* BloomPass::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new BloomPass();
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
                    logger::error("BloomPass shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
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

    bool BloomPass::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (m_initialized)
            return true;

        m_device = device;
        m_context = context;

        if (!CreateShaders())
        {
            logger::error("BloomPass: Failed to create shaders");
            return false;
        }

        if (!CreateResources())
        {
            logger::error("BloomPass: Failed to create resources");
            return false;
        }

        m_initialized = true;
        logger::info("BloomPass: Initialized successfully");
        return true;
    }

    void BloomPass::Shutdown()
    {
        m_vertexShader.Reset();
        m_accumulatePS.Reset();
        m_gaussianBlurPS.Reset();
        m_accumulateCB.Reset();
        m_blurCB.Reset();
        m_linearSampler.Reset();
        m_noBlendState.Reset();
        m_noDepthState.Reset();
        m_rasterizerState.Reset();
        m_initialized = false;
    }

    bool BloomPass::CreateShaders()
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
            logger::error("BloomPass: Failed to load vertex shader");
            return false;
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         nullptr, m_vertexShader.GetAddressOf());
        vsBlob->Release();
        if (FAILED(hr))
        {
            logger::error("BloomPass: Failed to create vertex shader: 0x{:X}", hr);
            return false;
        }

        // ========== Bloom 累加像素着色器 ==========
        ID3DBlob* accumulatePSBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\BloomAccumulate_PS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\BloomAccumulate_PS.hlsl",
            "main", "ps_5_0", &accumulatePSBlob);

        if (FAILED(hr) || !accumulatePSBlob)
        {
            logger::error("BloomPass: Failed to load BloomAccumulate pixel shader");
            return false;
        }

        hr = m_device->CreatePixelShader(accumulatePSBlob->GetBufferPointer(), accumulatePSBlob->GetBufferSize(),
                                        nullptr, m_accumulatePS.GetAddressOf());
        accumulatePSBlob->Release();
        if (FAILED(hr))
        {
            logger::error("BloomPass: Failed to create BloomAccumulate pixel shader: 0x{:X}", hr);
            return false;
        }

        // ========== 高斯模糊像素着色器 ==========
        ID3DBlob* blurPSBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\GaussianBlur_PS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\GaussianBlur_PS.hlsl",
            "main", "ps_5_0", &blurPSBlob);

        if (FAILED(hr) || !blurPSBlob)
        {
            logger::error("BloomPass: Failed to load GaussianBlur pixel shader");
            return false;
        }

        hr = m_device->CreatePixelShader(blurPSBlob->GetBufferPointer(), blurPSBlob->GetBufferSize(),
                                        nullptr, m_gaussianBlurPS.GetAddressOf());
        blurPSBlob->Release();
        if (FAILED(hr))
        {
            logger::error("BloomPass: Failed to create GaussianBlur pixel shader: 0x{:X}", hr);
            return false;
        }

        return true;
    }

    bool BloomPass::CreateResources()
    {
        HRESULT hr;

        // ========== Bloom 累加常量缓冲区 ==========
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(BloomAccumulateConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_accumulateCB.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("BloomPass: Failed to create accumulate constant buffer");
            return false;
        }

        // ========== 高斯模糊常量缓冲区 ==========
        cbDesc.ByteWidth = sizeof(GaussianBlurConstants);
        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_blurCB.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("BloomPass: Failed to create blur constant buffer");
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
            logger::error("BloomPass: Failed to create linear sampler");
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
            logger::error("BloomPass: Failed to create blend state");
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
            logger::error("BloomPass: Failed to create depth stencil state");
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
            logger::error("BloomPass: Failed to create rasterizer state");
            return false;
        }

        return true;
    }

    void BloomPass::UpdateAccumulateConstantBuffer()
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_accumulateCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_accumulateConstants, sizeof(BloomAccumulateConstants));
            m_context->Unmap(m_accumulateCB.Get(), 0);
        }
    }

    void BloomPass::UpdateBlurConstantBuffer()
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_blurCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_blurConstants, sizeof(GaussianBlurConstants));
            m_context->Unmap(m_blurCB.Get(), 0);
        }
    }

    void BloomPass::SetBloomOffsetWeight(int index, float offsetX, float offsetY, float weight)
    {
        if (index >= 0 && index < 4)
        {
            m_accumulateConstants.BloomOffsetWeight[index] = { offsetX, offsetY, weight, 0.0f };
        }
    }

    void BloomPass::SetBlurOffsetWeight(int index, float offsetX, float offsetY, float weight)
    {
        if (index >= 0 && index < 7)
        {
            m_blurConstants.BlurOffsetWeight[index] = { offsetX, offsetY, weight, 0.0f };
        }
    }

    void BloomPass::ApplyAccumulate(
        ID3D11ShaderResourceView* const* bloomMipTextures,
        int numMips,
        ID3D11RenderTargetView* outputRTV,
        UINT width,
        UINT height)
    {
        D3DPERF_BeginEvent(0xFFFF8800, L"BloomPass::ApplyAccumulate");

        if (!m_initialized || !bloomMipTextures || !outputRTV)
        {
            D3DPERF_EndEvent();
            return;
        }

        // 更新常量
        m_accumulateConstants.TargetSizeX = 1.0f / static_cast<float>(width);
        m_accumulateConstants.TargetSizeY = 1.0f / static_cast<float>(height);
        UpdateAccumulateConstantBuffer();

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
        m_context->PSSetShader(m_accumulatePS.Get(), nullptr, 0);

        // 设置常量缓冲区
        ID3D11Buffer* cbs[] = { m_accumulateCB.Get() };
        m_context->PSSetConstantBuffers(0, 1, cbs);

        // 设置 Bloom 纹理 (最多4个 mip 级别)
        ID3D11ShaderResourceView* srvs[4] = {};
        for (int i = 0; i < 4 && i < numMips; ++i)
        {
            srvs[i] = bloomMipTextures[i];
        }
        m_context->PSSetShaderResources(0, 4, srvs);

        // 设置采样器 (with null check)
        if (!m_linearSampler)
        {
            logger::error("BloomPass::ApplyAccumulate: Linear sampler is null!");
            D3DPERF_EndEvent();
            return;
        }
        ID3D11SamplerState* samplers[] = { m_linearSampler.Get() };
        m_context->PSSetSamplers(0, 1, samplers);

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
        m_context->PSSetShaderResources(0, 4, nullSRVs);

        D3DPERF_EndEvent();
    }

    void BloomPass::ApplyGaussianBlur(
        ID3D11ShaderResourceView* inputTexture,
        ID3D11RenderTargetView* outputRTV,
        bool horizontal,
        UINT width,
        UINT height)
    {
        D3DPERF_BeginEvent(0xFFFF8800, horizontal ? L"BloomPass::GaussianBlurH" : L"BloomPass::GaussianBlurV");

        if (!m_initialized || !inputTexture || !outputRTV)
        {
            D3DPERF_EndEvent();
            return;
        }

        // 更新模糊方向
        float texelSizeX = 1.0f / static_cast<float>(width);
        float texelSizeY = 1.0f / static_cast<float>(height);
        if (horizontal)
        {
            m_blurConstants.SetHorizontal(texelSizeX);
        }
        else
        {
            m_blurConstants.SetVertical(texelSizeY);
        }
        UpdateBlurConstantBuffer();

        // 清除绑定
        ID3D11ShaderResourceView* nullSRVs[4] = {};
        m_context->PSSetShaderResources(0, 4, nullSRVs);

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
        m_context->PSSetShader(m_gaussianBlurPS.Get(), nullptr, 0);

        // 设置常量缓冲区
        ID3D11Buffer* cbs[] = { m_blurCB.Get() };
        m_context->PSSetConstantBuffers(0, 1, cbs);

        // 设置输入纹理
        m_context->PSSetShaderResources(0, 1, &inputTexture);

        // 设置采样器 (with null check)
        if (!m_linearSampler)
        {
            logger::error("BloomPass::ApplyGaussianBlur: Linear sampler is null!");
            D3DPERF_EndEvent();
            return;
        }
        ID3D11SamplerState* samplers[] = { m_linearSampler.Get() };
        m_context->PSSetSamplers(0, 1, samplers);

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
        m_context->PSSetShaderResources(0, 1, nullSRVs);

        D3DPERF_EndEvent();
    }

    void BloomPass::ApplyFullBloom(
        ID3D11ShaderResourceView* inputTexture,
        ID3D11ShaderResourceView* bloomTexture,
        ID3D11RenderTargetView* outputRTV,
        ID3D11RenderTargetView* tempRTV,
        ID3D11ShaderResourceView* tempSRV,
        UINT width,
        UINT height)
    {
        D3DPERF_BeginEvent(0xFFFF8800, L"BloomPass::ApplyFullBloom");

        if (!m_initialized || !inputTexture || !bloomTexture || !outputRTV || !tempRTV || !tempSRV)
        {
            logger::warn("BloomPass::ApplyFullBloom: Invalid parameters");
            D3DPERF_EndEvent();
            return;
        }

        // Pass 1: Bloom 累加 (使用单个 bloom 纹理)
        ID3D11ShaderResourceView* bloomMips[4] = { bloomTexture, nullptr, nullptr, nullptr };
        ApplyAccumulate(bloomMips, 1, tempRTV, width, height);

        // Pass 2: 水平高斯模糊
        ApplyGaussianBlur(tempSRV, outputRTV, true, width, height);

        // Pass 3: 垂直高斯模糊 (输出到临时纹理，然后再复制回来或者直接输出)
        // 注意: 这里需要另一个临时纹理，或者修改流程
        // 简化版本: 直接在 output 上做垂直模糊
        // 实际使用时需要提供两个临时纹理进行 ping-pong

        D3DPERF_EndEvent();
    }
}
