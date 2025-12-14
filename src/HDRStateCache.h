#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace ThroughScope
{
    using Microsoft::WRL::ComPtr;

    /**
     * HDR 状态缓存结构
     * 用于捕获引擎正常 HDR 渲染时的 D3D 状态，然后在第二次渲染时重放
     */
    struct HDRStateCache
    {
        // Pixel Shader
        ComPtr<ID3D11PixelShader> pixelShader;
        
        // Vertex Shader
        ComPtr<ID3D11VertexShader> vertexShader;
        
        // Constant Buffers (PS)
        static constexpr UINT MAX_CB = 14;
        ComPtr<ID3D11Buffer> psConstantBuffers[MAX_CB];
        UINT numPSConstantBuffers = 0;
        
        // Constant Buffers (VS)
        ComPtr<ID3D11Buffer> vsConstantBuffers[MAX_CB];
        UINT numVSConstantBuffers = 0;
        
        // Shader Resources (PS) 
        static constexpr UINT MAX_SRV = 16;
        ComPtr<ID3D11ShaderResourceView> psSRVs[MAX_SRV];
        UINT numPSSRVs = 0;
        
        // Samplers (PS)
        static constexpr UINT MAX_SAMPLERS = 16;
        ComPtr<ID3D11SamplerState> psSamplers[MAX_SAMPLERS];
        UINT numPSSamplers = 0;
        
        // Render Target
        static constexpr UINT MAX_RENDER_TARGETS = 8;
        ComPtr<ID3D11RenderTargetView> renderTargets[MAX_RENDER_TARGETS];
        ComPtr<ID3D11DepthStencilView> depthStencilView;
        UINT numRenderTargets = 0;
        
        // Blend State
        ComPtr<ID3D11BlendState> blendState;
        FLOAT blendFactor[4] = {0, 0, 0, 0};
        UINT sampleMask = 0xFFFFFFFF;
        
        // Depth Stencil State
        ComPtr<ID3D11DepthStencilState> depthStencilState;
        UINT stencilRef = 0;
        
        // Rasterizer State
        ComPtr<ID3D11RasterizerState> rasterizerState;
        
        // Viewport
        D3D11_VIEWPORT viewport = {};
        bool hasViewport = false;
        
        // Input Layout
        ComPtr<ID3D11InputLayout> inputLayout;
        
        // Primitive Topology
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        
        // Geometry (用于全屏三角形)
        ComPtr<ID3D11Buffer> vertexBuffer;
        UINT vertexStride = 0;
        UINT vertexOffset = 0;

        // 复制的纹理内容 (避免原始SRV指向的纹理内容被修改)
        // Bloom纹理 (t0) - 复制内容
        ComPtr<ID3D11Texture2D> bloomTextureCopy;
        ComPtr<ID3D11ShaderResourceView> bloomSRVCopy;
        // Luminance纹理 (t2) - 复制内容
        ComPtr<ID3D11Texture2D> luminanceTextureCopy;
        ComPtr<ID3D11ShaderResourceView> luminanceSRVCopy;
        // Mask纹理 (t3) - 注意: mask可能依赖场景内容，保留原始SRV引用
        // 如果mask也出现问题，后续可以选择复制或为瞄具场景生成新的

        // 有效性标志
        bool isValid = false;
        
        // 捕获计数（用于调试）
        UINT captureCount = 0;
        
        /**
         * 从 D3D Context 捕获当前 HDR 渲染状态
         * 应在 HDR::Render 被调用之前调用
         */
        void Capture(ID3D11DeviceContext* context);
        
        /**
         * 将捕获的状态应用到 D3D Context
         * 用于在第二次渲染后重放 HDR 效果
         */
        void Apply(ID3D11DeviceContext* context);
        
        /**
         * 清除所有缓存状态
         */
        void Clear();
        
        /**
         * 检查缓存是否有效
         */
        bool IsValid() const { return isValid && pixelShader != nullptr; }
    };

    // 全局 HDR 状态缓存实例
    extern HDRStateCache g_HDRStateCache;
    
    // 获取当前 D3D11 Context
    ID3D11DeviceContext* GetCurrentD3DContext();
}
