#include "D3DHooks.h"
#include "Utilities.h"
#include <detours.h>
#include <wrl/client.h>
#include <ScopeCamera.h>

namespace ThroughScope {
    
    LPVOID D3DHooks::originalDrawIndexed = nullptr;
    ID3D11ShaderResourceView* D3DHooks::s_ScopeTextureView = nullptr;

	bool D3DHooks::s_isForwardStage = false;

	constexpr UINT MAX_SRV_SLOTS = 128;     // D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT
	constexpr UINT MAX_SAMPLER_SLOTS = 16;  // D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT
	constexpr UINT MAX_CB_SLOTS = 14;       // D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT

	static constexpr UINT TARGET_STRIDE = 12;
	static constexpr UINT TARGET_INDEX_COUNT = 6;
	static constexpr UINT TARGET_BUFFER_SIZE = 0x0000000008000000;


	const char* pixelShaderCode = R"(
            Texture2D scopeTexture : register(t0);
            SamplerState scopeSampler : register(s0);
            
            // 常量缓冲区包含屏幕分辨率、摄像头位置和瞄准镜位置
            cbuffer ScopeConstants : register(b0)
            {
                float screenWidth;
				float screenHeight;
				float2 padding1;  // 16字节对齐

				float3 cameraPosition;
				float padding2;  // 16字节对齐

				float3 scopePosition;
				float padding3;  // 16字节对齐

				float3 lastCameraPosition;
				float padding4;  // 16字节对齐

				float3 lastScopePosition;
				float padding5;  // 16字节对齐

				float parallax_relativeFogRadius;
				float parallax_scopeSwayAmount;
				float parallax_maxTravel;
				float parallax_Radius;

				float4x4 CameraRotation; 
            }
            
            struct PS_INPUT {
				float4 position : SV_POSITION;
				float4 texCoord : TEXCOORD;
				float4 color0 : COLOR0;
				float4 fogColor : COLOR1;
            };

			float2 clampMagnitude(float2 v, float l)
			{
				return normalize(v) * min(length(v), l);
			}

			float getparallax(float d, float2 ds, float dfov)
			{
				return clamp(1 - pow(abs(rcp(parallax_Radius * ds.y) * (parallax_relativeFogRadius * d * ds.y)), parallax_scopeSwayAmount), 0, parallax_maxTravel);
			}
            float2 aspect_ratio_correction(float2 tc)
			{
				tc.x -= 0.5f;
				tc.x *= screenWidth * rcp(screenHeight);
				tc.x += 0.5f;
				return tc;
			}


