#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    // Bloom 常量缓冲区结构 - 必须16字节对齐
    // 复现原版游戏的 Bloom 累加 + 高斯模糊 pass
    struct alignas(16) BloomAccumulateConstants
    {
        // Bloom 累加偏移和权重 (cb2[8-11])
        // xy = UV偏移, z = 权重, w = unused
        DirectX::XMFLOAT4 BloomOffsetWeight[4];

        // 全局参数 (cb2[4], cb2[7])
        float BloomUVScaleX;    // UV缩放 X
        float BloomUVScaleY;    // UV缩放 Y
        float TargetSizeX;      // 目标尺寸倒数 X (1/width)
        float TargetSizeY;      // 目标尺寸倒数 Y (1/height)

        float BloomMultiplier;  // 输出 alpha (cb2[7].z)
        float _Padding[3];

        BloomAccumulateConstants()
        {
            // 从 RenderDoc 捕获的原版游戏值 (shader a8c2404f)
            // cb2_v8-v11: xy=UV偏移, z=权重
            BloomOffsetWeight[0] = { -1.0f, -1.0f, 0.25f, 0.0f };  // cb2_v8
            BloomOffsetWeight[1] = {  1.0f, -1.0f, 0.25f, 0.0f };  // cb2_v9
            BloomOffsetWeight[2] = {  1.0f,  1.0f, 0.25f, 0.0f };  // cb2_v10
            BloomOffsetWeight[3] = { -1.0f,  1.0f, 0.25f, 0.0f };  // cb2_v11

            // cb2_v4.xy = BloomUVScale
            BloomUVScaleX = 1.0f;
            BloomUVScaleY = 1.0f;
            // cb2_v7.xy = TargetSize (约 1/1920, 1/1080)
            TargetSizeX = 0.00052f;  // ~1/1920
            TargetSizeY = 0.00093f;  // ~1/1080

            BloomMultiplier = 1.0f;
            _Padding[0] = _Padding[1] = _Padding[2] = 0.0f;
        }
    };

    // 高斯模糊常量缓冲区结构
    struct alignas(16) GaussianBlurConstants
    {
        // 模糊偏移和权重 (cb2[2-8])
        // xy = UV偏移, z = 权重, w = unused
        DirectX::XMFLOAT4 BlurOffsetWeight[7];

        float _Padding[4];

        GaussianBlurConstants()
        {
            // 从 RenderDoc 捕获的原版游戏值 (shader bfa8841e)
            // 这是垂直模糊的值，水平模糊需要交换 xy
            // cb2_v2-v8: xy=UV偏移, z=权重
            BlurOffsetWeight[0] = { 0.0f, -0.01111f, 0.03663f, 0.0f };  // cb2_v2
            BlurOffsetWeight[1] = { 0.0f, -0.00741f, 0.11128f, 0.0f };  // cb2_v3
            BlurOffsetWeight[2] = { 0.0f, -0.00370f, 0.21675f, 0.0f };  // cb2_v4
            BlurOffsetWeight[3] = { 0.0f,  0.0f,     0.27068f, 0.0f };  // cb2_v5 (center)
            BlurOffsetWeight[4] = { 0.0f,  0.00370f, 0.21675f, 0.0f };  // cb2_v6
            BlurOffsetWeight[5] = { 0.0f,  0.00741f, 0.11128f, 0.0f };  // cb2_v7
            BlurOffsetWeight[6] = { 0.0f,  0.01111f, 0.03663f, 0.0f };  // cb2_v8

            _Padding[0] = _Padding[1] = _Padding[2] = _Padding[3] = 0.0f;
        }

        // 设置为水平模糊 (使用捕获的权重)
        void SetHorizontal(float texelSizeX)
        {
            // 捕获的权重: 0.03663, 0.11128, 0.21675, 0.27068, 0.21675, 0.11128, 0.03663
            float weights[7] = { 0.03663f, 0.11128f, 0.21675f, 0.27068f, 0.21675f, 0.11128f, 0.03663f };
            float offsets[7] = { -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
            for (int i = 0; i < 7; ++i)
            {
                BlurOffsetWeight[i] = { offsets[i] * texelSizeX, 0.0f, weights[i], 0.0f };
            }
        }

        // 设置为垂直模糊 (使用捕获的权重)
        void SetVertical(float texelSizeY)
        {
            // 捕获的权重: 0.03663, 0.11128, 0.21675, 0.27068, 0.21675, 0.11128, 0.03663
            float weights[7] = { 0.03663f, 0.11128f, 0.21675f, 0.27068f, 0.21675f, 0.11128f, 0.03663f };
            float offsets[7] = { -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
            for (int i = 0; i < 7; ++i)
            {
                BlurOffsetWeight[i] = { 0.0f, offsets[i] * texelSizeY, weights[i], 0.0f };
            }
        }
    };

    /**
     * Bloom Pass - 复现原版游戏的 Bloom 处理
     * 包含:
     * 1. 4-tap Bloom 累加 (shader a8c2404f)
     * 2. 7-tap 可分离高斯模糊 (shader bfa8841e)
     */
    class BloomPass
    {
    public:
        static BloomPass* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        /**
         * 执行 Bloom 累加 pass
         * @param bloomMipTextures Bloom mip 级别纹理数组 (最多4个)
         * @param outputRTV 输出渲染目标
         * @param width 输出宽度
         * @param height 输出高度
         */
        void ApplyAccumulate(
            ID3D11ShaderResourceView* const* bloomMipTextures,
            int numMips,
            ID3D11RenderTargetView* outputRTV,
            UINT width,
            UINT height
        );

        /**
         * 执行高斯模糊 pass
         * @param inputTexture 输入纹理
         * @param outputRTV 输出渲染目标
         * @param horizontal true = 水平模糊, false = 垂直模糊
         * @param width 输出宽度
         * @param height 输出高度
         */
        void ApplyGaussianBlur(
            ID3D11ShaderResourceView* inputTexture,
            ID3D11RenderTargetView* outputRTV,
            bool horizontal,
            UINT width,
            UINT height
        );

        /**
         * 执行完整的 Bloom 处理流程
         * Bloom累加 -> 水平模糊 -> 垂直模糊
         * @param inputTexture 输入场景纹理
         * @param bloomTexture Bloom 纹理
         * @param outputRTV 输出渲染目标
         * @param tempRTV 临时渲染目标 (用于两次模糊之间)
         * @param tempSRV 临时纹理 SRV
         * @param width 输出宽度
         * @param height 输出高度
         */
        void ApplyFullBloom(
            ID3D11ShaderResourceView* inputTexture,
            ID3D11ShaderResourceView* bloomTexture,
            ID3D11RenderTargetView* outputRTV,
            ID3D11RenderTargetView* tempRTV,
            ID3D11ShaderResourceView* tempSRV,
            UINT width,
            UINT height
        );

        // 参数设置
        BloomAccumulateConstants& GetAccumulateConstants() { return m_accumulateConstants; }
        GaussianBlurConstants& GetBlurConstants() { return m_blurConstants; }
        void SetAccumulateConstants(const BloomAccumulateConstants& constants) { m_accumulateConstants = constants; }
        void SetBlurConstants(const GaussianBlurConstants& constants) { m_blurConstants = constants; }

        // 设置 Bloom 偏移权重 (从 RenderDoc 捕获的值)
        void SetBloomOffsetWeight(int index, float offsetX, float offsetY, float weight);
        void SetBlurOffsetWeight(int index, float offsetX, float offsetY, float weight);

        bool IsInitialized() const { return m_initialized; }

    private:
        BloomPass() = default;
        ~BloomPass() = default;

        bool CreateShaders();
        bool CreateResources();
        void UpdateAccumulateConstantBuffer();
        void UpdateBlurConstantBuffer();

        static BloomPass* s_instance;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;

        // Shaders
        ComPtr<ID3D11VertexShader> m_vertexShader;        // 全屏三角形 VS (复用)
        ComPtr<ID3D11PixelShader> m_accumulatePS;         // Bloom 累加 PS
        ComPtr<ID3D11PixelShader> m_gaussianBlurPS;       // 高斯模糊 PS

        // Constant Buffers
        ComPtr<ID3D11Buffer> m_accumulateCB;
        ComPtr<ID3D11Buffer> m_blurCB;

        // Samplers
        ComPtr<ID3D11SamplerState> m_linearSampler;

        // 渲染状态
        ComPtr<ID3D11BlendState> m_noBlendState;
        ComPtr<ID3D11DepthStencilState> m_noDepthState;
        ComPtr<ID3D11RasterizerState> m_rasterizerState;

        // 常量
        BloomAccumulateConstants m_accumulateConstants;
        GaussianBlurConstants m_blurConstants;

        bool m_initialized = false;
    };
}
