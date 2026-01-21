#include "RenderUtilities.h"

#include "ScopeCamera.h"

#include "Utilities.h"
#include <wrl/client.h>
namespace ThroughScope
{
    ID3D11Texture2D* RenderUtilities::s_FirstPassColorTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_FirstPassDepthTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_SecondPassColorTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_SecondPassDepthTexture = nullptr;

	ID3D11Texture2D* RenderUtilities::s_BackBufferTexture = nullptr;
    ID3D11ShaderResourceView* RenderUtilities::s_BackBufferSRV = nullptr;

	ID3D11Texture2D* RenderUtilities::s_MotionVectorBackup = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_FirstPassMVSRV = nullptr;

	ID3D11Texture2D* RenderUtilities::s_TempMVTexture = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_TempMVSRV = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_StencilSRV = nullptr;

	// GBuffer backup for stencil-based merge
	ID3D11Texture2D* RenderUtilities::s_GBufferNormalBackup = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_FirstPassGBNormalSRV = nullptr;
	ID3D11Texture2D* RenderUtilities::s_GBufferAlbedoBackup = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_FirstPassGBAlbedoSRV = nullptr;
	ID3D11Texture2D* RenderUtilities::s_TempGBufferTexture = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_TempGBufferSRV = nullptr;

	ID3D11VertexShader* RenderUtilities::s_FullscreenVS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVDebugPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVCopyPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVBlendPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_WhiteOutputPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_GBufferCopyPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_EmissiveDebugPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_HalfResMergePS = nullptr;

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
	
	// Scope quad screen position for MV merge (Plan A)
	float RenderUtilities::s_ScopeQuadCenterU = 0.5f;  // Default: screen center
	float RenderUtilities::s_ScopeQuadCenterV = 0.5f;
	float RenderUtilities::s_ScopeQuadRadius = 0.15f;  // Default radius


