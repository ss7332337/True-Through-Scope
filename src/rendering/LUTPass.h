#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    // LUT Pass 常量缓冲区 - 必须16字节对齐
    // 复现原版游戏的 LUT Color Grading (Shader hash 7f725af5)
    struct alignas(16) LUTConstants
    {
        // cb2[1].xyzw = LUT 混合权重
        float LUTWeight0;   // t3 LUT 权重
        float LUTWeight1;   // t4 LUT 权重
        float LUTWeight2;   // t5 LUT 权重
        float LUTWeight3;   // t6 LUT 权重

        LUTConstants()
        {
            // 默认只使用第一个 LUT
            LUTWeight0 = 1.0f;
            LUTWeight1 = 0.0f;
            LUTWeight2 = 0.0f;
            LUTWeight3 = 0.0f;
        }
    };

    /**
     * LUT Pass - 复现原版游戏的 Color Grading
     * 使用 4 个 3D LUT 纹理进行颜色分级
     * (Shader hash 7f725af5)
     *
     * 流程:
     * 1. 采样输入纹理
     * 2. 应用 gamma 校正 (pow 1/2.2)
     * 3. 缩放到 LUT 范围 (16x16x16 LUT)
     * 4. 采样并混合 4 个 LUT
     * 5. 输出最终颜色
     */
    class LUTPass
    {
    public:
        static LUTPass* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        /**
         * 应用 LUT Color Grading
         * @param inputTexture HDR pass 的输出
         * @param outputRTV 最终输出目标
         * @param width 输出宽度
         * @param height 输出高度
         */
        void Apply(
            ID3D11ShaderResourceView* inputTexture,
            ID3D11RenderTargetView* outputRTV,
            UINT width,
            UINT height
        );

        // 设置 LUT 纹理 (t3, t4, t5, t6)
        void SetLUTTextures(
            ID3D11ShaderResourceView* lut0,
            ID3D11ShaderResourceView* lut1,
            ID3D11ShaderResourceView* lut2,
            ID3D11ShaderResourceView* lut3
        );

        // 设置 LUT 混合权重
        void SetLUTWeights(float w0, float w1, float w2, float w3);

        LUTConstants& GetConstants() { return m_constants; }
        bool IsInitialized() const { return m_initialized; }
        bool HasLUTTextures() const {
            return m_lutTextures[0] || m_lutTextures[1] || m_lutTextures[2] || m_lutTextures[3];
        }

    private:
        LUTPass() = default;
        ~LUTPass() = default;

        bool CreateShaders();
        bool CreateResources();
        void UpdateConstantBuffer();

        static LUTPass* s_instance;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        // Shaders
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;

        // Resources
        ComPtr<ID3D11Buffer> m_constantBuffer;
        ComPtr<ID3D11SamplerState> m_linearSampler;

        // 渲染状态
        ComPtr<ID3D11BlendState> m_noBlendState;
        ComPtr<ID3D11DepthStencilState> m_noDepthState;
        ComPtr<ID3D11RasterizerState> m_rasterizerState;

        // LUT 纹理 (从引擎获取)
        ID3D11ShaderResourceView* m_lutTextures[4] = { nullptr };

        // 常量
        LUTConstants m_constants;

        bool m_initialized = false;
    };
}
