#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>

#include "BloomPass.h"
#include "DOFPass.h"
#include "ScopeHDR.h"
#include "LUTPass.h"

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    // 后处理配置
    struct PostProcessConfig
    {
        // Bloom 配置
        bool bloomEnabled = true;
        float bloomIntensity = 1.0f;

        // DOF 配置
        bool dofEnabled = true;
        float dofStrength = 0.5f;
        float focalPlane = 10.0f;
        float focalRange = 5.0f;
        bool nearBlurEnabled = true;
        bool farBlurEnabled = true;

        // HDR 配置 (使用 ScopeHDR 的默认值)
        bool hdrEnabled = true;

        // LUT 配置 (Color Grading)
        bool lutEnabled = true;

        PostProcessConfig() = default;
    };

    /**
     * 瞄具后处理管线管理器
     * 整合所有后处理 pass:
     * 1. BloomPass (4-tap累加 + 7-tap高斯模糊)
     * 2. ScopeHDR (Tonemapping + 饱和度/对比度)
     * 3. DOFPass (基于深度的景深混合) - 暂时禁用
     * 4. LUTPass (Color Grading LUT)
     */
    class ScopePostProcess
    {
    public:
        static ScopePostProcess* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        /**
         * 执行完整的后处理流水线
         * @param sceneTexture 瞄具渲染结果
         * @param bloomTexture Bloom 纹理 (从引擎获取)
         * @param depthTexture 深度纹理
         * @param luminanceTexture 亮度纹理 (用于自动曝光)
         * @param maskTexture Mask 纹理 (可选)
         * @param outputRTV 最终输出目标
         * @param width 输出宽度
         * @param height 输出高度
         * @param config 后处理配置
         */
        void Apply(
            ID3D11ShaderResourceView* sceneTexture,
            ID3D11ShaderResourceView* bloomTexture,
            ID3D11ShaderResourceView* depthTexture,
            ID3D11ShaderResourceView* luminanceTexture,
            ID3D11ShaderResourceView* maskTexture,
            ID3D11RenderTargetView* outputRTV,
            UINT width,
            UINT height,
            const PostProcessConfig& config = PostProcessConfig()
        );

        /**
         * 简化版本: 只需要场景纹理和 Bloom 纹理
         * DOF 使用默认深度纹理 (如果有)
         */
        void ApplySimple(
            ID3D11ShaderResourceView* sceneTexture,
            ID3D11ShaderResourceView* bloomTexture,
            ID3D11RenderTargetView* outputRTV,
            UINT width,
            UINT height
        );

        // 子系统访问
        BloomPass* GetBloomPass() { return m_bloomPass; }
        DOFPass* GetDOFPass() { return m_dofPass; }
        ScopeHDR* GetHDRPass() { return m_hdrPass; }
        LUTPass* GetLUTPass() { return m_lutPass; }

        // 配置
        PostProcessConfig& GetConfig() { return m_config; }
        void SetConfig(const PostProcessConfig& config) { m_config = config; }

        // 启用/禁用各个 pass
        void SetBloomEnabled(bool enabled) { m_config.bloomEnabled = enabled; }
        void SetDOFEnabled(bool enabled) { m_config.dofEnabled = enabled; }
        void SetHDREnabled(bool enabled) { m_config.hdrEnabled = enabled; }
        void SetLUTEnabled(bool enabled) { m_config.lutEnabled = enabled; }

        // LUT 纹理设置 (传递给 ScopeHDR)
        void SetLUTTextures(
            ID3D11ShaderResourceView* lut0,
            ID3D11ShaderResourceView* lut1,
            ID3D11ShaderResourceView* lut2,
            ID3D11ShaderResourceView* lut3);

        bool IsInitialized() const { return m_initialized; }

    private:
        ScopePostProcess() = default;
        ~ScopePostProcess() = default;

        bool CreateTempTextures(UINT width, UINT height);
        void ReleaseTempTextures();

        static ScopePostProcess* s_instance;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        // 子系统 (使用单例)
        BloomPass* m_bloomPass = nullptr;
        DOFPass* m_dofPass = nullptr;
        ScopeHDR* m_hdrPass = nullptr;
        LUTPass* m_lutPass = nullptr;

        // 中间纹理 (用于 ping-pong 渲染)
        ComPtr<ID3D11Texture2D> m_tempTexture0;
        ComPtr<ID3D11ShaderResourceView> m_tempSRV0;
        ComPtr<ID3D11RenderTargetView> m_tempRTV0;

        ComPtr<ID3D11Texture2D> m_tempTexture1;
        ComPtr<ID3D11ShaderResourceView> m_tempSRV1;
        ComPtr<ID3D11RenderTargetView> m_tempRTV1;

        // 当前临时纹理尺寸
        UINT m_tempWidth = 0;
        UINT m_tempHeight = 0;

        // 配置
        PostProcessConfig m_config;

        bool m_initialized = false;
    };
}
