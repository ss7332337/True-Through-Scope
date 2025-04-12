#include "Hook.h"
#include <d3dcompiler.h>
#include <MinHook.h>
#include <wrl/client.h>


#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=nullptr; } }
#ifndef HR
#	define HR(x)                                                 \
		{                                                         \
			HRESULT hr = (x);                                     \
			if (FAILED(hr)) {                                     \
				logger::error("[-] {}, {}, {}", __FILE__, __LINE__, hr); \
			}                                                     \
		}
#endif

Hook* _instance = nullptr;
RENDERDOC_API_1_6_0* rdoc_api = nullptr;

constexpr UINT MAX_SRV_SLOTS = 128;     // D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT
constexpr UINT MAX_SAMPLER_SLOTS = 16;  // D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT
constexpr UINT MAX_CB_SLOTS = 14;       // D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT

struct SavedState
{
	// IA Stage
	ID3D11InputLayout* pInputLayout;
	ID3D11Buffer* pVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT VertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT VertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	ID3D11Buffer* pIndexBuffer;
	DXGI_FORMAT IndexBufferFormat;
	UINT IndexBufferOffset;
	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;

	// VS Stage
	ID3D11VertexShader* pVS;
	ID3D11Buffer* pVSCBuffers[MAX_CB_SLOTS];
	ID3D11ShaderResourceView* pVSSRVs[MAX_SRV_SLOTS];
	ID3D11SamplerState* pVSSamplers[MAX_SAMPLER_SLOTS];

	// PS Stage
	ID3D11PixelShader* pPS;
	ID3D11Buffer* pPSCBuffers[MAX_CB_SLOTS];
	ID3D11ShaderResourceView* pPSSRVs[MAX_SRV_SLOTS];
	ID3D11SamplerState* pPSSamplers[MAX_SAMPLER_SLOTS];

	// RS Stage
	D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT NumViewports;
	ID3D11RasterizerState* pRasterizerState;

	// OM Stage
	ID3D11RenderTargetView* pRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView* pDSV;
	ID3D11BlendState* pBlendState;
	FLOAT BlendFactor[4];
	UINT SampleMask;
	ID3D11DepthStencilState* pDepthStencilState;
	UINT StencilRef;
};


Hook::Hook(RE::PlayerCamera* pcam) :
	m_playerCamera(pcam),
	m_mirrorConstantBuffer(nullptr),
	m_cbSize(0)
{
	// 初始化镜像常量缓冲区
	_instance = this;
	//HookDX11_Init();
	//InitMirrorResources();
}

void SaveState(ID3D11DeviceContext* pContext, SavedState& state)
{
	// IA Stage
	pContext->IAGetInputLayout(&state.pInputLayout);
	pContext->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
		state.pVertexBuffers, state.VertexStrides, state.VertexOffsets);
	pContext->IAGetIndexBuffer(&state.pIndexBuffer, &state.IndexBufferFormat, &state.IndexBufferOffset);
	pContext->IAGetPrimitiveTopology(&state.PrimitiveTopology);
	// VS Stage
	pContext->VSGetShader(&state.pVS, nullptr, nullptr);
	pContext->VSGetConstantBuffers(0, MAX_CB_SLOTS, state.pVSCBuffers);
	pContext->VSGetShaderResources(0, MAX_SRV_SLOTS, state.pVSSRVs);
	pContext->VSGetSamplers(0, MAX_SAMPLER_SLOTS, state.pVSSamplers);
	// PS Stage
	pContext->PSGetShader(&state.pPS, nullptr, nullptr);
	pContext->PSGetConstantBuffers(0, MAX_CB_SLOTS, state.pPSCBuffers);
	pContext->PSGetShaderResources(0, MAX_SRV_SLOTS, state.pPSSRVs);
	pContext->PSGetSamplers(0, MAX_SAMPLER_SLOTS, state.pPSSamplers);
	// RS Stage
	state.NumViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	pContext->RSGetViewports(&state.NumViewports, state.Viewports);
	pContext->RSGetState(&state.pRasterizerState);
	// OM Stage
	pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state.pRTVs, &state.pDSV);
	pContext->OMGetBlendState(&state.pBlendState, state.BlendFactor, &state.SampleMask);
	pContext->OMGetDepthStencilState(&state.pDepthStencilState, &state.StencilRef);
}

