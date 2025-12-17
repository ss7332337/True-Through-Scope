#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    // HDR 常量缓冲区结构 - 必须16字节对齐
    struct alignas(16) ScopeHDRConstants
    {
        // HDR 曝光参数
        float MaxExposure;      // 最大曝光值 (默认 15.0)
        float MinExposure;      // 最小曝光值 (默认 0.5)
        float MiddleGray;       // 中间灰度值 (默认 0.18)
        float WhitePoint;       // 白点参数 (默认 0.02)

        // 颜色调整参数
        float Saturation;       // 饱和度 (默认 1.0)
        float _Padding1;
        float BlendIntensity;   // 混合强度 (默认 1.0)
        float Contrast;         // 对比度 (默认 1.0)

        // 色调调整
        float ColorTintR;       // 色调 R (默认 0.0)
        float ColorTintG;       // 色调 G (默认 0.0)
        float ColorTintB;       // 色调 B (默认 0.0)
        float ColorTintW;       // 色调权重 (默认 0.0)

        // UV 缩放和额外参数
        float BloomUVScaleX;    // Bloom UV 缩放 X (默认 1.0)
        float BloomUVScaleY;    // Bloom UV 缩放 Y (默认 1.0)
        float ExposureMultiplier; // 自动曝光乘数 (默认 1.0，用于调整瞄具场景亮度)
        float _Padding3;

        // 控制参数
        float BloomStrength;    // Bloom 强度 (默认 1.0)
        float FixedExposure;    // 固定曝光值 (> 0 则使用固定值，<= 0 使用自动曝光)
        int   SkipHDRTonemapping; // 跳过 HDR tonemapping (默认 0 = false)
        int   _Padding4;        // 对齐填充

        // 默认构造函数
        ScopeHDRConstants()
        {
            // 基于原版 DrawCall 9 的 CBuffer 值
            MaxExposure = 15.0f;
            MinExposure = 0.5f;
            MiddleGray = 0.18f;
            WhitePoint = 0.03f;

            Saturation = 1.2f;    // 增加饱和度补偿褪色
            _Padding1 = 0.0f;
            BlendIntensity = 1.0f;
            Contrast = 1.3f;      // 增加对比度

            ColorTintR = 0.0f;
            ColorTintG = 0.0f;
            ColorTintB = 0.0f;
            ColorTintW = 0.0f;

            BloomUVScaleX = 1.0f;
            BloomUVScaleY = 1.0f;
            ExposureMultiplier = 1.5f;  // 自动曝光乘数（调试：尝试穿透黑色遮罩）
            _Padding3 = 0.0f;

            BloomStrength = 1.0f;
            FixedExposure = 0.0f;  // 使用自动曝光（从 LuminancePass 计算）
            SkipHDRTonemapping = 0; // 启用 HDR tonemapping
            _Padding4 = 0;
        }
    };

    /**
     * 瞄具专用 HDR 效果
     * 使用自定义 shader 实现 Uncharted 2 Filmic Tonemapping
     */
    class ScopeHDR
    {
    public:
        static ScopeHDR* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        /**
         * 应用 HDR 效果
         * @param sceneTexture 输入场景纹理 (瞄具渲染结果)
         * @param bloomTexture Bloom 纹理 (可选，可为 nullptr)
         * @param luminanceTexture 亮度纹理 (可选，如果使用固定曝光则可为 nullptr)
         * @param maskTexture Mask 纹理 (可选)
         * @param outputRTV 输出渲染目标
         */
        void Apply(
            ID3D11ShaderResourceView* sceneTexture,
            ID3D11ShaderResourceView* bloomTexture,
            ID3D11ShaderResourceView* luminanceTexture,
            ID3D11ShaderResourceView* maskTexture,
            ID3D11RenderTargetView* outputRTV
        );

        // 参数访问
        ScopeHDRConstants& GetConstants() { return m_constants; }
        void SetConstants(const ScopeHDRConstants& constants) { m_constants = constants; }

        // 单独设置常用参数
        void SetExposure(float fixedExposure) { m_constants.FixedExposure = fixedExposure; }
        void SetExposureMultiplier(float multiplier) { m_constants.ExposureMultiplier = multiplier; }
        void SetBloomStrength(float strength) { m_constants.BloomStrength = strength; }
        void SetSaturation(float saturation) { m_constants.Saturation = saturation; }
        void SetContrast(float contrast) { m_constants.Contrast = contrast; }
        void SetSkipHDRTonemapping(bool skip) { m_constants.SkipHDRTonemapping = skip ? 1 : 0; }
        void SetColorTint(float r, float g, float b, float weight) {
            m_constants.ColorTintR = r;
            m_constants.ColorTintG = g;
            m_constants.ColorTintB = b;
            m_constants.ColorTintW = weight;
        }

        bool IsInitialized() const { return m_initialized; }

    private:
        ScopeHDR() = default;
        ~ScopeHDR() = default;

        bool CreateShaders();
        bool CreateResources();
        void UpdateConstantBuffer();

        static ScopeHDR* s_instance;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        // Shaders
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;

        // Resources
        ComPtr<ID3D11Buffer> m_constantBuffer;
        ComPtr<ID3D11SamplerState> m_linearSampler;
        ComPtr<ID3D11SamplerState> m_pointSampler;

        // 默认纹理 (当输入为 nullptr 时使用)
        ComPtr<ID3D11Texture2D> m_defaultBloomTexture;
        ComPtr<ID3D11ShaderResourceView> m_defaultBloomSRV;
        ComPtr<ID3D11Texture2D> m_defaultLuminanceTexture;
        ComPtr<ID3D11ShaderResourceView> m_defaultLuminanceSRV;
        ComPtr<ID3D11Texture2D> m_defaultMaskTexture;
        ComPtr<ID3D11ShaderResourceView> m_defaultMaskSRV;

        // 状态
        ScopeHDRConstants m_constants;
        bool m_initialized = false;

        // 渲染状态 - 显式创建以确保正确应用
        ComPtr<ID3D11BlendState> m_noBlendState;          // 禁用混合
        ComPtr<ID3D11DepthStencilState> m_noDepthState;   // 禁用深度测试
        ComPtr<ID3D11RasterizerState> m_rasterizerState;  // 无剔除光栅化
    };
}
