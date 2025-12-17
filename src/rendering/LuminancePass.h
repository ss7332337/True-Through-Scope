#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    // Downsample pass 常量缓冲区 - 必须16字节对齐
    struct alignas(16) LuminanceDownsampleConstants
    {
        uint32_t InputSizeX;      // 输入纹理宽度
        uint32_t InputSizeY;      // 输入纹理高度
        uint32_t OutputSizeX;     // 输出纹理宽度 (InputSize / 8)
        uint32_t OutputSizeY;     // 输出纹理高度 (InputSize / 8)

        float MinLuminance;       // 最小亮度阈值 (避免 log(0))
        float CenterWeightScale;  // 中心加权缩放因子
        float _Padding[2];

        LuminanceDownsampleConstants()
        {
            InputSizeX = 1920;
            InputSizeY = 1080;
            OutputSizeX = 240;
            OutputSizeY = 135;
            MinLuminance = 0.0001f;
            CenterWeightScale = 1.5f;  // 中心权重衰减速度
            _Padding[0] = _Padding[1] = 0.0f;
        }
    };

    // Final reduction pass 常量缓冲区
    struct alignas(16) LuminanceFinalConstants
    {
        uint32_t IntermediateSizeX;  // 中间纹理宽度
        uint32_t IntermediateSizeY;  // 中间纹理高度
        float    AdaptationSpeed;    // 时域平滑速度 (e.g., 2.0)
        float    DeltaTime;          // 帧时间

        float    TotalWeight;        // 总权重 (用于加权平均)
        float    MinAdaptedLum;      // 最小适应亮度
        float    MaxAdaptedLum;      // 最大适应亮度
        float    _Padding;

        LuminanceFinalConstants()
        {
            IntermediateSizeX = 240;
            IntermediateSizeY = 135;
            AdaptationSpeed = 2.0f;
            DeltaTime = 0.016f;
            TotalWeight = 1.0f;
            MinAdaptedLum = 0.001f;
            MaxAdaptedLum = 10.0f;
            _Padding = 0.0f;
        }
    };

    /**
     * LuminancePass - 独立计算瞄镜场景平均亮度
     *
     * 使用 Compute Shader 层级化归约:
     * 1. Pass 1: Downsample (8x8 归约) + log-luminance + 中心加权
     * 2. Pass 2: Final reduction (归约到 1x1) + exp() + 时域平滑
     *
     * 输出 1x1 R32_FLOAT 纹理供 ScopeHDR 使用
     */
    class LuminancePass
    {
    public:
        static LuminancePass* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        /**
         * 计算场景平均亮度
         * @param sceneTexture 输入场景纹理 SRV
         * @param width 输入纹理宽度
         * @param height 输入纹理高度
         * @param deltaTime 帧时间 (用于时域平滑)
         * @return 1x1 亮度纹理 SRV (R32_FLOAT)
         */
        ID3D11ShaderResourceView* Compute(
            ID3D11ShaderResourceView* sceneTexture,
            uint32_t width,
            uint32_t height,
            float deltaTime = 0.016f
        );

        // 获取计算结果 SRV
        ID3D11ShaderResourceView* GetLuminanceSRV() const { return m_luminanceSRV.Get(); }

        // 配置参数
        void SetAdaptationSpeed(float speed) { m_finalConstants.AdaptationSpeed = speed; }
        void SetMinLuminance(float minLum) { m_downsampleConstants.MinLuminance = minLum; }
        void SetCenterWeightScale(float scale) { m_downsampleConstants.CenterWeightScale = scale; }
        void SetCenterWeighted(bool enabled) { m_centerWeighted = enabled; }

        float GetAdaptationSpeed() const { return m_finalConstants.AdaptationSpeed; }
        bool IsCenterWeighted() const { return m_centerWeighted; }
        bool IsInitialized() const { return m_initialized; }

    private:
        LuminancePass() = default;
        ~LuminancePass() = default;
        LuminancePass(const LuminancePass&) = delete;
        LuminancePass& operator=(const LuminancePass&) = delete;

        bool CreateShaders();
        bool CreateResources(uint32_t width, uint32_t height);
        void UpdateConstantBuffers(uint32_t width, uint32_t height, float deltaTime);

        static LuminancePass* s_instance;

        ID3D11Device*        m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        // Compute Shaders
        ComPtr<ID3D11ComputeShader> m_downsampleCS;
        ComPtr<ID3D11ComputeShader> m_finalCS;

        // Constant Buffers
        ComPtr<ID3D11Buffer> m_downsampleCB;
        ComPtr<ID3D11Buffer> m_finalCB;

        // 中间纹理 (第一次归约后)
        ComPtr<ID3D11Texture2D>          m_intermediateTexture;
        ComPtr<ID3D11UnorderedAccessView> m_intermediateUAV;
        ComPtr<ID3D11ShaderResourceView>  m_intermediateSRV;
        uint32_t m_intermediateWidth = 0;
        uint32_t m_intermediateHeight = 0;

        // 权重纹理 (用于中心加权的权重和)
        ComPtr<ID3D11Texture2D>          m_weightTexture;
        ComPtr<ID3D11UnorderedAccessView> m_weightUAV;
        ComPtr<ID3D11ShaderResourceView>  m_weightSRV;

        // 最终 1x1 亮度纹理
        ComPtr<ID3D11Texture2D>          m_luminanceTexture;
        ComPtr<ID3D11UnorderedAccessView> m_luminanceUAV;
        ComPtr<ID3D11ShaderResourceView>  m_luminanceSRV;

        // 上一帧亮度 (用于时域平滑)
        ComPtr<ID3D11Texture2D>          m_prevLuminanceTexture;
        ComPtr<ID3D11ShaderResourceView>  m_prevLuminanceSRV;

        // 常量
        LuminanceDownsampleConstants m_downsampleConstants;
        LuminanceFinalConstants m_finalConstants;

        // 缓存的尺寸
        uint32_t m_lastWidth = 0;
        uint32_t m_lastHeight = 0;

        // 设置
        bool m_centerWeighted = true;   // 启用中心加权
        bool m_initialized = false;
    };
}