void RestoreState(ID3D11DeviceContext* pContext, SavedState& state)
{
	// IA Stage
	pContext->IASetInputLayout(state.pInputLayout);
	pContext->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
		state.pVertexBuffers, state.VertexStrides, state.VertexOffsets);
	pContext->IASetIndexBuffer(state.pIndexBuffer, state.IndexBufferFormat, state.IndexBufferOffset);
	pContext->IASetPrimitiveTopology(state.PrimitiveTopology);
	// VS Stage
	pContext->VSSetShader(state.pVS, nullptr, 0);
	pContext->VSSetConstantBuffers(0, MAX_CB_SLOTS, state.pVSCBuffers);
	pContext->VSSetShaderResources(0, MAX_SRV_SLOTS, state.pVSSRVs);
	pContext->VSSetSamplers(0, MAX_SAMPLER_SLOTS, state.pVSSamplers);
	// PS Stage
	pContext->PSSetShader(state.pPS, nullptr, 0);
	pContext->PSSetConstantBuffers(0, MAX_CB_SLOTS, state.pPSCBuffers);
	pContext->PSSetShaderResources(0, MAX_SRV_SLOTS, state.pPSSRVs);
	pContext->PSSetSamplers(0, MAX_SAMPLER_SLOTS, state.pPSSamplers);
	// RS Stage
	pContext->RSSetViewports(state.NumViewports, state.Viewports);
	pContext->RSSetState(state.pRasterizerState);
	// OM Stage
	pContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state.pRTVs, state.pDSV);
	pContext->OMSetBlendState(state.pBlendState, state.BlendFactor, state.SampleMask);
	pContext->OMSetDepthStencilState(state.pDepthStencilState, state.StencilRef);
	// 释放临时引用
