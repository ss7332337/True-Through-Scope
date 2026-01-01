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
        static void CleanupBackBufferResources();
        
        // Texture management
        static bool CreateTemporaryTextures();
        static void ReleaseTemporaryTextures();
    
        // Texture getters
        static ID3D11Texture2D* GetFirstPassColorTexture() { return s_FirstPassColorTexture; }
        static ID3D11Texture2D* GetFirstPassDepthTexture() { return s_FirstPassDepthTexture; }
        static ID3D11Texture2D* GetSecondPassColorTexture() { return s_SecondPassColorTexture; }
        static ID3D11Texture2D* GetSecondPassDepthTexture() { return s_SecondPassDepthTexture; }

		static ID3D11Texture2D* GetBackBufferTexture() { return s_BackBufferTexture; }
		static ID3D11ShaderResourceView* GetBackBufferSRV() { return s_BackBufferSRV; }

        // Status flags
        static bool IsFirstPassComplete() { return s_FirstPassComplete; }
        static void SetFirstPassComplete(bool value) { s_FirstPassComplete = value; }
        static bool IsSecondPassComplete() { return s_SecondPassComplete; }
        static void SetSecondPassComplete(bool value) { s_SecondPassComplete = value; }
		static bool IsRender_PreUIComplete() { return s_Render_PreUIComplete; }
		static void SetRender_PreUIComplete(bool value) { s_Render_PreUIComplete = value; }
        
        // Scope texture management
        static RE::BSGraphics::Texture* GetScopeBSTexture() { return s_ScopeBSTexture; }
        static void SetScopeBSTexture(RE::BSGraphics::Texture* texture) { s_ScopeBSTexture = texture; }
        static RE::NiTexture* GetScopeNiTexture() { return s_ScopeNiTexture; }
        static void SetScopeNiTexture(RE::NiTexture* texture) { s_ScopeNiTexture = texture; }
		static float GetScreenWidth() { return s_ScreenWidth; }
		static float GetScreenHeight() { return s_ScreenHeight; }

		// Shader management - Fullscreen VS with UV
		static ID3D11VertexShader* GetFullscreenVS() { return s_FullscreenVS; }

		// MV Debug visualization shader
		static ID3D11PixelShader* GetMVDebugPS() { return s_MVDebugPS; }

		// MV Copy shader for stencil-masked FirstPassMV copying
		static ID3D11PixelShader* GetMVCopyPS() { return s_MVCopyPS; }

		// First pass MV backup for Plan A merge
		static ID3D11Texture2D* GetFirstPassMVBackup() { return s_MotionVectorBackup; }
		static ID3D11ShaderResourceView* GetFirstPassMVSRV() { return s_FirstPassMVSRV; }

		// Temporary MV texture for blend operation (copy of RT29)
		static ID3D11Texture2D* GetTempMVTexture() { return s_TempMVTexture; }
		static ID3D11ShaderResourceView* GetTempMVSRV() { return s_TempMVSRV; }

		// Stencil SRV for reading stencil buffer in shader
		static ID3D11ShaderResourceView* GetStencilSRV() { return s_StencilSRV; }

		// MV Blend shader for edge feathering
		static ID3D11PixelShader* GetMVBlendPS() { return s_MVBlendPS; }

		// White output shader for writing to FG interpolation skip mask
		static ID3D11PixelShader* GetWhiteOutputPS() { return s_WhiteOutputPS; }

		// ========== GBuffer Backup (for stencil-based merge) ==========
		// First pass GBuffer backup for merge after scope rendering
		static ID3D11Texture2D* GetFirstPassGBufferNormal() { return s_GBufferNormalBackup; }
		static ID3D11ShaderResourceView* GetFirstPassGBufferNormalSRV() { return s_FirstPassGBNormalSRV; }
		static ID3D11Texture2D* GetFirstPassGBufferAlbedo() { return s_GBufferAlbedoBackup; }
		static ID3D11ShaderResourceView* GetFirstPassGBufferAlbedoSRV() { return s_FirstPassGBAlbedoSRV; }
		
		// Temporary GBuffer texture for blend operation (copy of current RT)
		static ID3D11Texture2D* GetTempGBufferTexture() { return s_TempGBufferTexture; }
		static ID3D11ShaderResourceView* GetTempGBufferSRV() { return s_TempGBufferSRV; }
		
		// GBuffer copy shader for float4 textures (used for debug display and stencil-masked copy)
		static ID3D11PixelShader* GetGBufferCopyPS() { return s_GBufferCopyPS; }
		
		// Emissive debug shader with amplification for visibility
		static ID3D11PixelShader* GetEmissiveDebugPS() { return s_EmissiveDebugPS; }
		
		// Half-resolution RT merge shader with UV*2 stencil sampling
		static ID3D11PixelShader* GetHalfResMergePS() { return s_HalfResMergePS; }

    private:
        static ID3D11Texture2D* s_FirstPassColorTexture;
        static ID3D11Texture2D* s_FirstPassDepthTexture;
        static ID3D11Texture2D* s_SecondPassColorTexture;
        static ID3D11Texture2D* s_SecondPassDepthTexture;

		static ID3D11Texture2D* s_BackBufferTexture;
		static ID3D11ShaderResourceView* s_BackBufferSRV;

		// Motion vector backup for Plan A MV merge
		static ID3D11Texture2D* s_MotionVectorBackup;
		static ID3D11ShaderResourceView* s_FirstPassMVSRV;

		// Temporary MV texture for blend operation
		static ID3D11Texture2D* s_TempMVTexture;
		static ID3D11ShaderResourceView* s_TempMVSRV;

		// Stencil SRV for reading stencil in shader
		static ID3D11ShaderResourceView* s_StencilSRV;

		// GBuffer backup for stencil-based merge (RT_20 Normal, RT_22 Albedo)
		static ID3D11Texture2D* s_GBufferNormalBackup;
		static ID3D11ShaderResourceView* s_FirstPassGBNormalSRV;
		static ID3D11Texture2D* s_GBufferAlbedoBackup;
		static ID3D11ShaderResourceView* s_FirstPassGBAlbedoSRV;
		
		// Temporary GBuffer texture for blend operation
		static ID3D11Texture2D* s_TempGBufferTexture;
		static ID3D11ShaderResourceView* s_TempGBufferSRV;
		
		// Utility Shaders
	public:
		static ID3D11VertexShader* s_FullscreenVS;      // Fullscreen triangle VS with UV
		static ID3D11PixelShader* s_MVDebugPS;       // MV 可视化 shader
		static ID3D11PixelShader* s_MVCopyPS;        // MV 复制 shader (stencil-masked)
		static ID3D11PixelShader* s_MVBlendPS;       // MV 混合 shader (edge feathering)
		static ID3D11PixelShader* s_WhiteOutputPS;   // White output shader for FG skip mask
		static ID3D11PixelShader* s_GBufferCopyPS;   // GBuffer 复制 shader (float4 output)
		static ID3D11PixelShader* s_EmissiveDebugPS; // Emissive 调试 shader (50x amplification)
		static ID3D11PixelShader* s_HalfResMergePS;  // 半分辨率 RT 合并 shader (UV*2 stencil sampling)

	private:
        // Scope textures
        static RE::BSGraphics::Texture* s_ScopeBSTexture;
        static RE::NiTexture* s_ScopeNiTexture;
        
        // Status flags
        static bool s_FirstPassComplete;
        static bool s_SecondPassComplete;
		static bool s_Render_PreUIComplete;
		static int s_ScreenWidth;
		static int s_ScreenHeight;
		
		// FirstPass viewport for DLSS/FSR3 upscaling
		static D3D11_VIEWPORT s_FirstPassViewport;
		static bool s_HasFirstPassViewport;
		
		// Scope quad screen position for MV merge (Plan A)
		static float s_ScopeQuadCenterU;  // Scope center X in UV space (0-1)
		static float s_ScopeQuadCenterV;  // Scope center Y in UV space (0-1)
		static float s_ScopeQuadRadius;   // Scope radius in UV space
		
	public:
		// Scope screen position for MV merge (Plan A)
		static void SetScopeQuadScreenPosition(float centerU, float centerV, float radius) {
			s_ScopeQuadCenterU = centerU;
			s_ScopeQuadCenterV = centerV;
			s_ScopeQuadRadius = radius;
		}
		static float GetScopeQuadCenterU() { return s_ScopeQuadCenterU; }
		static float GetScopeQuadCenterV() { return s_ScopeQuadCenterV; }
		static float GetScopeQuadRadius() { return s_ScopeQuadRadius; }
		
	public:
		static void SetFirstPassViewport(const D3D11_VIEWPORT& viewport) {
			s_FirstPassViewport = viewport;
			s_HasFirstPassViewport = true;
		}
		static bool GetFirstPassViewport(D3D11_VIEWPORT& outViewport) {
			if (s_HasFirstPassViewport) {
				outViewport = s_FirstPassViewport;
				return true;
			}
			return false;
		}
		static void ClearFirstPassViewport() {
			s_HasFirstPassViewport = false;
		}
    };
}
