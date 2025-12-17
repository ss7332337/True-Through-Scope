#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    // DOF 常量缓冲区结构 - 必须16字节对齐
    // 复现原版游戏的景深混合 pass (Shader hash 681671ef)
    struct alignas(16) DOFConstants
    {
        // 深度参数 (cb2[0])
        float FocalRange;       // 焦点范围 (近平面模糊因子)
        float BlurFalloff;      // 模糊衰减 (远平面模糊因子)
        float FocalPlane;       // 焦平面距离
        float _Pad0;

        // 控制标志 (cb2[1])
        float DOFStrength;      // DOF 强度
        int   EnableNearBlur;   // 近平面模糊开关 (cb2[1].y 作为 bool)
        int   EnableFarBlur;    // 远平面模糊开关 (cb2[1].z 作为 bool)
        int   EnableDOF;        // DOF 总开关 (cb2[1].w 作为 bool)

        // 深度重建参数 (cb2[2])
        float DepthNear;        // 近裁剪面 (用于调试，可能不需要)
        float DepthOffset;      // 深度偏移 (cb2[2].y)
        float DepthScale;       // 深度缩放 (cb2[2].z)
        float DepthFar;         // 远裁剪面 (cb2[2].w)

        // UV 参数 (cb2[3])
        float BlurUVScaleX;     // 模糊纹理 UV 缩放 X (cb2[3].z)
        float BlurUVScaleY;     // 模糊纹理 UV 缩放 Y (cb2[3].w)
        float _Pad1[2];

        DOFConstants()
        {
            // 从 RenderDoc 捕获的原版游戏值 (shader 681671ef)
            // 注意: 原版游戏在该帧可能禁用了 DOF，但我们使用其参数作为参考

            // cb2_v0 = (800.0, 800.0, 724.73669, 10000.0)
            FocalRange = 800.0f;    // 近平面模糊因子
            BlurFalloff = 800.0f;   // 远平面模糊因子
            FocalPlane = 724.73669f;  // 焦平面距离
            _Pad0 = 10000.0f;       // cb2_v0.w (可能是远平面距离)

            // cb2_v1 = (0.5, 1.0, 0.0, 0.0)
            DOFStrength = 0.5f;
            EnableNearBlur = 1;     // cb2_v1.y = 1.0
            EnableFarBlur = 0;      // cb2_v1.z = 0.0 (原版关闭)
            EnableDOF = 1;          // 为瞄具启用 DOF (原版 cb2_v1.w = 0.0)

            // cb2_v2 = (-1e8, 15.0, 353825.0, 5307600.0)
            DepthNear = -1.0e8f;    // 可能是特殊值
            DepthOffset = 15.0f;
            DepthScale = 353825.0f;
            DepthFar = 5307600.0f;

            // cb2_v3 = (1.0, 1.0, 1.0, 1.0)
            BlurUVScaleX = 1.0f;
            BlurUVScaleY = 1.0f;
            _Pad1[0] = _Pad1[1] = 0.0f;
        }
    };

    /**
     * DOF Pass - 复现原版游戏的景深效果
     * 基于深度纹理混合清晰图像和模糊图像
     * (Shader hash 681671ef)
     */
    class DOFPass
    {
    public:
        static DOFPass* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        /**
         * 应用 DOF 效果
         * @param sharpTexture 清晰的场景纹理
         * @param blurredTexture 模糊后的场景纹理 (来自 BloomPass 的模糊输出)
         * @param depthTexture 深度纹理
         * @param outputRTV 输出渲染目标
         * @param width 输出宽度
         * @param height 输出高度
         */
        void Apply(
            ID3D11ShaderResourceView* sharpTexture,
            ID3D11ShaderResourceView* blurredTexture,
            ID3D11ShaderResourceView* depthTexture,
            ID3D11RenderTargetView* outputRTV,
            UINT width,
            UINT height
        );

        // 参数访问
        DOFConstants& GetConstants() { return m_constants; }
        void SetConstants(const DOFConstants& constants) { m_constants = constants; }

        // 单独设置常用参数
        void SetFocalPlane(float focalPlane) { m_constants.FocalPlane = focalPlane; }
        void SetFocalRange(float range) { m_constants.FocalRange = range; }
        void SetBlurFalloff(float falloff) { m_constants.BlurFalloff = falloff; }
        void SetDOFStrength(float strength) { m_constants.DOFStrength = strength; }
        void SetEnabled(bool enabled) { m_constants.EnableDOF = enabled ? 1 : 0; }
        void SetNearBlurEnabled(bool enabled) { m_constants.EnableNearBlur = enabled ? 1 : 0; }
        void SetFarBlurEnabled(bool enabled) { m_constants.EnableFarBlur = enabled ? 1 : 0; }

        // 深度重建参数 (从 RenderDoc 捕获)
        void SetDepthParameters(float offset, float scale, float farPlane)
        {
            m_constants.DepthOffset = offset;
            m_constants.DepthScale = scale;
            m_constants.DepthFar = farPlane;
        }

        bool IsInitialized() const { return m_initialized; }

    private:
        DOFPass() = default;
        ~DOFPass() = default;

        bool CreateShaders();
        bool CreateResources();
        void UpdateConstantBuffer();

        static DOFPass* s_instance;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        // Shaders
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;

        // Resources
        ComPtr<ID3D11Buffer> m_constantBuffer;
        ComPtr<ID3D11SamplerState> m_linearSampler;
        ComPtr<ID3D11SamplerState> m_pointSampler;

        // 渲染状态
        ComPtr<ID3D11BlendState> m_noBlendState;
        ComPtr<ID3D11DepthStencilState> m_noDepthState;
        ComPtr<ID3D11RasterizerState> m_rasterizerState;

        // 常量
        DOFConstants m_constants;

        bool m_initialized = false;
    };
}