#define SAFE_RELEASE_ARRAY(arr, count) \
	for (UINT i = 0; i < count; ++i) SAFE_RELEASE(arr[i])
	SAFE_RELEASE(state.pInputLayout);
	SAFE_RELEASE_ARRAY(state.pVertexBuffers, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
	SAFE_RELEASE(state.pIndexBuffer);
	SAFE_RELEASE(state.pVS);
	SAFE_RELEASE_ARRAY(state.pVSCBuffers, MAX_CB_SLOTS);
	SAFE_RELEASE_ARRAY(state.pVSSRVs, MAX_SRV_SLOTS);
	SAFE_RELEASE_ARRAY(state.pVSSamplers, MAX_SAMPLER_SLOTS);
	SAFE_RELEASE(state.pPS);
	SAFE_RELEASE_ARRAY(state.pPSCBuffers, MAX_CB_SLOTS);
	SAFE_RELEASE_ARRAY(state.pPSSRVs, MAX_SRV_SLOTS);
	SAFE_RELEASE_ARRAY(state.pPSSamplers, MAX_SAMPLER_SLOTS);
	SAFE_RELEASE(state.pRasterizerState);
	SAFE_RELEASE_ARRAY(state.pRTVs, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
	SAFE_RELEASE(state.pDSV);
	SAFE_RELEASE(state.pBlendState);
	SAFE_RELEASE(state.pDepthStencilState);
#undef SAFE_RELEASE_ARRAY
}

HRESULT CreateShaderFromFile(const WCHAR* csoFileNameInOut, const WCHAR* hlslFileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	if (csoFileNameInOut && D3DReadFileToBlob(csoFileNameInOut, ppBlobOut) == S_OK) {
		return hr;
	} else {
		DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG

		dwShaderFlags |= D3DCOMPILE_DEBUG;

		dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		ID3DBlob* errorBlob = nullptr;
		hr = D3DCompileFromFile(hlslFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel,
			dwShaderFlags, 0, ppBlobOut, &errorBlob);
		if (FAILED(hr)) {
			if (errorBlob != nullptr) {
				OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			SAFE_RELEASE(errorBlob);
			return hr;
		}

		if (csoFileNameInOut) {
			return D3DWriteBlobToFile(*ppBlobOut, csoFileNameInOut, FALSE);
		}
	}

	return hr;
}

DirectX::XMMATRIX Hook::GetGameViewMatrix()
{
	auto state = RE::BSGraphics::State::GetSingleton();
	const auto& viewMat = state.cameraState.camViewData.viewMat;
	// 视图矩阵以行主序存储，直接构造XMMATRIX
	return DirectX::XMMATRIX(
		viewMat[0].m128_f32[0], viewMat[0].m128_f32[1], viewMat[0].m128_f32[2], viewMat[0].m128_f32[3],
		viewMat[1].m128_f32[0], viewMat[1].m128_f32[1], viewMat[1].m128_f32[2], viewMat[1].m128_f32[3],
		viewMat[2].m128_f32[0], viewMat[2].m128_f32[1], viewMat[2].m128_f32[2], viewMat[2].m128_f32[3],
		viewMat[3].m128_f32[0], viewMat[3].m128_f32[1], viewMat[3].m128_f32[2], viewMat[3].m128_f32[3]);
}

DirectX::XMMATRIX Hook::GetGameProjectionMatrix()
{
	auto state = RE::BSGraphics::State::GetSingleton();
	const auto& projMat = state.cameraState.camViewData.projMat;
	// 投影矩阵同样以行主序存储，直接构造XMMATRIX
	return DirectX::XMMATRIX(
		projMat[0].m128_f32[0], projMat[0].m128_f32[1], projMat[0].m128_f32[2], projMat[0].m128_f32[3],
		projMat[1].m128_f32[0], projMat[1].m128_f32[1], projMat[1].m128_f32[2], projMat[1].m128_f32[3],
		projMat[2].m128_f32[0], projMat[2].m128_f32[1], projMat[2].m128_f32[2], projMat[2].m128_f32[3],
		projMat[3].m128_f32[0], projMat[3].m128_f32[1], projMat[3].m128_f32[2], projMat[3].m128_f32[3]);
}


DirectX::XMMATRIX GetMainCameraViewMatrix(RE::NiCamera* camera)
{
	if (!camera)
		return DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX viewMatrix = DirectX::XMMATRIX(
		camera->worldToCam[0][0], camera->worldToCam[1][0], camera->worldToCam[2][0], camera->worldToCam[3][0],
		camera->worldToCam[0][1], camera->worldToCam[1][1], camera->worldToCam[2][1], camera->worldToCam[3][1],
		camera->worldToCam[0][2], camera->worldToCam[1][2], camera->worldToCam[2][2], camera->worldToCam[3][2],
		camera->worldToCam[0][3], camera->worldToCam[1][3], camera->worldToCam[2][3], camera->worldToCam[3][3]);
	return viewMatrix;
}


void Hook::UpdateMirrorViewMatrix()
{
	if (!m_mirrorConstantBuffer)
		return;

	// Get the current view and projection matrices from the game
	// This is game-specific and might require finding these matrices
	// For this example, we'll create a mirror view by reflecting the current view

	// Find game's view matrix (placeholder - you need to implement this)
	DirectX::XMMATRIX gameViewMatrix = GetGameViewMatrix();
	DirectX::XMMATRIX gameProjMatrix = GetGameProjectionMatrix();

	// Define the mirror plane (position and normal)
	// This should be adjusted based on where you want the mirror to be in the game world
	DirectX::XMFLOAT3 mirrorPosition = { 0.0f, 0.0f, 10.0f };  // Position of mirror in world space
	DirectX::XMFLOAT3 mirrorNormal = { 0.0f, 0.0f, -1.0f };    // Normal pointing toward viewer

	// Normalize the mirror normal
	DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&mirrorNormal));

	// Create a reflection matrix for the mirror plane
	DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&mirrorPosition);
	float d = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(normal, position));
	DirectX::XMFLOAT4 plane = {
		DirectX::XMVectorGetX(normal),
		DirectX::XMVectorGetY(normal),
		DirectX::XMVectorGetZ(normal),
		d
	};

	DirectX::XMMATRIX reflectionMatrix = DirectX::XMMatrixReflect(DirectX::XMLoadFloat4(&plane));

	// Create mirror view matrix by reflecting the camera
	DirectX::XMMATRIX mirrorViewMatrix = reflectionMatrix * gameViewMatrix;

	// Update constant buffer with new matrices
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = g_Context->Map(m_mirrorConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr)) {
		MirrorConstants* constants = (MirrorConstants*)mappedResource.pData;
		constants->viewMatrix = DirectX::XMMatrixTranspose(mirrorViewMatrix);
		constants->projectionMatrix = DirectX::XMMatrixTranspose(gameProjMatrix);
		constants->mirrorPlane = plane;
		g_Context->Unmap(m_mirrorConstantBuffer, 0);
	}
}

