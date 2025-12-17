#include "PCH.h"
#include "ScopePostProcess.h"
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

namespace ThroughScope
{
    ScopePostProcess* ScopePostProcess::s_instance = nullptr;

    ScopePostProcess* ScopePostProcess::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new ScopePostProcess();
        }
        return s_instance;
    }

    bool ScopePostProcess::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (m_initialized)
            return true;

        m_device = device;
        m_context = context;

        // 初始化子系统
        m_bloomPass = BloomPass::GetSingleton();
        if (!m_bloomPass->Initialize(device, context))
        {
            logger::error("ScopePostProcess: Failed to initialize BloomPass");
            return false;
        }

        m_dofPass = DOFPass::GetSingleton();
        if (!m_dofPass->Initialize(device, context))
        {
            logger::error("ScopePostProcess: Failed to initialize DOFPass");
            return false;
        }

        m_hdrPass = ScopeHDR::GetSingleton();
        if (!m_hdrPass->Initialize(device, context))
        {
            logger::error("ScopePostProcess: Failed to initialize ScopeHDR");
            return false;
        }

        m_lutPass = LUTPass::GetSingleton();
        if (!m_lutPass->Initialize(device, context))
        {
            logger::error("ScopePostProcess: Failed to initialize LUTPass");
            return false;
        }

        m_initialized = true;
        logger::info("ScopePostProcess: Initialized successfully");
        return true;
    }

    void ScopePostProcess::Shutdown()
    {
        ReleaseTempTextures();

        if (m_bloomPass)
            m_bloomPass->Shutdown();
        if (m_dofPass)
            m_dofPass->Shutdown();
        if (m_hdrPass)
            m_hdrPass->Shutdown();
        if (m_lutPass)
            m_lutPass->Shutdown();

        m_bloomPass = nullptr;
        m_dofPass = nullptr;
        m_hdrPass = nullptr;
        m_lutPass = nullptr;

        m_initialized = false;
    }

    bool ScopePostProcess::CreateTempTextures(UINT width, UINT height)
    {
        // 如果尺寸相同，不需要重新创建
        if (m_tempWidth == width && m_tempHeight == height &&
            m_tempTexture0 && m_tempTexture1)
        {
            return true;
        }

        // 释放旧的纹理
        ReleaseTempTextures();

        HRESULT hr;

        // 创建临时纹理 0
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;  // HDR 格式
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        hr = m_device->CreateTexture2D(&texDesc, nullptr, m_tempTexture0.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopePostProcess: Failed to create temp texture 0: 0x{:X}", hr);
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_tempTexture0.Get(), nullptr, m_tempSRV0.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopePostProcess: Failed to create temp SRV 0: 0x{:X}", hr);
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_tempTexture0.Get(), nullptr, m_tempRTV0.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopePostProcess: Failed to create temp RTV 0: 0x{:X}", hr);
            return false;
        }

        // 创建临时纹理 1
        hr = m_device->CreateTexture2D(&texDesc, nullptr, m_tempTexture1.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopePostProcess: Failed to create temp texture 1: 0x{:X}", hr);
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_tempTexture1.Get(), nullptr, m_tempSRV1.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopePostProcess: Failed to create temp SRV 1: 0x{:X}", hr);
            return false;
        }

        hr = m_device->CreateRenderTargetView(m_tempTexture1.Get(), nullptr, m_tempRTV1.GetAddressOf());
        if (FAILED(hr))
        {
            logger::error("ScopePostProcess: Failed to create temp RTV 1: 0x{:X}", hr);
            return false;
        }

        m_tempWidth = width;
        m_tempHeight = height;

        logger::debug("ScopePostProcess: Created temp textures {}x{}", width, height);
        return true;
    }

    void ScopePostProcess::ReleaseTempTextures()
    {
        m_tempRTV0.Reset();
        m_tempSRV0.Reset();
        m_tempTexture0.Reset();
        m_tempRTV1.Reset();
        m_tempSRV1.Reset();
        m_tempTexture1.Reset();
        m_tempWidth = 0;
        m_tempHeight = 0;
    }

    void ScopePostProcess::SetLUTTextures(
        ID3D11ShaderResourceView* lut0,
        ID3D11ShaderResourceView* lut1,
        ID3D11ShaderResourceView* lut2,
        ID3D11ShaderResourceView* lut3)
    {
        // LUT 纹理现在由 LUTPass 处理
        if (m_lutPass)
        {
            m_lutPass->SetLUTTextures(lut0, lut1, lut2, lut3);
        }
    }

    void ScopePostProcess::Apply(
        ID3D11ShaderResourceView* sceneTexture,
        ID3D11ShaderResourceView* bloomTexture,
        ID3D11ShaderResourceView* depthTexture,
        ID3D11ShaderResourceView* luminanceTexture,
        ID3D11ShaderResourceView* maskTexture,
        ID3D11RenderTargetView* outputRTV,
        UINT width,
        UINT height,
        const PostProcessConfig& config)
    {
        D3DPERF_BeginEvent(0xFFFFFF00, L"ScopePostProcess::Apply");

        if (!m_initialized)
        {
            logger::warn("ScopePostProcess::Apply: Not initialized");
            D3DPERF_EndEvent();
            return;
        }

        if (!sceneTexture || !outputRTV)
        {
            logger::warn("ScopePostProcess::Apply: Invalid parameters");
            D3DPERF_EndEvent();
            return;
        }

        // 确保临时纹理存在
        if (!CreateTempTextures(width, height))
        {
            logger::error("ScopePostProcess::Apply: Failed to create temp textures");
            D3DPERF_EndEvent();
            return;
        }

        // ========== 纹理追踪 ==========
        // sceneInput: 当前场景纹理 (初始为输入场景，可能被 DOF 修改)
        // processedBloom: 处理后的 bloom 纹理 (经过累加和模糊)
        ID3D11ShaderResourceView* sceneInput = sceneTexture;
        ID3D11ShaderResourceView* processedBloom = bloomTexture;  // 默认使用原始 bloom
        int pingPongIndex = 0;

        // ========== Pass 1: Bloom 累加 + 模糊 (处理 bloom 纹理，不影响场景) ==========
        if (config.bloomEnabled && bloomTexture && m_bloomPass->IsInitialized())
        {
            D3DPERF_BeginEvent(0xFFFF8800, L"ScopePostProcess::BloomPasses");

            // 使用 temp0 作为 bloom 处理的输出
            ID3D11RenderTargetView* bloomOutput = m_tempRTV0.Get();
            ID3D11ShaderResourceView* bloomOutputSRV = m_tempSRV0.Get();

            // Bloom 累加
            // 注意：原版游戏使用 4 级 mip，但我们只有 1 个 bloom 纹理
            // 为了保持正确的亮度，所有 4 个槽都使用同一个纹理 (0.25 * 4 = 1.0)
            ID3D11ShaderResourceView* bloomMips[4] = { bloomTexture, bloomTexture, bloomTexture, bloomTexture };
            m_bloomPass->ApplyAccumulate(bloomMips, 4, bloomOutput, width, height);

            // 水平高斯模糊 (temp0 -> temp1)
            m_bloomPass->ApplyGaussianBlur(bloomOutputSRV, m_tempRTV1.Get(), true, width, height);

            // 垂直高斯模糊 (temp1 -> temp0)
            m_bloomPass->ApplyGaussianBlur(m_tempSRV1.Get(), m_tempRTV0.Get(), false, width, height);

            // 处理后的 bloom 在 temp0
            processedBloom = m_tempSRV0.Get();
            pingPongIndex = 0;  // temp0 被 bloom 占用

            D3DPERF_EndEvent();
        }

        // ========== Pass 2: DOF 景深 (暂时禁用，需要场景模糊纹理) ==========
        // 注意: DOF 需要模糊的场景纹理，而不是 bloom 纹理
        // 目前先跳过 DOF，因为我们没有单独的场景模糊 pass
        // TODO: 添加场景模糊 pass 用于 DOF
        if (false && config.dofEnabled && depthTexture && m_dofPass->IsInitialized())
        {
            D3DPERF_BeginEvent(0xFFFF0088, L"ScopePostProcess::DOFPass");

            // 配置 DOF 参数
            m_dofPass->SetDOFStrength(config.dofStrength);
            m_dofPass->SetFocalPlane(config.focalPlane);
            m_dofPass->SetFocalRange(config.focalRange);
            m_dofPass->SetNearBlurEnabled(config.nearBlurEnabled);
            m_dofPass->SetFarBlurEnabled(config.farBlurEnabled);
            m_dofPass->SetEnabled(true);

            // 选择未被 bloom 占用的 temp 纹理
            ID3D11RenderTargetView* dofOutput = (pingPongIndex == 0) ? m_tempRTV1.Get() : m_tempRTV0.Get();
            ID3D11ShaderResourceView* dofOutputSRV = (pingPongIndex == 0) ? m_tempSRV1.Get() : m_tempSRV0.Get();

            // 应用 DOF (需要场景模糊纹理，目前暂时使用原始场景)
            m_dofPass->Apply(sceneTexture, sceneTexture, depthTexture, dofOutput, width, height);

            sceneInput = dofOutputSRV;

            D3DPERF_EndEvent();
        }

        // ========== Pass 3: HDR Tonemapping ==========
        // HDR 输出到临时纹理（如果 LUT 启用），否则直接输出
        ID3D11ShaderResourceView* hdrOutput = sceneInput;  // 默认使用场景

        if (config.hdrEnabled && m_hdrPass->IsInitialized())
        {
            D3DPERF_BeginEvent(0xFF00FF00, L"ScopePostProcess::HDRPass");

            // 决定 HDR 输出目标
            // 如果 LUT 启用且有 LUT 纹理，输出到 temp 纹理；否则直接输出到最终目标
            bool lutWillRun = config.lutEnabled && m_lutPass && m_lutPass->IsInitialized() && m_lutPass->HasLUTTextures();
            ID3D11RenderTargetView* hdrOutputRTV = lutWillRun ? m_tempRTV1.Get() : outputRTV;

            m_hdrPass->Apply(
                sceneInput,       // 场景纹理
                processedBloom,   // 处理后的 bloom 纹理
                luminanceTexture,
                maskTexture,
                hdrOutputRTV
            );

            // 如果 LUT 将运行，更新 hdrOutput
            if (lutWillRun)
            {
                hdrOutput = m_tempSRV1.Get();
            }

            D3DPERF_EndEvent();
        }
        else
        {
            logger::debug("ScopePostProcess: HDR disabled");
        }

        // ========== Pass 4: LUT Color Grading ==========
        if (config.lutEnabled && m_lutPass && m_lutPass->IsInitialized())
        {
            D3DPERF_BeginEvent(0xFF00FFFF, L"ScopePostProcess::LUTPass");

            m_lutPass->Apply(
                hdrOutput,    // HDR 输出 (或原始场景)
                outputRTV,    // 最终输出
                width,
                height
            );

            D3DPERF_EndEvent();
        }
        else if (!config.hdrEnabled)
        {
            // 如果 HDR 和 LUT 都禁用，需要直接复制到输出
            logger::warn("ScopePostProcess: Both HDR and LUT disabled, output may be incorrect");
        }

        D3DPERF_EndEvent();
    }

    void ScopePostProcess::ApplySimple(
        ID3D11ShaderResourceView* sceneTexture,
        ID3D11ShaderResourceView* bloomTexture,
        ID3D11RenderTargetView* outputRTV,
        UINT width,
        UINT height)
    {
        // 使用默认配置，禁用 DOF (因为没有深度纹理)
        PostProcessConfig config = m_config;
        config.dofEnabled = false;

        Apply(sceneTexture, bloomTexture, nullptr, nullptr, nullptr, outputRTV, width, height, config);
    }
}