	// Simple Pixel Shader to copy MV from texture (samples t0, outputs directly)
	const char* g_MVCopyPSCode = R"(
		Texture2D<float2> InputMV : register(t0);
		SamplerState PointSamp : register(s0);
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		float2 main(PS_IN input) : SV_Target {
			return InputMV.Sample(PointSamp, input.UV);
		}
	)";

	// Generic Pixel Shader to copy RGBA texture (for GBuffer debug display)
	const char* g_GBufferCopyPSCode = R"(
		Texture2D<float4> InputTex : register(t0);
		SamplerState PointSamp : register(s0);
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		float4 main(PS_IN input) : SV_Target {
			return InputTex.Sample(PointSamp, input.UV);
		}
	)";

	// Half-Resolution Merge Pixel Shader - for merging half-res RTs with stencil masking
	// The half-res RT (960x540) needs to sample the full-res stencil (1920x1080)
	// Since UV is normalized (0-1), the same UV maps to the same screen position in both resolutions
	// t0 = backup texture (first-pass content to restore)
	// t1 = stencil texture (full-res)
	// If stencil == 127, we're in scope region - DISCARD (keep current content)
	// Otherwise, output first-pass content (restore outside scope)
	const char* g_HalfResMergePSCode = R"(
		Texture2D<float4> FirstPassTex : register(t0);
		Texture2D<uint2> StencilTex : register(t1);  // R24G8 format, stencil in .y
		SamplerState PointSamp : register(s0);
		
		cbuffer HalfResParams : register(b0) {
			float2 FullResSize;  // 1920, 1080
			float2 Padding;
		};
		
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		
		float4 main(PS_IN input) : SV_Target {
			// UV is already in 0-1 normalized space for both half-res and full-res
			// Same UV = same screen position
			float2 stencilUV = saturate(input.UV);
			
			// Get stencil value at corresponding full-res pixel
			int2 stencilCoord = int2(stencilUV * FullResSize);
			uint stencilVal = StencilTex.Load(int3(stencilCoord, 0)).y;
			
			// If stencil == 127, we're in scope region - keep current content (discard)
			if (stencilVal == 127) {
				discard;
			}
			
			// Outside scope - restore first-pass content
			return FirstPassTex.Sample(PointSamp, input.UV);
		}
	)";

	// Simple Pixel Shader that outputs white (1.0) - for writing to interpolation skip mask
	const char* g_WhiteOutputPSCode = R"(
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		float main(PS_IN input) : SV_Target {
			return 1.0;  // Mark this pixel for interpolation skip
		}
	)";

	// MV Blend Pixel Shader with Edge Feathering
	// Blends FirstPassMV with CurrentMV based on stencil value and edge distance
	// t0 = FirstPassMV (scene without scope), t1 = CurrentMV (copy of RT29), t2 = StencilTex
	const char* g_MVBlendPSCode = R"(
		cbuffer BlendConstants : register(b0) {
			float2 TexelSize;      // 1.0 / TextureSize
			float FeatherRadius;   // Feather distance in pixels (e.g., 5-10)
			float StencilRef;      // Stencil value for scope (127)
		};

		Texture2D<float2> FirstPassMV : register(t0);    // Backed up first pass MV (scene without scope)
		Texture2D<float2> CurrentMV : register(t1);      // Copy of current RT29 MV (with scope content)
		Texture2D<uint2> StencilTex : register(t2);      // Stencil buffer (R24G8 - we read G8 as .y)
		SamplerState PointSamp : register(s0);

		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };

		float2 main(PS_IN input) : SV_Target {
			// Sample both MV sources
			float2 firstMV = FirstPassMV.Sample(PointSamp, input.UV);
			float2 curMV = CurrentMV.Sample(PointSamp, input.UV);
			
			// Read stencil value at current pixel (G8 channel of R24G8)
			int2 pixelCoord = int2(input.Pos.xy);
			uint stencilVal = StencilTex.Load(int3(pixelCoord, 0)).y;  // .y = stencil (G8)
			
			// Calculate edge distance by sampling neighbors
			float scopeCount = 0;
			float totalCount = 0;
			int sampleRadius = (int)FeatherRadius;
			
			// Sample in a box pattern for accurate edge detection
			for (int dy = -sampleRadius; dy <= sampleRadius; dy++) {
				for (int dx = -sampleRadius; dx <= sampleRadius; dx++) {
					uint neighborStencil = StencilTex.Load(int3(pixelCoord + int2(dx, dy), 0)).y;
					if (neighborStencil == (uint)StencilRef) {
						scopeCount += 1.0;
					}
					totalCount += 1.0;
				}
			}
			
			// Calculate blend factor: 0 = use FirstPassMV, 1 = use CurrentMV (scope)
			// If all neighbors are scope -> blend = 1 (use current MV / scope MV)
			// If all neighbors are non-scope -> blend = 0 (use first pass MV)
			// If mixed -> smooth transition
			float blendFactor = scopeCount / totalCount;
			
			// Blend between first pass MV (outside scope) and current MV (inside scope)
			return lerp(firstMV, curMV, blendFactor);
		}
	)";


	// Scope Motion Vector Vertex Shader - fullscreen triangle with UV
	const char* g_FullscreenVSCode = R"(
		struct VS_OUT { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		VS_OUT main(uint id : SV_VertexID) {
			VS_OUT o;
			o.UV = float2((id << 1) & 2, id & 2);
			o.Pos = float4(o.UV * 2.0 - 1.0, 0.0, 1.0);
			o.Pos.y = -o.Pos.y;
			return o;
		}
	)";

	// MV Debug Pixel Shader - visualizes motion vectors as colors
	const char* g_MVDebugPSCode = R"(
		Texture2D<float2> MVTexture : register(t0);
		SamplerState PointSamp : register(s0);
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		float4 main(PS_IN input) : SV_Target {
			float2 mv = MVTexture.Sample(PointSamp, input.UV);
			// Gray background so we can see the overlay
			float3 color = float3(0.2, 0.2, 0.2);  // Base gray
			// Amplify and visualize: red = horizontal, green = vertical
			float r = abs(mv.x) * 10.0;
			float g = abs(mv.y) * 10.0;
			color.r += r;
			color.g += g;
			// Blue = direction hint (positive = blue tint)
			if (mv.x > 0) color.b += 0.3;
			if (mv.y > 0) color.b += 0.3;
			return float4(saturate(color), 1.0);
		}
	)";

	// Emissive Debug Pixel Shader - amplifies and tone-maps HDR emissive values for visibility
	const char* g_EmissiveDebugPSCode = R"(
		Texture2D<float4> EmissiveTex : register(t0);
		SamplerState PointSamp : register(s0);
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		float4 main(PS_IN input) : SV_Target {
			float4 emissive = EmissiveTex.Sample(PointSamp, input.UV);
			// Amplify emissive values (they are typically very small HDR values)
			float3 color = emissive.rgb * 50.0;  // 50x amplification
			// Apply simple Reinhard tone mapping for HDR visualization
			color = color / (1.0 + color);
			// Add slight gamma for visibility
			color = pow(abs(color), 0.8);
			return float4(color, 1.0);
		}
	)";


	static bool CreateMVShaders()
	{
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		ID3D11Device* device = (ID3D11Device*)rendererData->device;

		ID3DBlob* blob = nullptr;
		ID3DBlob* errorBlob = nullptr;
		HRESULT hr;

		// --- Compile ScopeMV VS (fullscreen triangle with UV) ---
		hr = D3DCompile(
			g_FullscreenVSCode,
			strlen(g_FullscreenVSCode),
			nullptr, nullptr, nullptr,
			"main", "vs_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile FullscreenVS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			return false;  // Critical - needed for all MV operations
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_FullscreenVS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create ScopeMV vertex shader");
				return false;
			}
		}

		// --- Compile MV Debug PS ---
		hr = D3DCompile(
			g_MVDebugPSCode,
			strlen(g_MVDebugPSCode),
			nullptr, nullptr, nullptr,
			"main", "ps_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile MVDebugPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal - debug overlay just won't work
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_MVDebugPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create MVDebug pixel shader");
			}
		}

		// --- Compile MV Copy PS (stencil-masked copy from FirstPassMV) ---
		hr = D3DCompile(
			g_MVCopyPSCode,
			strlen(g_MVCopyPSCode),
			nullptr, nullptr, nullptr,
			"main", "ps_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile MVCopyPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			return false;  // Critical - needed for MV mask merge
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_MVCopyPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create MVCopy pixel shader");
				return false;
			}
		}

		// --- Compile MV Blend PS (edge feathering for Frame Generation) ---
		hr = D3DCompile(
			g_MVBlendPSCode,
			strlen(g_MVBlendPSCode),
			nullptr, nullptr, nullptr,
			"main", "ps_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile MVBlendPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal - will fallback to hard edge version
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_MVBlendPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create MVBlend pixel shader");
			} else {
				logger::info("Successfully created MV Blend shader for edge feathering");
			}
		}

		// Compile WhiteOutputPS (for writing to FG interpolation skip mask)
		hr = D3DCompile(g_WhiteOutputPSCode, strlen(g_WhiteOutputPSCode), "WhiteOutputPS", nullptr, nullptr,
			"main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile WhiteOutputPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_WhiteOutputPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create WhiteOutput pixel shader");
			} else {
				logger::info("Successfully created WhiteOutput shader for FG mask");
			}
		}

		// Compile GBufferCopyPS (for GBuffer debug display - float4 output)
		hr = D3DCompile(g_GBufferCopyPSCode, strlen(g_GBufferCopyPSCode), "GBufferCopyPS", nullptr, nullptr,
			"main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile GBufferCopyPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_GBufferCopyPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create GBufferCopy pixel shader");
			} else {
				logger::info("Successfully created GBufferCopy shader for debug display");
			}
		}

		// Compile EmissiveDebugPS (for Emissive visualization with amplification)
		hr = D3DCompile(g_EmissiveDebugPSCode, strlen(g_EmissiveDebugPSCode), "EmissiveDebugPS", nullptr, nullptr,
			"main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile EmissiveDebugPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_EmissiveDebugPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create EmissiveDebug pixel shader");
			} else {
				logger::info("Successfully created EmissiveDebug shader for debug display");
			}
		}

		// Compile HalfResMergePS (for half-resolution RT merge with UV*2 stencil sampling)
		hr = D3DCompile(g_HalfResMergePSCode, strlen(g_HalfResMergePSCode), "HalfResMergePS", nullptr, nullptr,
			"main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile HalfResMergePS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_HalfResMergePS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create HalfResMerge pixel shader");
			} else {
				logger::info("Successfully created HalfResMerge shader for half-res RT merge");
			}
		}

		logger::info("Successfully created all shaders (FullscreenVS, MVDebugPS, MVCopyPS, MVBlendPS, WhiteOutputPS, GBufferCopyPS, EmissiveDebugPS, HalfResMergePS)");
		return true;
	}


	// Check and resize backup textures if dimensions mismatch (e.g. dynamic resolution)
	void RenderUtilities::ResizeFirstPassTextures(ID3D11Device* device, unsigned int width, unsigned int height)
	{
		bool recreateColor = false;
		bool recreateDepth = false;

		// Check Color Texture
		if (s_FirstPassColorTexture) {
			D3D11_TEXTURE2D_DESC desc;
			s_FirstPassColorTexture->GetDesc(&desc);
			if (desc.Width != width || desc.Height != height) {
				recreateColor = true;
			}
		} else {
			recreateColor = true;
		}

		// Check Depth Texture
		if (s_FirstPassDepthTexture) {
			D3D11_TEXTURE2D_DESC desc;
			s_FirstPassDepthTexture->GetDesc(&desc);
			if (desc.Width != width || desc.Height != height) {
				recreateDepth = true;
			}
		} else {
			recreateDepth = true;
		}

		if (recreateColor) {
			SAFE_RELEASE(s_FirstPassColorTexture);
			SAFE_RELEASE(s_SecondPassColorTexture);

			D3D11_TEXTURE2D_DESC colorDesc;
			ZeroMemory(&colorDesc, sizeof(colorDesc));
			colorDesc.Width = width;
			colorDesc.Height = height;
			colorDesc.MipLevels = 1;
			colorDesc.ArraySize = 1;
			colorDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;  // Standard HDR format
			colorDesc.SampleDesc.Count = 1;
			colorDesc.SampleDesc.Quality = 0;
			colorDesc.Usage = D3D11_USAGE_DEFAULT;
			colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			colorDesc.CPUAccessFlags = 0;
			colorDesc.MiscFlags = 0;

			device->CreateTexture2D(&colorDesc, nullptr, &s_FirstPassColorTexture);
			device->CreateTexture2D(&colorDesc, nullptr, &s_SecondPassColorTexture);
			logger::info("Resized FirstPass Color Textures to {}x{}", width, height);
		}

		if (recreateDepth) {
			SAFE_RELEASE(s_FirstPassDepthTexture);
			SAFE_RELEASE(s_SecondPassDepthTexture);

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

			device->CreateTexture2D(&depthDesc, nullptr, &s_FirstPassDepthTexture);
			device->CreateTexture2D(&depthDesc, nullptr, &s_SecondPassDepthTexture);
			logger::info("Resized FirstPass Depth Textures to {}x{}", width, height);
		}
	}


    bool RenderUtilities::Initialize()
    {
        bool result = CreateTemporaryTextures();

        if (!result) {
            logger::error("Failed to create temporary textures");
            return false;
        }

		if (!CreateMVShaders()) {
			logger::warn("Failed to create MV shaders - motion vector operations disabled");
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

        // Create MV backup texture for Plan A merge (matching RT_29 format: R16G16_FLOAT)
        D3D11_TEXTURE2D_DESC mvDesc;
        ZeroMemory(&mvDesc, sizeof(mvDesc));
        mvDesc.Width = width;
        mvDesc.Height = height;
        mvDesc.MipLevels = 1;
        mvDesc.ArraySize = 1;
        mvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;  // Motion vector format (2-channel float16)
        mvDesc.SampleDesc.Count = 1;
        mvDesc.SampleDesc.Quality = 0;
        mvDesc.Usage = D3D11_USAGE_DEFAULT;
        mvDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;  // Only need SRV for reading backed up MV
        mvDesc.CPUAccessFlags = 0;
        mvDesc.MiscFlags = 0;

        hr = device->CreateTexture2D(&mvDesc, nullptr, &s_MotionVectorBackup);
        if (FAILED(hr)) {
            logger::error("Failed to create MV backup texture. HRESULT: 0x{:X}", hr);
            // Non-fatal - Plan A merge will be disabled
        } else {
            // Create SRV for the MV backup texture
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            hr = device->CreateShaderResourceView(s_MotionVectorBackup, &srvDesc, &s_FirstPassMVSRV);
            if (FAILED(hr)) {
                logger::error("Failed to create MV backup SRV. HRESULT: 0x{:X}", hr);
                s_MotionVectorBackup->Release();
                s_MotionVectorBackup = nullptr;
            } else {
                logger::info("MV backup texture created for Plan A merge ({}x{})", width, height);
            }
        }

		// Create temp MV texture for blend operation (same format as RT29)
		hr = device->CreateTexture2D(&mvDesc, nullptr, &s_TempMVTexture);
		if (FAILED(hr)) {
			logger::error("Failed to create Temp MV texture. HRESULT: 0x{:X}", hr);
		} else {
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			hr = device->CreateShaderResourceView(s_TempMVTexture, &srvDesc, &s_TempMVSRV);
			if (FAILED(hr)) {
				logger::error("Failed to create Temp MV SRV. HRESULT: 0x{:X}", hr);
				s_TempMVTexture->Release();
				s_TempMVTexture = nullptr;
			} else {
				logger::info("Temp MV texture created for edge feathering ({}x{})", width, height);
			}
		}

		// Create Stencil SRV for reading stencil buffer in shader
		// Stencil is in depthStencilTargets[2] which is R24G8_TYPELESS format
		// We need to create SRV with X24_TYPELESS_G8_UINT to read stencil as .y component
		if (rendererData && rendererData->depthStencilTargets[2].texture) {
			ID3D11Texture2D* stencilTex = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;
			
			D3D11_SHADER_RESOURCE_VIEW_DESC stencilSrvDesc;
			ZeroMemory(&stencilSrvDesc, sizeof(stencilSrvDesc));
			stencilSrvDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;  // Read stencil (G8)
			stencilSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			stencilSrvDesc.Texture2D.MostDetailedMip = 0;
			stencilSrvDesc.Texture2D.MipLevels = 1;
			
			hr = device->CreateShaderResourceView(stencilTex, &stencilSrvDesc, &s_StencilSRV);
			if (FAILED(hr)) {
				logger::error("Failed to create Stencil SRV. HRESULT: 0x{:X}", hr);
			} else {
				logger::info("Stencil SRV created for edge feathering");
			}
		}

		// ========== GBuffer Backup Textures (RT_20 Normal, RT_22 Albedo) ==========
		// Create backup textures matching the actual GBuffer formats from the engine
		
		// RT_20: Normal GBuffer (typically DXGI_FORMAT_R10G10B10A2_UNORM or R8G8B8A8)
		if (rendererData && rendererData->renderTargets[20].texture) {
			ID3D11Texture2D* normalTex = (ID3D11Texture2D*)rendererData->renderTargets[20].texture;
			D3D11_TEXTURE2D_DESC normalDesc;
			normalTex->GetDesc(&normalDesc);
			
			// Create backup texture with SRV bind flag
			D3D11_TEXTURE2D_DESC backupDesc = normalDesc;
			backupDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			backupDesc.Usage = D3D11_USAGE_DEFAULT;
			backupDesc.CPUAccessFlags = 0;
			backupDesc.MiscFlags = 0;
			
			hr = device->CreateTexture2D(&backupDesc, nullptr, &s_GBufferNormalBackup);
			if (FAILED(hr)) {
				logger::error("Failed to create GBuffer Normal backup texture. HRESULT: 0x{:X}", hr);
			} else {
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				ZeroMemory(&srvDesc, sizeof(srvDesc));
				srvDesc.Format = normalDesc.Format;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				
				hr = device->CreateShaderResourceView(s_GBufferNormalBackup, &srvDesc, &s_FirstPassGBNormalSRV);
				if (FAILED(hr)) {
					logger::error("Failed to create GBuffer Normal backup SRV. HRESULT: 0x{:X}", hr);
					s_GBufferNormalBackup->Release();
					s_GBufferNormalBackup = nullptr;
				} else {
					logger::info("GBuffer Normal backup (RT_20) created for merge ({}x{}, format: {:X})", 
						normalDesc.Width, normalDesc.Height, (UINT)normalDesc.Format);
				}
			}
		}
		
		// RT_22: Albedo GBuffer (typically DXGI_FORMAT_R8G8B8A8_UNORM)
		if (rendererData && rendererData->renderTargets[22].texture) {
			ID3D11Texture2D* albedoTex = (ID3D11Texture2D*)rendererData->renderTargets[22].texture;
			D3D11_TEXTURE2D_DESC albedoDesc;
			albedoTex->GetDesc(&albedoDesc);
			
			// Create backup texture with SRV bind flag
			D3D11_TEXTURE2D_DESC backupDesc = albedoDesc;
			backupDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			backupDesc.Usage = D3D11_USAGE_DEFAULT;
			backupDesc.CPUAccessFlags = 0;
			backupDesc.MiscFlags = 0;
			
			hr = device->CreateTexture2D(&backupDesc, nullptr, &s_GBufferAlbedoBackup);
			if (FAILED(hr)) {
				logger::error("Failed to create GBuffer Albedo backup texture. HRESULT: 0x{:X}", hr);
			} else {
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				ZeroMemory(&srvDesc, sizeof(srvDesc));
				srvDesc.Format = albedoDesc.Format;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				
				hr = device->CreateShaderResourceView(s_GBufferAlbedoBackup, &srvDesc, &s_FirstPassGBAlbedoSRV);
				if (FAILED(hr)) {
					logger::error("Failed to create GBuffer Albedo backup SRV. HRESULT: 0x{:X}", hr);
					s_GBufferAlbedoBackup->Release();
					s_GBufferAlbedoBackup = nullptr;
				} else {
					logger::info("GBuffer Albedo backup (RT_22) created for merge ({}x{}, format: {:X})", 
						albedoDesc.Width, albedoDesc.Height, (UINT)albedoDesc.Format);
				}
			}
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
		if (s_MotionVectorBackup) {
			s_MotionVectorBackup->Release();
			s_MotionVectorBackup = nullptr;
		}
		if (s_FirstPassMVSRV) {
			s_FirstPassMVSRV->Release();
			s_FirstPassMVSRV = nullptr;
		}
		if (s_FullscreenVS) {
			s_FullscreenVS->Release();
			s_FullscreenVS = nullptr;
		}
		if (s_MVDebugPS) {
			s_MVDebugPS->Release();
			s_MVDebugPS = nullptr;
		}
		if (s_MVCopyPS) {
			s_MVCopyPS->Release();
			s_MVCopyPS = nullptr;
		}
		if (s_MVBlendPS) {
			s_MVBlendPS->Release();
			s_MVBlendPS = nullptr;
		}
		if (s_GBufferCopyPS) {
			s_GBufferCopyPS->Release();
			s_GBufferCopyPS = nullptr;
		}
		if (s_EmissiveDebugPS) {
			s_EmissiveDebugPS->Release();
			s_EmissiveDebugPS = nullptr;
		}
		if (s_HalfResMergePS) {
			s_HalfResMergePS->Release();
			s_HalfResMergePS = nullptr;
		}
		if (s_TempMVTexture) {
			s_TempMVTexture->Release();
			s_TempMVTexture = nullptr;
		}
		if (s_TempMVSRV) {
			s_TempMVSRV->Release();
			s_TempMVSRV = nullptr;
		}
		if (s_StencilSRV) {
			s_StencilSRV->Release();
			s_StencilSRV = nullptr;
		}
		// GBuffer backup resources
		if (s_GBufferNormalBackup) {
			s_GBufferNormalBackup->Release();
			s_GBufferNormalBackup = nullptr;
		}
		if (s_FirstPassGBNormalSRV) {
			s_FirstPassGBNormalSRV->Release();
			s_FirstPassGBNormalSRV = nullptr;
		}
		if (s_GBufferAlbedoBackup) {
			s_GBufferAlbedoBackup->Release();
			s_GBufferAlbedoBackup = nullptr;
		}
		if (s_FirstPassGBAlbedoSRV) {
			s_FirstPassGBAlbedoSRV->Release();
			s_FirstPassGBAlbedoSRV = nullptr;
		}
		if (s_TempGBufferTexture) {
			s_TempGBufferTexture->Release();
			s_TempGBufferTexture = nullptr;
		}
		if (s_TempGBufferSRV) {
			s_TempGBufferSRV->Release();
			s_TempGBufferSRV = nullptr;
		}
    }

	bool RenderUtilities::SafeCopyTexture(ID3D11DeviceContext* context, ID3D11Device* device, ID3D11Texture2D* dest, ID3D11Texture2D* src)
	{
		if (!context || !device || !dest || !src) return false;

		D3D11_TEXTURE2D_DESC srcDesc, destDesc;
		src->GetDesc(&srcDesc);
		dest->GetDesc(&destDesc);

		// 1. Direct Copy if dimensions and format match
		if (srcDesc.Width == destDesc.Width &&
			srcDesc.Height == destDesc.Height &&
			srcDesc.Format == destDesc.Format) {
			context->CopyResource(dest, src);
			return true;
		}

		// 2. Fallback: Shader Copy (Requires RTV on Dest, SRV on Src)
		// Only works if both bind flags are appropriate.
		// Usually works for Color buffers, fails for Depth buffers (unless special care)

		if (!(destDesc.BindFlags & D3D11_BIND_RENDER_TARGET)) {
			logger::warn("SafeCopyTexture: Mismatch ({}x{} vs {}x{}), but Dest is not RenderTarget. Skipping.",
				srcDesc.Width, srcDesc.Height, destDesc.Width, destDesc.Height);
			return false;
		}

		// Check/Create RTV for destination
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> destRTV;
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		ZeroMemory(&rtvDesc, sizeof(rtvDesc));
		rtvDesc.Format = destDesc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		if (FAILED(device->CreateRenderTargetView(dest, &rtvDesc, destRTV.GetAddressOf()))) {
			logger::warn("SafeCopyTexture: Failed to create RTV for backup copy.");
			return false;
		}

		// Check/Create SRV for source
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSRV;

		// If it's a bindable shader resource, created SRV
		if (srcDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = srcDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			// Handle some typeless formats if needed, or assume straight mapping for now
			if (FAILED(device->CreateShaderResourceView(src, &srvDesc, srcSRV.GetAddressOf()))) {
				logger::warn("SafeCopyTexture: Failed to create SRV for source copy.");
				return false;
			}
		}
		else {
			logger::warn("SafeCopyTexture: Mismatch ({}x{} vs {}x{}), but Source is not ShaderResource. Skipping.",
				srcDesc.Width, srcDesc.Height, destDesc.Width, destDesc.Height);
			return false;
		}

		// --- Perform Draw ---
		// Backup state
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
		context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs[0].GetAddressOf(), oldDSV.GetAddressOf());

		D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		context->RSGetViewports(&numViewports, oldViewports);

		// Set State
		context->OMSetRenderTargets(1, destRTV.GetAddressOf(), nullptr);

		D3D11_VIEWPORT vp;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		vp.Width = (float)destDesc.Width;
		vp.Height = (float)destDesc.Height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		context->RSSetViewports(1, &vp);

		// Set Shaders (FullscreenVS + GBufferCopyPS)
		context->VSSetShader(GetFullscreenVS(), nullptr, 0);
		context->PSSetShader(GetGBufferCopyPS(), nullptr, 0); // Copies color
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		ID3D11ShaderResourceView* srvs[] = { srcSRV.Get() };
		context->PSSetShaderResources(0, 1, srvs);
		
		// Draw
		context->Draw(3, 0);

		// Restore
		// Unbind SRV
		ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
		context->PSSetShaderResources(0, 1, nullSRVs);

		context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, (ID3D11RenderTargetView**)oldRTVs, oldDSV.Get());
		context->RSSetViewports(numViewports, oldViewports);
		
		return true;
	}
}
