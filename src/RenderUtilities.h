#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include "Constants.h"
#include "RE/Bethesda/BSGraphics.hpp"

namespace ThroughScope
{
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
		static bool CreateScopeTexture();
        
        // Texture getters
        static ID3D11Texture2D* GetFirstPassColorTexture() { return s_FirstPassColorTexture; }
        static ID3D11Texture2D* GetFirstPassDepthTexture() { return s_FirstPassDepthTexture; }
        static ID3D11Texture2D* GetSecondPassColorTexture() { return s_SecondPassColorTexture; }
        static ID3D11Texture2D* GetSecondPassDepthTexture() { return s_SecondPassDepthTexture; }
        
        // Status flags
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
    };
}
