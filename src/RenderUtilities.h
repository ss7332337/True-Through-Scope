#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include "Constants.h"
#include "RE/Bethesda/BSGraphics.hpp"

namespace ThroughScope
{
	#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = nullptr; } }

    class RenderUtilities
    {
    public:
        // Screen quad vertex structure
        struct ScreenVertex
        {
            float x, y, z;  // Position
            float u, v;     // Texture coordinates
        };

        // Initialization
        static bool Initialize();
        static void Shutdown();
        
        // Texture management
        static bool CreateTemporaryTextures();
        static void ReleaseTemporaryTextures();
        
        // Screen quad rendering
        static bool InitializeScreenQuad();
        static void RenderScreenQuad(ID3D11ShaderResourceView* textureView, float x, float y, float width, float height);
		static bool SetupWeaponScopeShape();
		static void UpdateScopeModelTexture();

        // Texture getters
        static ID3D11Texture2D* GetFirstPassColorTexture() { return s_FirstPassColorTexture; }
        static ID3D11Texture2D* GetFirstPassDepthTexture() { return s_FirstPassDepthTexture; }
        static ID3D11Texture2D* GetSecondPassColorTexture() { return s_SecondPassColorTexture; }
        static ID3D11Texture2D* GetSecondPassDepthTexture() { return s_SecondPassDepthTexture; }

        // Status flags
		static bool IsCreateMaterial() { return s_CreatedMaterial; }
        static bool IsFirstPassComplete() { return s_FirstPassComplete; }
        static void SetFirstPassComplete(bool value) { s_FirstPassComplete = value; }
        static bool IsSecondPassComplete() { return s_SecondPassComplete; }
        static void SetSecondPassComplete(bool value) { s_SecondPassComplete = value; }
        
        // Scope texture management
        static RE::BSGraphics::Texture* GetScopeBSTexture() { return s_ScopeBSTexture; }
        static void SetScopeBSTexture(RE::BSGraphics::Texture* texture) { s_ScopeBSTexture = texture; }
        static RE::NiTexture* GetScopeNiTexture() { return s_ScopeNiTexture; }
        static void SetScopeNiTexture(RE::NiTexture* texture) { s_ScopeNiTexture = texture; }

    private:
        // Temporary textures for multi-pass rendering
		static ID3D11Texture2D* s_DirectX11Texture;
		static ID3D11ShaderResourceView* s_DirectX11SRV;

        static ID3D11Texture2D* s_FirstPassColorTexture;
        static ID3D11Texture2D* s_FirstPassDepthTexture;
        static ID3D11Texture2D* s_SecondPassColorTexture;
        static ID3D11Texture2D* s_SecondPassDepthTexture;
        
        // Screen quad resources
        static ID3D11VertexShader* s_ScreenQuadVS;
        static ID3D11PixelShader* s_ScreenQuadPS;
        static ID3D11InputLayout* s_ScreenQuadInputLayout;
        static ID3D11Buffer* s_ScreenQuadVertexBuffer;
        static ID3D11SamplerState* s_ScreenQuadSamplerState;
        
        // Scope textures
        static RE::BSGraphics::Texture* s_ScopeBSTexture;
        static RE::NiTexture* s_ScopeNiTexture;
        
        // Status flags
        static bool s_ScreenQuadInitialized;
        static bool s_FirstPassComplete;
        static bool s_SecondPassComplete;
		static bool s_CreatedMaterial;
    
	public:
		struct SavedD3DState
		{
			// 渲染目标和深度模板视图
			ID3D11RenderTargetView* renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D11DepthStencilView* depthStencilView;

			// 着色器和资源
			ID3D11VertexShader* vertexShader;
			ID3D11PixelShader* pixelShader;
			ID3D11GeometryShader* geometryShader;
			ID3D11HullShader* hullShader;
			ID3D11DomainShader* domainShader;
			ID3D11ComputeShader* computeShader;
			ID3D11ClassInstance* vsClassInstances[256];
			UINT vsNumClassInstances;

			// 输入装配器状态
			ID3D11InputLayout* inputLayout;
			ID3D11Buffer* vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT vertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT vertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11Buffer* indexBuffer;
			DXGI_FORMAT indexFormat;
			UINT indexOffset;
			D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;

			// 光栅化器状态
			ID3D11RasterizerState* rasterizerState;
			D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			UINT numViewports;
			D3D11_RECT scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			UINT numScissorRects;

			// 混合状态
			ID3D11BlendState* blendState;
			FLOAT blendFactor[4];
			UINT sampleMask;

			// 深度模板状态
			ID3D11DepthStencilState* depthStencilState;
			UINT stencilRef;

			// 着色器资源和采样器
			ID3D11ShaderResourceView* psShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11ShaderResourceView* vsShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11SamplerState* psSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D11SamplerState* vsSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

			// 常量缓冲区
			ID3D11Buffer* vsConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D11Buffer* psConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		};

		// 静态成员
		static SavedD3DState s_SavedState;
    };
}