void Hook::CompileMirrorShaders()
{
	// Vertex shader code for rendering a quad
	const char* vsCode = R"(
        struct VS_INPUT {
            float3 Position : POSITION;
            float2 TexCoord : TEXCOORD0;
        };
        
        struct VS_OUTPUT {
            float4 Position : SV_POSITION;
            float2 TexCoord : TEXCOORD0;
        };
        
        VS_OUTPUT main(VS_INPUT input) {
            VS_OUTPUT output;
            output.Position = float4(input.Position, 1.0f);
            output.TexCoord = input.TexCoord;
            return output;
        }
    )";

	// Pixel shader code for sampling the mirror texture
	const char* psCode = R"(
        Texture2D mirrorTexture : register(t0);
        SamplerState mirrorSampler : register(s0);
        
        struct PS_INPUT {
            float4 Position : SV_POSITION;
            float2 TexCoord : TEXCOORD0;
        };
        
        float4 main(PS_INPUT input) : SV_TARGET {
            float4 color = mirrorTexture.Sample(mirrorSampler, input.TexCoord);
            // Add some effects to make it look like a mirror (slight tint, etc.)
            color.rgb *= float3(0.95f, 0.97f, 1.0f); // Slight blue tint
            color.a = 0.9f; // Slight transparency
            return color;
        }
    )";

	// Compile shaders
	ID3DBlob* pVSBlob = nullptr;
	ID3DBlob* pPSBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	// Use D3DCompile to compile shaders
	HRESULT hr = D3DCompile(vsCode, strlen(vsCode), "MirrorVS", nullptr, nullptr, "main", "vs_5_0",
		D3DCOMPILE_ENABLE_STRICTNESS, 0, &pVSBlob, &pErrorBlob);

	if (FAILED(hr)) {
		if (pErrorBlob) {
			logger::error("Vertex shader compilation failed: %s", (char*)pErrorBlob->GetBufferPointer());
			pErrorBlob->Release();
		}
		return;
	}

	hr = D3DCompile(psCode, strlen(psCode), "MirrorPS", nullptr, nullptr, "main", "ps_5_0",
		D3DCOMPILE_ENABLE_STRICTNESS, 0, &pPSBlob, &pErrorBlob);

	if (FAILED(hr)) {
		if (pErrorBlob) {
			logger::error("Pixel shader compilation failed: %s", (char*)pErrorBlob->GetBufferPointer());
			pErrorBlob->Release();
		}
		if (pVSBlob)
			pVSBlob->Release();
		return;
	}

	// Create shader objects
	hr = g_Device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, m_mirrorVS.GetAddressOf());
	if (FAILED(hr)) {
		logger::error("Failed to create vertex shader");
		if (pVSBlob)
			pVSBlob->Release();
		if (pPSBlob)
			pPSBlob->Release();
		return;
	}

	hr = g_Device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, m_mirrorPS.GetAddressOf());
	if (FAILED(hr)) {
		logger::error("Failed to create pixel shader");
		if (pVSBlob)
			pVSBlob->Release();
		if (pPSBlob)
			pPSBlob->Release();
		return;
	}

	// Create input layout
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	hr = g_Device->CreateInputLayout(layout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), m_mirrorInputLayout.GetAddressOf());
	if (FAILED(hr)) {
		logger::error("Failed to create input layout");
	}

	// Release shader blobs
	if (pVSBlob)
		pVSBlob->Release();
	if (pPSBlob)
		pPSBlob->Release();
}