            float4 main(PS_INPUT input) : SV_TARGET {
                float2 texCoord = input.position.xy / float2(screenWidth, screenHeight);
				float2 aspectCorrectTex = aspect_ratio_correction(texCoord);

				float3 virDir = scopePosition - cameraPosition;
				float3 lastVirDir = lastScopePosition - lastCameraPosition;
				float3 eyeDirectionLerp = virDir - lastVirDir;
				float4 abseyeDirectionLerp = mul(float4((eyeDirectionLerp), 1), CameraRotation);

				if (abseyeDirectionLerp.y < 0 && abseyeDirectionLerp.y >= -0.001)
					abseyeDirectionLerp.y = -0.001;
				else if (abseyeDirectionLerp.y >= 0 && abseyeDirectionLerp.y <= 0.001)
					abseyeDirectionLerp.y = 0.001;

				// Get original texture
				float4 color = scopeTexture.Sample(scopeSampler, texCoord);

				float2 eye_velocity = clampMagnitude(abseyeDirectionLerp.xy , 1.5f);

				float2 parallax_offset = float2(0.5 + eye_velocity.x  , 0.5 - eye_velocity.y);
				float distToParallax = distance(aspectCorrectTex, parallax_offset);
				float2 scope_center = float2(0.5,0.5);
				float distToCenter = distance(aspectCorrectTex, scope_center);

				if (distToCenter > 2) {
					return float4(0, 1, 0, 1);  // Red indicates pixels where step() would return 0
				}
    
				float parallaxValue = (step(distToCenter, 2) * getparallax(distToParallax,float2(1,1),1));
				if (parallaxValue <= 0.01) {
					return float4(0, 0, 1, 1);  // Green indicates pixels where getparallax() returns near 0
				}
    
				// Apply final effect
				color.rgb *= parallaxValue;
				return color;
            }
        )";

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
    
    bool D3DHooks::Initialize() {
        logger::info("Initializing D3D11 hooks...");
        
        // Get the D3D11 device and context from the game's renderer
        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        if (!rendererData || !rendererData->device || !rendererData->context) {
            logger::error("Failed to get D3D11 device or context");
            return false;
        }
        
        // Get the virtual table of the device context
        void** vTable = *(void***)rendererData->context;
        
        // Hook the DrawIndexed function (index 12 in the virtual table)
        void* drawIndexedFunc = vTable[12];
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&drawIndexedFunc, hkDrawIndexed);
        HRESULT result = DetourTransactionCommit();
        
        if (result != NO_ERROR) {
            logger::error("Failed to hook DrawIndexed. Error: {}", result);
            return false;
        }
        
        originalDrawIndexed = drawIndexedFunc;
        logger::info("D3D11 hooks initialized successfully");
        return true;
    }
    
    void WINAPI D3DHooks::hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
        // Check if the current draw call is for our scope quad
        bool isScopeQuad = IsScopeQuadBeingDrawn(pContext, IndexCount);
        
        if (isScopeQuad && RenderUtilities::IsSecondPassComplete()) {

             // Save current shader resources
			ID3D11ShaderResourceView* psShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
			pContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psShaderResources);

			// Clear any shader resources that might conflict with render targets
			ID3D11ShaderResourceView* nullSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
			pContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRV);

            SetScopeTexture(pContext);
            
            // Call the original DrawIndexed
            typedef void (WINAPI* DrawIndexedFunc)(ID3D11DeviceContext*, UINT, UINT, INT);
            ((DrawIndexedFunc)originalDrawIndexed)(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
            
        } else {
            // Not our scope quad or no texture ready, just pass through
            typedef void (WINAPI* DrawIndexedFunc)(ID3D11DeviceContext*, UINT, UINT, INT);
            ((DrawIndexedFunc)originalDrawIndexed)(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
        }
    }

	bool D3DHooks::IsTargetDrawCall(const BufferInfo& vertexInfo, const BufferInfo& indexInfo, UINT indexCount)
	{
		return 
			vertexInfo.stride == 12 
			&& indexCount == 6 
			//&& indexInfo.offset == 2133504
			&& indexInfo.desc.ByteWidth == TARGET_BUFFER_SIZE 
			&& vertexInfo.desc.ByteWidth == TARGET_BUFFER_SIZE;
	}

	UINT D3DHooks::GetVertexBuffersInfo(
		ID3D11DeviceContext* pContext,
		std::vector<BufferInfo>& outInfos,
		UINT maxSlotsToCheck)
	{
		outInfos.clear();

		// 1. 直接尝试获取所有可能的槽位
		std::vector<ID3D11Buffer*> buffers(maxSlotsToCheck);
		std::vector<UINT> strides(maxSlotsToCheck);
		std::vector<UINT> offsets(maxSlotsToCheck);

		pContext->IAGetVertexBuffers(0, maxSlotsToCheck, buffers.data(), strides.data(), offsets.data());

		// 2. 计算实际绑定的缓冲区数量
		UINT actualCount = 0;
		for (UINT i = 0; i < maxSlotsToCheck; ++i) {
			if (buffers[i] != nullptr) {
				actualCount++;
			}
		}

		if (actualCount == 0)
			return 0;

		// 3. 填充输出结构
		outInfos.resize(actualCount);
		UINT validIndex = 0;
		for (UINT i = 0; i < maxSlotsToCheck && validIndex < actualCount; ++i) {
			if (buffers[i] != nullptr) {
				outInfos[validIndex].stride = strides[i];
				outInfos[validIndex].offset = offsets[i];
				buffers[i]->GetDesc(&outInfos[validIndex].desc);
				buffers[i]->Release();  // 释放获取的引用
				validIndex++;
			}
		}

		return actualCount;
	}

	bool D3DHooks::GetIndexBufferInfo(ID3D11DeviceContext* pContext, BufferInfo& outInfo)
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
		DXGI_FORMAT format;

		// 获取索引缓冲区
		pContext->IAGetIndexBuffer(&indexBuffer, &format, &outInfo.offset);

		if (!indexBuffer)
			return false;

		// 获取缓冲区描述
		indexBuffer->GetDesc(&outInfo.desc);

		// 将格式信息存入stride（因为索引缓冲区没有stride概念）
		outInfo.stride = (format == DXGI_FORMAT_R32_UINT) ? 4 : 2;

		return true;
	}
    
    bool D3DHooks::IsScopeQuadBeingDrawn(ID3D11DeviceContext* pContext, UINT IndexCount)
	{
		// Our scope should use exactly 6 indices (2 triangles)
		if (IndexCount != 6 || !s_isForwardStage)
			return false;

		// Check if player exists
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D())
			return false;

		std::vector<BufferInfo> vertexInfo;
		BufferInfo indexInfo;
		if (!GetVertexBuffersInfo(pContext, vertexInfo) || !GetIndexBufferInfo(pContext, indexInfo))
			return false;

		if (IsTargetDrawCall(vertexInfo[0], indexInfo, IndexCount))
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DrawIndexedSRV;
			pContext->PSGetShaderResources(0, 1, DrawIndexedSRV.GetAddressOf());

			if (!DrawIndexedSRV.Get())
				return true;
		}

		return false;
	}
    
    void D3DHooks::SetScopeTexture(ID3D11DeviceContext* pContext)
	{
		// 确保我们有有效的纹理
		if (!RenderUtilities::GetSecondPassColorTexture()) {
			logger::error("No second pass texture available");
			return;
		}

		// 获取D3D11设备
		ID3D11Device* device = nullptr;
		pContext->GetDevice(&device);
		if (!device) {
			logger::error("Failed to get D3D11 device in SetScopeTexture");
			return;
		}

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		UINT screenWidth = rendererData->renderWindow[0].windowWidth;
		UINT screenHeight = rendererData->renderWindow[0].windowHeight;

		// 获取玩家摄像头位置
		auto playerCamera = RE::PlayerCharacter::GetSingleton()->Get3D(true)->GetObjectByName("Camera");
		RE::NiPoint3 cameraPos(0, 0, 0);
		RE::NiPoint3 lastCameraPos(0, 0, 0);

		if (playerCamera) {
			cameraPos = playerCamera->world.translate;
			lastCameraPos = playerCamera->previousWorld.translate;
		}

		// 获取ScopeNode位置
		RE::NiPoint3 scopePos(0, 0, 0);
		RE::NiPoint3 lastScopePos(0, 0, 0);
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (playerCharacter && playerCharacter->Get3D()) {
			auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
			if (weaponNode && weaponNode->IsNode()) {
				auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
				auto scopeNode = weaponNiNode->GetObjectByName("ScopeNode");

				if (scopeNode) {
					scopePos = scopeNode->world.translate;
					lastScopePos = scopeNode->previousWorld.translate;
				}
			}
		}

		// 为瞄准镜创建和管理资源的静态变量
		static ID3D11Texture2D* stagingTexture = nullptr;
		static ID3D11ShaderResourceView* stagingSRV = nullptr;
		static ID3D11PixelShader* scopePixelShader = nullptr;
		static ID3D11SamplerState* samplerState = nullptr;
		static ID3D11BlendState* blendState = nullptr;
		static ID3D11Buffer* constantBuffer = nullptr;

		// 获取纹理描述
		D3D11_TEXTURE2D_DESC srcTexDesc;
		RenderUtilities::GetSecondPassColorTexture()->GetDesc(&srcTexDesc);

		// 创建或重新创建资源(如果需要)
		if (!stagingTexture) {
			// 创建中间纹理
			D3D11_TEXTURE2D_DESC stagingDesc = srcTexDesc;
			stagingDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			stagingDesc.MiscFlags = 0;
			stagingDesc.SampleDesc.Count = 1;
			stagingDesc.SampleDesc.Quality = 0;
			stagingDesc.Usage = D3D11_USAGE_DEFAULT;
			stagingDesc.CPUAccessFlags = 0;

			HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
			if (FAILED(hr)) {
				logger::error("Failed to create staging texture: 0x{:X}", hr);
				device->Release();
				return;
			}

			// 创建着色器资源视图(SRV)
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = stagingDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;

			hr = device->CreateShaderResourceView(stagingTexture, &srvDesc, &stagingSRV);
			if (FAILED(hr)) {
				logger::error("Failed to create staging SRV: 0x{:X}", hr);
				stagingTexture->Release();
				stagingTexture = nullptr;
				device->Release();
				return;
			}

			 // 创建常量缓冲区
			D3D11_BUFFER_DESC cbDesc;
			ZeroMemory(&cbDesc, sizeof(cbDesc));
			cbDesc.ByteWidth = sizeof(ScopeConstantBuffer);
			cbDesc.Usage = D3D11_USAGE_DYNAMIC;
			cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			cbDesc.MiscFlags = 0;
			cbDesc.StructureByteStride = 0;

			hr = device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);
			if (FAILED(hr)) {
				logger::error("Failed to create constant buffer: 0x{:X}", hr);
				stagingSRV->Release();
				stagingTexture->Release();
				stagingTexture = nullptr;
				stagingSRV = nullptr;
				device->Release();
				return;
			}

			ID3DBlob* psBlob = nullptr;
			ID3DBlob* errorBlob = nullptr;

			hr = D3DCompile(
				pixelShaderCode, strlen(pixelShaderCode),
				"ScopePixelShader", nullptr, nullptr,
				"main", "ps_5_0",
				0, 0, &psBlob, &errorBlob);

			if (FAILED(hr)) {
				if (errorBlob) {
					logger::error("Pixel shader compilation failed: {}", (char*)errorBlob->GetBufferPointer());
					errorBlob->Release();
				}
				device->Release();
				return;
			}

			// 创建像素着色器
			hr = device->CreatePixelShader(
				psBlob->GetBufferPointer(),
				psBlob->GetBufferSize(),
				nullptr,
				&scopePixelShader);

			if (FAILED(hr)) {
				logger::error("Failed to create pixel shader: 0x{:X}", hr);
				psBlob->Release();
				device->Release();
				return;
			}

			psBlob->Release();

			// 创建采样器状态
			D3D11_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			samplerDesc.MinLOD = 0;
			samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

			hr = device->CreateSamplerState(&samplerDesc, &samplerState);
			if (FAILED(hr)) {
				logger::error("Failed to create sampler state: 0x{:X}", hr);
				scopePixelShader->Release();
				scopePixelShader = nullptr;
				device->Release();
				return;
			}

			logger::info("Successfully created all scope rendering resources");
		}

		// 复制/解析纹理内容
		if (srcTexDesc.SampleDesc.Count > 1) {
			pContext->ResolveSubresource(
				stagingTexture, 0,
				RenderUtilities::GetSecondPassColorTexture(), 0,
				srcTexDesc.Format);
		} else {
			pContext->CopyResource(stagingTexture, RenderUtilities::GetSecondPassColorTexture());
		}

		 // 更新常量缓冲区数据
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = pContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr)) {
			ScopeConstantBuffer* cbData = (ScopeConstantBuffer*)mappedResource.pData;

			// 填充常量缓冲区数据
			cbData->screenWidth = static_cast<float>(screenWidth);
			cbData->screenHeight = static_cast<float>(screenHeight);

			cbData->cameraPosition[0] = cameraPos.x;
			cbData->cameraPosition[1] = cameraPos.y;
			cbData->cameraPosition[2] = cameraPos.z;

			cbData->scopePosition[0] = scopePos.x;
			cbData->scopePosition[1] = scopePos.y;
			cbData->scopePosition[2] = scopePos.z;

			cbData->lastCameraPosition[0] = lastCameraPos.x;
			cbData->lastCameraPosition[1] = lastCameraPos.y;
			cbData->lastCameraPosition[2] = lastCameraPos.z;

			cbData->lastScopePosition[0] = lastScopePos.x;
			cbData->lastScopePosition[1] = lastScopePos.y;
			cbData->lastScopePosition[2] = lastScopePos.z;

			 DirectX::XMFLOAT4X4 rotationMatrix = {
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1
			};

			 rotationMatrix._11 = playerCamera->world.rotate.entry[0].x;
			 rotationMatrix._12 = playerCamera->world.rotate.entry[0].y;
			 rotationMatrix._13 = playerCamera->world.rotate.entry[0].z;
			 rotationMatrix._14 = playerCamera->world.rotate.entry[0].w;

			 rotationMatrix._21 = playerCamera->world.rotate.entry[1].x;
			 rotationMatrix._22 = playerCamera->world.rotate.entry[1].y;
			 rotationMatrix._23 = playerCamera->world.rotate.entry[1].z;
			 rotationMatrix._24 = playerCamera->world.rotate.entry[1].w;

			 rotationMatrix._31 = playerCamera->world.rotate.entry[2].x;
			 rotationMatrix._32 = playerCamera->world.rotate.entry[2].y;
			 rotationMatrix._33 = playerCamera->world.rotate.entry[2].z;
			 rotationMatrix._34 = playerCamera->world.rotate.entry[2].w;

			// 效果强度参数 - 可以通过配置文件或UI调整
			cbData->parallax_Radius = 2.0f;              // 折射强度
			cbData->parallax_relativeFogRadius = 8.0f;  // 视差强度
			cbData->parallax_scopeSwayAmount = 2.0f;     // 暗角强度
			cbData->parallax_maxTravel = 16.0f;           // 折射强度

			//auto camMat = playerCamera->local.rotate.entry[0];
			auto camMat = RE::PlayerCamera::GetSingleton() -> cameraRoot->local.rotate.entry[0];
			memcpy_s(&cbData->CameraRotation, sizeof(cbData->CameraRotation), &rotationMatrix, sizeof(rotationMatrix));
			pContext->Unmap(constantBuffer, 0);
		}

		pContext->PSSetConstantBuffers(0, 1, &constantBuffer);

		// 设置我们的像素着色器
		pContext->PSSetShader(scopePixelShader, nullptr, 0);

		// 设置纹理资源和采样器
		pContext->PSSetShaderResources(0, 1, &stagingSRV);
		pContext->PSSetSamplers(0, 1, &samplerState);

		device->Release();
	}
}
