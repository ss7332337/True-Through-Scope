#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace ThroughScope {
    class RenderOptimization {
    public:
        enum class QualityLevel {
            Ultra = 0,      // 1.0x 原始分辨率
            High = 1,       // 0.75x 分辨率
            Medium = 2,     // 0.5x 分辨率
            Low = 3,        // 0.35x 分辨率
            Performance = 4 // 0.25x 分辨率
        };

        struct OptimizationSettings {
            QualityLevel qualityLevel = QualityLevel::Medium;
            float resolutionScale = 0.5f;
            bool skipShadows = true;
            bool skipReflections = true;
            bool skipAO = true;
            bool skipVolumetrics = true;
            bool skipPostProcessing = true;
            bool useDynamicQuality = false;
            bool useTemporalUpsampling = false;
        };

        static RenderOptimization* GetSingleton();

        bool Initialize(ID3D11Device* device);
        void Cleanup();

        // 创建降采样的RenderTarget
        bool CreateDownsampledRenderTargets(UINT originalWidth, UINT originalHeight);

        // 获取当前设置
        const OptimizationSettings& GetSettings() const { return m_settings; }
        void SetQualityLevel(QualityLevel level);

        // 获取缩放后的分辨率
        void GetScaledResolution(UINT originalWidth, UINT originalHeight, UINT& scaledWidth, UINT& scaledHeight) const;

        // 获取降采样的RenderTarget
        ID3D11RenderTargetView* GetDownsampledRTV() const { return m_downsampledRTV.Get(); }
        ID3D11DepthStencilView* GetDownsampledDSV() const { return m_downsampledDSV.Get(); }
        ID3D11ShaderResourceView* GetDownsampledSRV() const { return m_downsampledSRV.Get(); }

        // 上采样到原始分辨率
        void Upsample(ID3D11DeviceContext* context,
                     ID3D11ShaderResourceView* sourceSRV,
                     ID3D11RenderTargetView* destRTV,
                     UINT destWidth, UINT destHeight);

        // 动态质量调整
        void UpdateDynamicQuality(float frameTime);

        // 检查是否应该跳过某个渲染阶段
        bool ShouldSkipShadows() const { return m_settings.skipShadows; }
        bool ShouldSkipReflections() const { return m_settings.skipReflections; }
        bool ShouldSkipAO() const { return m_settings.skipAO; }
        bool ShouldSkipVolumetrics() const { return m_settings.skipVolumetrics; }
        bool ShouldSkipPostProcessing() const { return m_settings.skipPostProcessing; }

    private:
        RenderOptimization() = default;
        ~RenderOptimization() = default;

        bool CreateUpsampleResources();
        void SetQualityPreset(QualityLevel level);

        OptimizationSettings m_settings;

        // 降采样的渲染目标
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_downsampledColorTexture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_downsampledDepthTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_downsampledRTV;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_downsampledDSV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_downsampledSRV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_downsampledDepthSRV;

        // 上采样资源
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_upsamplePS;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_upsampleVS;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearSampler;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_upsampleCB;

        // 动态质量调整
        float m_targetFrameTime = 16.67f; // 60 FPS目标
        float m_qualityAdjustThreshold = 2.0f; // ms
        float m_currentQualityScale = 0.5f;

        ID3D11Device* m_device = nullptr;
        UINT m_currentWidth = 0;
        UINT m_currentHeight = 0;
    };
}