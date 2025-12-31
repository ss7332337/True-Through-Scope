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

	ID3D11Texture2D* RenderUtilities::s_MotionVectorBackup = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_FirstPassMVSRV = nullptr;

	ID3D11Texture2D* RenderUtilities::s_TempMVTexture = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_TempMVSRV = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_StencilSRV = nullptr;

	ID3D11VertexShader* RenderUtilities::s_ScopeMVVS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVDebugPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVCopyPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVBlendPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_WhiteOutputPS = nullptr;

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
	const char* g_ScopeMVVSCode = R"(
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


	static bool CreateMVShaders()
	{
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		ID3D11Device* device = (ID3D11Device*)rendererData->device;

		ID3DBlob* blob = nullptr;
		ID3DBlob* errorBlob = nullptr;
		HRESULT hr;

		// --- Compile ScopeMV VS (fullscreen triangle with UV) ---
		hr = D3DCompile(
			g_ScopeMVVSCode,
			strlen(g_ScopeMVVSCode),
			nullptr, nullptr, nullptr,
			"main", "vs_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile ScopeMVVS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			return false;  // Critical - needed for all MV operations
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_ScopeMVVS);
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

		logger::info("Successfully created MV shaders (ScopeMVVS, MVDebugPS, MVCopyPS, MVBlendPS, WhiteOutputPS)");
		return true;
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
		if (s_ScopeMVVS) {
			s_ScopeMVVS->Release();
			s_ScopeMVVS = nullptr;
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
    }
}