void Hook::InitMirrorResources()
{
	if (mirror_initialized)
		return;

	// Create mirror texture
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = MIRROR_WIDTH;
	texDesc.Height = MIRROR_HEIGHT;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* pMirrorTexture = nullptr;
	HRESULT hr = g_Device->CreateTexture2D(&texDesc, nullptr, &pMirrorTexture);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror texture");
		return;
	}

	// Create RTV for the mirror texture
	hr = g_Device->CreateRenderTargetView(pMirrorTexture, nullptr, &pMirrorRTV);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror RTV");
		SAFE_RELEASE(pMirrorTexture);
		return;
	}

	// Create SRV for the mirror texture
	hr = g_Device->CreateShaderResourceView(pMirrorTexture, nullptr, &pMirrorSRV);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror SRV");
		SAFE_RELEASE(pMirrorTexture);
		SAFE_RELEASE(pMirrorRTV);
		return;
	}

	// Create depth texture for mirror
	texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ID3D11Texture2D* pMirrorDepthTexture = nullptr;
	hr = g_Device->CreateTexture2D(&texDesc, nullptr, &pMirrorDepthTexture);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror depth texture");
		SAFE_RELEASE(pMirrorTexture);
		SAFE_RELEASE(pMirrorRTV);
		SAFE_RELEASE(pMirrorSRV);
		return;
	}

	// Create DSV for the mirror depth texture
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = texDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	hr = g_Device->CreateDepthStencilView(pMirrorDepthTexture, &dsvDesc, &pMirrorDSV);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror DSV");
		SAFE_RELEASE(pMirrorTexture);
		SAFE_RELEASE(pMirrorRTV);
		SAFE_RELEASE(pMirrorSRV);
		SAFE_RELEASE(pMirrorDepthTexture);
		return;
	}

	// Create depth stencil state
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dsDesc.StencilEnable = FALSE;

	hr = g_Device->CreateDepthStencilState(&dsDesc, &pMirrorDepthState);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror depth stencil state");
		SAFE_RELEASE(pMirrorTexture);
		SAFE_RELEASE(pMirrorRTV);
		SAFE_RELEASE(pMirrorSRV);
		SAFE_RELEASE(pMirrorDepthTexture);
		SAFE_RELEASE(pMirrorDSV);
		return;
	}

	// Create sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = g_Device->CreateSamplerState(&sampDesc, &pMirrorSampler);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror sampler state");
		SAFE_RELEASE(pMirrorTexture);
		SAFE_RELEASE(pMirrorRTV);
		SAFE_RELEASE(pMirrorSRV);
		SAFE_RELEASE(pMirrorDepthTexture);
		SAFE_RELEASE(pMirrorDSV);
		SAFE_RELEASE(pMirrorDepthState);
		return;
	}

	// Create constant buffer for mirror view matrix
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(MirrorConstants);
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	hr = g_Device->CreateBuffer(&bufferDesc, nullptr, &m_mirrorConstantBuffer);
	if (FAILED(hr)) {
		logger::error("Failed to create mirror constant buffer");
		SAFE_RELEASE(pMirrorTexture);
		SAFE_RELEASE(pMirrorRTV);
		SAFE_RELEASE(pMirrorSRV);
		SAFE_RELEASE(pMirrorDepthTexture);
		SAFE_RELEASE(pMirrorDSV);
		SAFE_RELEASE(pMirrorDepthState);
		SAFE_RELEASE(pMirrorSampler);
		return;
	}

	// Create shaders for rendering mirror to screen
	CompileMirrorShaders();

	// Cleanup textures (views retain references)
	SAFE_RELEASE(pMirrorTexture);
	SAFE_RELEASE(pMirrorDepthTexture);

	mirror_initialized = true;
	logger::info("Mirror resources initialized successfully");
}

