#include "PCH.h"
#include "LUTPass.h"
#include <d3dcompiler.h>
#include <d3d9.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d9.lib")

namespace ThroughScope
{
    LUTPass* LUTPass::s_instance = nullptr;

    LUTPass* LUTPass::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new LUTPass();
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
                    logger::error("LUTPass shader compilation error: {}", (char*)errorBlob->GetBufferPointer());
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

    bool LUTPass::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (m_initialized)
            return true;

        m_device = device;
        m_context = context;

        if (!CreateShaders())
        {
            logger::error("LUTPass: Failed to create shaders");
            return false;
        }

        if (!CreateResources())
        {
            logger::error("LUTPass: Failed to create resources");
            return false;
        }

        m_initialized = true;
        logger::info("LUTPass: Initialized successfully");
        return true;
    }

    void LUTPass::Shutdown()
    {
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        m_constantBuffer.Reset();
        m_linearSampler.Reset();
        m_noBlendState.Reset();
        m_noDepthState.Reset();
        m_rasterizerState.Reset();

        for (int i = 0; i < 4; ++i)
            m_lutTextures[i] = nullptr;

        m_initialized = false;
    }

    bool LUTPass::CreateShaders()
    {
        HRESULT hr;

        // ========== 顶点着色器 (复用全屏三角形 VS) ==========
        ID3DBlob* vsBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\ScopeHDR_VS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\ScopeHDR_VS.hlsl",
            "main", "vs_5_0", &vsBlob);

        if (FAILED(hr) || !vsBlob)
        {
            logger::error("LUTPass: Failed to load vertex shader");
            return false;
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         nullptr, m_vertexShader.GetAddressOf());
        vsBlob->Release();
        if (FAILED(hr))
        {
            logger::error("LUTPass: Failed to create vertex shader: 0x{:X}", hr);
            return false;
        }

        // ========== LUT 像素着色器 ==========
        ID3DBlob* psBlob = nullptr;
        hr = LoadShaderFromFile(
            L"Data\\Shaders\\XiFeiLi\\LUT_PS.cso",
            L"Data\\F4SE\\Plugins\\TTS\\HLSL\\LUT_PS.hlsl",
            "main", "ps_5_0", &psBlob);

        if (FAILED(hr) || !psBlob)
        {
            logger::error("LUTPass: Failed to load LUT pixel shader");
            return false;
        }

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                        nullptr, m_pixelShader.GetAddressOf());
        psBlob->Release();
        if (FAILED(hr))
        {
            logger::error("LUTPass: Failed to create LUT pixel shader: 0x{:X}", hr);
            return false;
        }

        return true;
    }

    bool LUTPass::CreateResources()
    {
        HRESULT hr;

        // ========== 常量缓冲区 ==========
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(LUTConstants);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LUTPass: Failed to create constant buffer");
            return false;
        }

        // ========== 线性采样器 ==========
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
            logger::error("LUTPass: Failed to create linear sampler");
            return false;
        }

        // ========== 渲染状态 ==========

        // 禁用混合
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&blendDesc, m_noBlendState.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LUTPass: Failed to create blend state");
            return false;
        }

        // 禁用深度测试
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = FALSE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        dsDesc.StencilEnable = FALSE;

        hr = m_device->CreateDepthStencilState(&dsDesc, m_noDepthState.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("LUTPass: Failed to create depth stencil state");
            return false;
        }

        // 无剔除光栅化状态
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
            logger::error("LUTPass: Failed to create rasterizer state");
            return false;
        }

        return true;
    }

    void LUTPass::UpdateConstantBuffer()
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &m_constants, sizeof(LUTConstants));
            m_context->Unmap(m_constantBuffer.Get(), 0);
        }
    }

    void LUTPass::SetLUTTextures(
        ID3D11ShaderResourceView* lut0,
        ID3D11ShaderResourceView* lut1,
        ID3D11ShaderResourceView* lut2,
        ID3D11ShaderResourceView* lut3)
    {
        m_lutTextures[0] = lut0;
        m_lutTextures[1] = lut1;
        m_lutTextures[2] = lut2;
        m_lutTextures[3] = lut3;
    }

    void LUTPass::SetLUTWeights(float w0, float w1, float w2, float w3)
    {
        m_constants.LUTWeight0 = w0;
        m_constants.LUTWeight1 = w1;
        m_constants.LUTWeight2 = w2;
        m_constants.LUTWeight3 = w3;
    }

    void LUTPass::Apply(
        ID3D11ShaderResourceView* inputTexture,
        ID3D11RenderTargetView* outputRTV,
        UINT width,
        UINT height)
    {
        D3DPERF_BeginEvent(0xFF00FFFF, L"LUTPass::Apply");

        if (!m_initialized)
        {
            logger::warn("LUTPass::Apply: Not initialized");
            D3DPERF_EndEvent();
            return;
        }

        if (!inputTexture || !outputRTV)
        {
            logger::warn("LUTPass::Apply: Invalid parameters");
            D3DPERF_EndEvent();
            return;
        }

        // 检查是否有至少一个 LUT
        bool hasLUT = false;
        for (int i = 0; i < 4; ++i)
        {
            if (m_lutTextures[i])
            {
                hasLUT = true;
                break;
            }
        }

        if (!hasLUT)
        {
            logger::debug("LUTPass::Apply: No LUT textures, skipping");
            D3DPERF_EndEvent();
            return;
        }

        // 更新常量缓冲区
        UpdateConstantBuffer();

        // 清除绑定
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
        // t0 = 输入纹理 (HDR 输出)
        // t3, t4, t5, t6 = LUT 纹理
        ID3D11ShaderResourceView* srvs[7] = {
            inputTexture,           // t0
            nullptr,                // t1
            nullptr,                // t2
            m_lutTextures[0],       // t3
            m_lutTextures[1],       // t4
            m_lutTextures[2],       // t5
            m_lutTextures[3]        // t6
        };
        m_context->PSSetShaderResources(0, 7, srvs);

        // 设置采样器 (s0, s3, s4, s5, s6)
        if (!m_linearSampler)
        {
            logger::error("LUTPass::Apply: Linear sampler is null!");
            D3DPERF_EndEvent();
            return;
        }
        ID3D11SamplerState* samplers[7] = {
            m_linearSampler.Get(),  // s0
            nullptr,                // s1
            nullptr,                // s2
            m_linearSampler.Get(),  // s3
            m_linearSampler.Get(),  // s4
            m_linearSampler.Get(),  // s5
            m_linearSampler.Get()   // s6
        };
        m_context->PSSetSamplers(0, 7, samplers);

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
        m_context->PSSetShaderResources(0, 7, nullSRVs);

        D3DPERF_EndEvent();
    }
}
