#include "RenderUtilities.h"

#include "ScopeCamera.h"

#include "Utilities.h"
namespace ThroughScope
{
    ID3D11Texture2D* RenderUtilities::s_FirstPassColorTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_FirstPassDepthTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_SecondPassColorTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_SecondPassDepthTexture = nullptr;

	ID3D11Texture2D* RenderUtilities::s_BackBufferTexture = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_BackBufferSRV = nullptr;

    RE::BSGraphics::Texture* RenderUtilities::s_ScopeBSTexture = nullptr;
    RE::NiTexture* RenderUtilities::s_ScopeNiTexture = nullptr;
    bool RenderUtilities::s_FirstPassComplete = false;
    bool RenderUtilities::s_SecondPassComplete = false;
	bool RenderUtilities::s_Render_PreUIComplete = false;
	int RenderUtilities::s_ScreenWidth = 1920;
	int RenderUtilities::s_ScreenHeight = 1080;
	
	// DLSS/FSR3 upscaling support
	D3D11_VIEWPORT RenderUtilities::s_FirstPassViewport = {};
	bool RenderUtilities::s_HasFirstPassViewport = false;


    bool RenderUtilities::Initialize()
    {
        bool result = CreateTemporaryTextures();

        if (!result) {
            logger::error("Failed to create temporary textures");
            return false;
        }
        return true;
    }

    void RenderUtilities::Shutdown()
    {
        ReleaseTemporaryTextures();
        CleanupBackBufferResources();
    }

    void RenderUtilities::CleanupBackBufferResources()
    {
        SAFE_RELEASE(s_BackBufferTexture);
        SAFE_RELEASE(s_BackBufferSRV);
        logger::info("BackBuffer resources cleaned up successfully");
    }

    bool RenderUtilities::CreateTemporaryTextures()
    {
        // Get D3D11 device from renderer data
        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        ID3D11Device* device = (ID3D11Device*)rendererData->device;

        // Get actual render dimensions from BackBuffer or RenderTarget, not window size
		auto rendererState = RE::BSGraphics::State::GetSingleton();
		unsigned int width = rendererState.backBufferWidth;
		unsigned int height = rendererState.backBufferHeight;

        logger::info("Creating temporary textures with size {}x{}", width, height);

        // Clean up any existing resources
        ReleaseTemporaryTextures();

        // Create color textures for passes
        D3D11_TEXTURE2D_DESC colorDesc;
        ZeroMemory(&colorDesc, sizeof(colorDesc));
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.MipLevels = 1;
        colorDesc.ArraySize = 1;
        colorDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;  // Match render target format
        colorDesc.SampleDesc.Count = 1;
        colorDesc.SampleDesc.Quality = 0;
        colorDesc.Usage = D3D11_USAGE_DEFAULT;
        colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        colorDesc.CPUAccessFlags = 0;
        colorDesc.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&colorDesc, nullptr, &s_FirstPassColorTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create first pass color texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        hr = device->CreateTexture2D(&colorDesc, nullptr, &s_SecondPassColorTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create second pass color texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        // Create depth texture
        D3D11_TEXTURE2D_DESC depthDesc;
        ZeroMemory(&depthDesc, sizeof(depthDesc));
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthDesc.CPUAccessFlags = 0;
        depthDesc.MiscFlags = 0;

        hr = device->CreateTexture2D(&depthDesc, nullptr, &s_FirstPassDepthTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create first pass depth texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        hr = device->CreateTexture2D(&depthDesc, nullptr, &s_SecondPassDepthTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create Second pass depth texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        logger::info("Temporary textures created successfully");
        return true;
    }

    void RenderUtilities::ReleaseTemporaryTextures()
    {
        if (s_FirstPassColorTexture) {
            s_FirstPassColorTexture->Release();
            s_FirstPassColorTexture = nullptr;
        }
        if (s_FirstPassDepthTexture) {
            s_FirstPassDepthTexture->Release();
            s_FirstPassDepthTexture = nullptr;
        }
        if (s_SecondPassColorTexture) {
            s_SecondPassColorTexture->Release();
            s_SecondPassColorTexture = nullptr;
        }
        if (s_SecondPassDepthTexture) {
            s_SecondPassDepthTexture->Release();
            s_SecondPassDepthTexture = nullptr;
        }
    }
}