void __stdcall Hook::DrawIndexedHook(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	static bool renderingMirror = false;
	if (renderingMirror) {
		// 使用原始函数来避免递归
		_instance->oldFuncs.phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
		return;
	}

	// 先执行原始绘制（主场景渲染）
	_instance->oldFuncs.phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

	// Check if mirror resources are initialized
	if (!_instance->mirror_initialized || !_instance->pMirrorRTV || !_instance->pMirrorDSV)
		return;

	// Only render to mirror for certain draw calls (optional optimization)
	// You can add filtering here to only render important geometry to the mirror

	// Store a flag to avoid recursion
	renderingMirror = true;

	// Save original state
	SavedState originalState = {};
	SaveState(pContext, originalState);

	// Configure for mirror rendering
	// Set the mirror render target and depth buffer
	pContext->OMSetRenderTargets(1, &_instance->pMirrorRTV, _instance->pMirrorDSV);

	// Clear the mirror render target and depth buffer
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	pContext->ClearRenderTargetView(_instance->pMirrorRTV, clearColor);
	pContext->ClearDepthStencilView(_instance->pMirrorDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Set mirror viewport
	D3D11_VIEWPORT vp = {
		0.0f, 0.0f,
		static_cast<float>(_instance->MIRROR_WIDTH),
		static_cast<float>(_instance->MIRROR_HEIGHT),
		0.0f, 1.0f
	};
	pContext->RSSetViewports(1, &vp);

	// Set mirror depth state
	pContext->OMSetDepthStencilState(_instance->pMirrorDepthState, 0);

	// Update and set the mirror view matrix constant buffer
	_instance->UpdateMirrorViewMatrix();

	// Backup original VS constant buffers so we can restore them after
	ID3D11Buffer* originalVSConstantBuffers[8] = {};
	pContext->VSGetConstantBuffers(0, 8, originalVSConstantBuffers);

	// Set our mirror constant buffer to the first slot
	// Note: You may need to use a different slot depending on the game's shader expectations
	pContext->VSSetConstantBuffers(0, 1, &_instance->m_mirrorConstantBuffer);

	// Now perform the mirror draw call
	// This renders the scene from the mirror's perspective
	_instance->oldFuncs.phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

	// Restore original VS constant buffers
	pContext->VSSetConstantBuffers(0, 8, originalVSConstantBuffers);
	for (int i = 0; i < 8; i++) {
		SAFE_RELEASE(originalVSConstantBuffers[i]);
	}

	// Restore original state
	RestoreState(pContext, originalState);

	// Reset flag
	renderingMirror = false;
}


HRESULT __stdcall Hook::PresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	// Ensure mirror resources are initialized
	static bool firstFrame = true;
	if (firstFrame) {
		_instance->InitMirrorResources();
		firstFrame = false;
	}

	// Handle RenderDoc capture trigger
	if (GetAsyncKeyState(VK_F4) & 1) {
		logger::info("Frame capture requested");
		if (rdoc_api)
			rdoc_api->TriggerCapture();
	}

	// Draw mirror to screen if initialized
	if (_instance->mirror_initialized && _instance->pMirrorSRV) {
		ID3D11Texture2D* pBackBuffer = nullptr;
		HRESULT hr = pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

		if (SUCCEEDED(hr)) {
			ID3D11RenderTargetView* pRTV = nullptr;
			hr = _instance->g_Device->CreateRenderTargetView(pBackBuffer, nullptr, &pRTV);

			if (SUCCEEDED(hr)) {
				// Save current state
				SavedState originalState;
				SaveState(_instance->g_Context.Get(), originalState);

				// Set render target
				_instance->g_Context->OMSetRenderTargets(1, &pRTV, nullptr);

				// Set shaders for rendering mirror quad
				_instance->g_Context->VSSetShader(_instance->m_mirrorVS.Get(), nullptr, 0);
				_instance->g_Context->PSSetShader(_instance->m_mirrorPS.Get(), nullptr, 0);

				// Set mirror texture
				_instance->g_Context->PSSetShaderResources(0, 1, &_instance->pMirrorSRV);
				_instance->g_Context->PSSetSamplers(0, 1, &_instance->pMirrorSampler);

				// Set viewport for mirror display (position in top-right corner)
				D3D11_VIEWPORT vp = {};
				vp.Width = static_cast<float>(_instance->MIRROR_WIDTH);
				vp.Height = static_cast<float>(_instance->MIRROR_HEIGHT);
				vp.TopLeftX = static_cast<float>(_instance->windowWidth - _instance->MIRROR_WIDTH - 10);
				vp.TopLeftY = 10.0f;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 0.1f;
				_instance->g_Context->RSSetViewports(1, &vp);

				// Create and set blend state for transparency
				ID3D11BlendState* pMirrorBlendState = nullptr;
				D3D11_BLEND_DESC blendDesc = {};
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

				_instance->g_Device->CreateBlendState(&blendDesc, &pMirrorBlendState);
				_instance->g_Context->OMSetBlendState(pMirrorBlendState, nullptr, 0xffffffff);

				// Create quad vertices
				struct Vertex
				{
					DirectX::XMFLOAT3 pos;
					DirectX::XMFLOAT2 tex;
				};

				Vertex vertices[] = {
					{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },  // Bottom left
					{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },   // Top left
					{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },   // Bottom right
					{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } }     // Top right
				};

				// Create vertex buffer
				D3D11_BUFFER_DESC bd = {};
				bd.ByteWidth = sizeof(vertices);
				bd.Usage = D3D11_USAGE_DEFAULT;
				bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices;

				ID3D11Buffer* pVertexBuffer = nullptr;
				_instance->g_Device->CreateBuffer(&bd, &initData, &pVertexBuffer);

				// Set vertex buffer
				UINT stride = sizeof(Vertex);
				UINT offset = 0;
				_instance->g_Context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
				_instance->g_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				_instance->g_Context->IASetInputLayout(_instance->m_mirrorInputLayout.Get());

				// Draw the quad
				_instance->g_Context->Draw(4, 0);

				// Cleanup resources
				SAFE_RELEASE(pVertexBuffer);
				SAFE_RELEASE(pMirrorBlendState);
				SAFE_RELEASE(pRTV);

				// Restore original state
				RestoreState(_instance->g_Context.Get(), originalState);
			}

			SAFE_RELEASE(pBackBuffer);
		}
	}

	// Continue with original Present function
	return _instance->oldFuncs.phookD3D11Present(pSwapChain, SyncInterval, Flags);
}

