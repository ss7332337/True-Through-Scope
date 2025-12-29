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

	ID3D11PixelShader* RenderUtilities::s_ClearVelocityPS = nullptr;
	ID3D11VertexShader* RenderUtilities::s_ClearVelocityVS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_ScopeMVPS = nullptr;
	ID3D11VertexShader* RenderUtilities::s_ScopeMVVS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVDebugPS = nullptr;
	ID3D11PixelShader* RenderUtilities::s_MVMergePS = nullptr;

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

	// Simple Pixel Shader to clear motion vectors (output 0,0,0,0)
	const char* g_ClearVelocityPSCode = 
		"struct PS_OUTPUT { float4 Color : SV_Target; };"
		"PS_OUTPUT main() {"
		"   PS_OUTPUT output;"
		"   output.Color = float4(0.0f, 0.0f, 0.0f, 0.0f);"
		"   return output;"
		"}";

	// Simple Vertex Shader for Fullscreen Triangle (no VB needed)
	const char* g_ClearVelocityVSCode = 
		"struct VS_OUTPUT { float4 Pos : SV_POSITION; };"
		"VS_OUTPUT main(uint id : SV_VertexID) {"
		"   VS_OUTPUT output;"
		"   output.Pos.x = (float)(id / 2) * 4.0 - 1.0;"
		"   output.Pos.y = (float)(id % 2) * 4.0 - 1.0;"
		"   output.Pos.z = 0.0;"
		"   output.Pos.w = 1.0;"
		"   return output;"
		"}";

	// Scope Motion Vector Pixel Shader - calculates correct MV from depth reprojection
	const char* g_ScopeMVPSCode = R"(
		cbuffer MVConstants : register(b0) {
			float4x4 InvViewProj;      // Current frame inverse ViewProj
			float4x4 PrevViewProj;     // Previous frame ViewProj
			float2 ScreenSize;         // Screen dimensions
			float2 Padding;            // Alignment padding
		};
		Texture2D<float> DepthTex : register(t0);
		SamplerState PointSamp : register(s0);
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		float4 main(PS_IN input) : SV_Target {
			float depth = DepthTex.Sample(PointSamp, input.UV);
			float2 ndc = input.UV * 2.0 - 1.0;
			ndc.y = -ndc.y;
			float4 clipPos = float4(ndc, depth, 1.0);
			float4 worldPos = mul(InvViewProj, clipPos);
			worldPos /= worldPos.w;
			float4 prevClip = mul(PrevViewProj, worldPos);
			float2 prevNDC = prevClip.xy / prevClip.w;
			float2 velocity = (ndc - prevNDC) * 0.5;
			return float4(velocity, 0.0, 0.0);
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


	// MV Merge Pixel Shader - merges first pass MV with scope-corrected MV based on region mask
	// Plan A implementation: backup first pass MV, then selectively overwrite scope region
	const char* g_MVMergePSCode = R"(
		cbuffer MVMergeConstants : register(b0) {
			float4x4 InvViewProj;      // Current frame inverse ViewProj (scope camera)
			float4x4 PrevViewProj;     // Previous frame ViewProj (scope camera)
			float2 ScreenSize;         // Screen dimensions
			float2 ScopeCenter;        // Scope center in UV space (0-1)
			float ScopeRadius;         // Scope radius in UV space
			float DepthThreshold;      // Depth threshold for scope region detection
			float2 Padding2;           // Alignment padding
		};
		Texture2D<float2> FirstPassMV : register(t0);  // Backed up first pass MV
		Texture2D<float> DepthTex : register(t1);      // Current depth buffer
		SamplerState PointSamp : register(s0);
		struct PS_IN { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };
		
		float2 main(PS_IN input) : SV_Target {
			float2 uv = input.UV;
			
			// 1. Check if pixel is inside scope circular region
			// Account for aspect ratio by converting to pixel-space distance
			float aspectRatio = ScreenSize.x / ScreenSize.y;
			float2 offset = uv - ScopeCenter;
			offset.x *= aspectRatio;  // Scale X by aspect ratio to make circle round
			float dist = length(offset);
			
			// Adjust radius for aspect ratio comparison
			float adjustedRadius = ScopeRadius * aspectRatio;
			
			if (dist > adjustedRadius) {
				// Outside scope region - return original first pass MV
				return FirstPassMV.Sample(PointSamp, uv);
			}
			
			// 2. Inside scope region - calculate correct MV using scope camera matrices
			float depth = DepthTex.Sample(PointSamp, uv);
			
			// Reconstruct world position from depth
			float2 ndc = uv * 2.0 - 1.0;
			ndc.y = -ndc.y;  // D3D Y flip
			float4 clipPos = float4(ndc, depth, 1.0);
			
			float4 worldPos = mul(InvViewProj, clipPos);
			worldPos /= worldPos.w;
			
			// Reproject using previous frame's scope camera matrix
			float4 prevClip = mul(PrevViewProj, worldPos);
			float2 prevNDC = prevClip.xy / prevClip.w;
			
			// Calculate velocity (MV is in NDC/2 space)
			float2 velocity = (ndc - prevNDC) * 0.5;
			
			// 3. Blend at scope edge for smooth transition
			float edgeFade = saturate((adjustedRadius - dist) / (adjustedRadius * 0.1));
			float2 firstPassMV = FirstPassMV.Sample(PointSamp, uv);
			
			return lerp(firstPassMV, velocity, edgeFade);
		}
	)";

	static bool CreateClearVelocityShader()
	{
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		ID3D11Device* device = (ID3D11Device*)rendererData->device;

		ID3DBlob* blob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		// --- Compile PS ---
		HRESULT hr = D3DCompile(
			g_ClearVelocityPSCode,
			strlen(g_ClearVelocityPSCode),
			nullptr, nullptr, nullptr,
			"main", "ps_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile ClearVelocityPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			return false;
		}
		if (errorBlob) errorBlob->Release();

		hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_ClearVelocityPS);
		blob->Release();
		if (FAILED(hr)) {
			logger::error("Failed to create ClearVelocity pixel shader");
			return false;
		}

		// --- Compile VS ---
		hr = D3DCompile(
			g_ClearVelocityVSCode,
			strlen(g_ClearVelocityVSCode),
			nullptr, nullptr, nullptr,
			"main", "vs_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile ClearVelocityVS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			return false;
		}
		if (errorBlob) errorBlob->Release();

		hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_ClearVelocityVS);
		blob->Release();
		if (FAILED(hr)) {
			logger::error("Failed to create ClearVelocity vertex shader");
			return false;
		}

		// --- Compile ScopeMV PS (for correct motion vector calculation) ---
		hr = D3DCompile(
			g_ScopeMVPSCode,
			strlen(g_ScopeMVPSCode),
			nullptr, nullptr, nullptr,
			"main", "ps_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile ScopeMVPS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Continue - fallback to clear velocity shader
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_ScopeMVPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create ScopeMV pixel shader");
			}
		}

		// --- Compile ScopeMV VS ---
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
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_ScopeMVVS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create ScopeMV vertex shader");
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
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_MVDebugPS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create MVDebug pixel shader");
			}
		}

		// --- Compile MV Merge PS (Plan A) ---
		hr = D3DCompile(
			g_MVMergePSCode,
			strlen(g_MVMergePSCode),
			nullptr, nullptr, nullptr,
			"main", "ps_5_0",
			0, 0, &blob, &errorBlob
		);

		if (FAILED(hr)) {
			if (errorBlob) {
				logger::error("Failed to compile MVMergePS: {}", (char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			// Non-fatal - fallback to existing MV shaders
		} else {
			if (errorBlob) errorBlob->Release();
			hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &RenderUtilities::s_MVMergePS);
			blob->Release();
			if (FAILED(hr)) {
				logger::error("Failed to create MVMerge pixel shader");
			} else {
				logger::info("Successfully created MV Merge shader for Plan A");
			}
		}

		return true;
	}


    bool RenderUtilities::Initialize()
    {
        bool result = CreateTemporaryTextures();

        if (!result) {
            logger::error("Failed to create temporary textures");
            return false;
        }

		if (!CreateClearVelocityShader()) {
			logger::warn("Failed to create ClearVelocity shader - motion vector clearing disabled");
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
		if (s_ClearVelocityPS) {
			s_ClearVelocityPS->Release();
			s_ClearVelocityPS = nullptr;
		}
		if (s_ClearVelocityVS) {
			s_ClearVelocityVS->Release();
			s_ClearVelocityVS = nullptr;
		}
		if (s_MotionVectorBackup) {
			s_MotionVectorBackup->Release();
			s_MotionVectorBackup = nullptr;
		}
		if (s_FirstPassMVSRV) {
			s_FirstPassMVSRV->Release();
			s_FirstPassMVSRV = nullptr;
		}
		if (s_MVMergePS) {
			s_MVMergePS->Release();
			s_MVMergePS = nullptr;
		}
    }
}
