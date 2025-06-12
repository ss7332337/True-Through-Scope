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

	public:
	/*	static bool isInMainRenderSetup;
		static bool isInDoZPrePass;
		static bool isInDoZPrePass;*/

    private:
        static ID3D11Texture2D* s_FirstPassColorTexture;
        static ID3D11Texture2D* s_FirstPassDepthTexture;
        static ID3D11Texture2D* s_SecondPassColorTexture;
        static ID3D11Texture2D* s_SecondPassDepthTexture;

		static ID3D11Texture2D* s_BackBufferTexture;
		static ID3D11ShaderResourceView* s_BackBufferSRV;


        // Scope textures
        static RE::BSGraphics::Texture* s_ScopeBSTexture;
        static RE::NiTexture* s_ScopeNiTexture;
        
        // Status flags
        static bool s_FirstPassComplete;
        static bool s_SecondPassComplete;
		static bool s_Render_PreUIComplete;
		static int s_ScreenWidth;
		static int s_ScreenHeight;
    };
}