bool CreateAndEnableHook(void* target, void* hook, void** original, const char* hookName)
{
	if (MH_CreateHook(target, hook, original) != MH_OK) {
		logger::error("Failed to create %s hook", hookName);
		return false;
	}
	if (MH_EnableHook(target) != MH_OK) {
		logger::error("Failed to enable %s hook", hookName);
		return false;
	}
	return true;
}


void Hook::InitRenderDoc()
{
	HMODULE mod = LoadLibraryA("renderdoc.dll");
	if (!mod) {
		logger::error("Failed to load renderdoc.dll. Error code: {}", GetLastError());
		return;
	}

	pRENDERDOC_GetAPI RENDERDOC_GetAPI =
		(pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
	if (!RENDERDOC_GetAPI) {
		logger::error("Failed to get RENDERDOC_GetAPI function. Error code: {}", GetLastError());
		return;
	}

	int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
	if (ret != 1) {
		logger::error("RENDERDOC_GetAPI failed with return code: {}", ret);
		return;
	}

	int major, minor, patch;
	rdoc_api->GetAPIVersion(&major, &minor, &patch);
	logger::info("RenderDoc API v{}.{}.{} loaded", major, minor, patch);

	// Set RenderDoc options
	rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_AllowFullscreen, 1);
	rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_AllowVSync, 1);
	rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_APIValidation, 1);
	rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_CaptureAllCmdLists, 1);
	rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks, 1);
	rdoc_api->SetCaptureOptionU32(eRENDERDOC_Option_RefAllResources, 1);

	logger::info("RenderDoc initialized successfully");
}

DWORD __stdcall Hook::HookDX11_Init()
{

	const auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	g_Swapchain = (IDXGISwapChain*)static_cast<void*>(rendererData->renderWindow->swapChain);
	g_Device = (ID3D11Device*)static_cast<void*>(rendererData->device);
	g_Context = (ID3D11DeviceContext*)static_cast<void*>(rendererData->context);
	m_hWnd = (HWND)rendererData->renderWindow->hwnd;
	windowHeight = rendererData->renderWindow->windowHeight;
	windowWidth = rendererData->renderWindow->windowWidth;

	// 获取虚函数表
	pSwapChainVTable = *reinterpret_cast<DWORD_PTR**>(g_Swapchain.Get());
	pDeviceVTable = *reinterpret_cast<DWORD_PTR**>(g_Device.Get());
	pDeviceContextVTable = *reinterpret_cast<DWORD_PTR**>(g_Context.Get());


	logger::info("Get VTable");

	// 初始化MinHook
	if (MH_Initialize() != MH_OK) {
		logger::error("Failed to initialize MinHook");
		return 1;
	}

	const std::pair<DWORD_PTR*, HookInfo> hooks[] = {
		{ pSwapChainVTable, HookInfo{ 8, reinterpret_cast<void*>(PresentHook), reinterpret_cast<void**>(&oldFuncs.phookD3D11Present), "PresentHook" } },
		{ pDeviceContextVTable, HookInfo{ 12, reinterpret_cast<void*>(DrawIndexedHook), reinterpret_cast<void**>(&oldFuncs.phookD3D11DrawIndexed), "DrawIndexedHook" } },
	};

	for (const auto& [vtable, info] : hooks) {
		CreateAndEnableHook(reinterpret_cast<void*>(vtable[info.index]), info.hook, info.original, info.name);
	}

	//DWORD old_protect;
	//VirtualProtect(oldFuncs.phookD3D11Present, 2, PAGE_EXECUTE_READWRITE, &old_protect);

	if (rdoc_api && g_Device.Get()) {
		// Get DXGI device for RenderDoc
		IDXGIDevice* pDXGIDevice = nullptr;
		if (SUCCEEDED(g_Device->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice))) {
			// Set active window and register this device+window combo
			rdoc_api->SetActiveWindow((void*)pDXGIDevice, m_hWnd);
			logger::info("Set RenderDoc active window to {:x}", (uintptr_t)m_hWnd);
			pDXGIDevice->Release();
		} else {
			logger::error("Failed to get DXGI device for RenderDoc");
		}
	}

	return S_OK;
}




bool Hook::mirror_initialized = false;
