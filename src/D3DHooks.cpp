#include "D3DHooks.h"
#include "Utilities.h"
#include <detours.h>
#include <wrl/client.h>

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
    
    void D3DHooks::Shutdown() {
        logger::info("Shutting down D3D11 hooks...");
        
        if (originalDrawIndexed) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach(&originalDrawIndexed, hkDrawIndexed);
            DetourTransactionCommit();
        }
        
        if (s_ScopeTextureView) {
            s_ScopeTextureView->Release();
            s_ScopeTextureView = nullptr;
        }
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

		// 为瞄准镜创建和管理资源的静态变量
		static ID3D11Texture2D* stagingTexture = nullptr;
		static ID3D11ShaderResourceView* stagingSRV = nullptr;
		static ID3D11PixelShader* scopePixelShader = nullptr;
		static ID3D11SamplerState* samplerState = nullptr;
		static ID3D11BlendState* blendState = nullptr;

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

			// 编译并创建瞄准镜像素着色器
			// 注意：更新了输入结构以匹配顶点着色器输出
			

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

		// 保存当前的像素着色器状态
		ID3D11PixelShader* originalPS = nullptr;
		pContext->PSGetShader(&originalPS, nullptr, nullptr);

		// 保存当前的着色器资源和采样器状态
		ID3D11ShaderResourceView* originalSRVs[1] = { nullptr };
		ID3D11SamplerState* originalSamplers[1] = { nullptr };
		pContext->PSGetShaderResources(0, 1, originalSRVs);
		pContext->PSGetSamplers(0, 1, originalSamplers);

		// 设置我们的像素着色器
		pContext->PSSetShader(scopePixelShader, nullptr, 0);

		// 设置纹理资源和采样器
		pContext->PSSetShaderResources(0, 1, &stagingSRV);
		pContext->PSSetSamplers(0, 1, &samplerState);

		device->Release();

		// 释放保存的原始状态引用
		if (originalPS)
			originalPS->Release();
		if (originalSRVs[0])
			originalSRVs[0]->Release();
		if (originalSamplers[0])
			originalSamplers[0]->Release();
	}
    
    void D3DHooks::SaveD3DState(ID3D11DeviceContext* pContext, RenderUtilities::SavedD3DState& state) {
        // Save shader resources
        pContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, state.psShaderResources);
        pContext->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, state.vsShaderResources);
        
        // Save samplers
        pContext->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, state.psSamplers);
        pContext->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, state.vsSamplers);
        
        // Save blend state
        pContext->OMGetBlendState(&state.blendState, state.blendFactor, &state.sampleMask);
        
        // Save depth-stencil state
        pContext->OMGetDepthStencilState(&state.depthStencilState, &state.stencilRef);
        
        // Save rasterizer state
        pContext->RSGetState(&state.rasterizerState);
        
        // Save current shaders
        pContext->VSGetShader(&state.vertexShader, state.vsClassInstances, &state.vsNumClassInstances);
        pContext->PSGetShader(&state.pixelShader, nullptr, nullptr);
        pContext->GSGetShader(&state.geometryShader, nullptr, nullptr);
        pContext->HSGetShader(&state.hullShader, nullptr, nullptr);
        pContext->DSGetShader(&state.domainShader, nullptr, nullptr);
        
        // Save input layout and other input assembler state
        pContext->IAGetInputLayout(&state.inputLayout);
        pContext->IAGetPrimitiveTopology(&state.primitiveTopology);
        
        // Save constant buffers
        pContext->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, state.vsConstantBuffers);
        pContext->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, state.psConstantBuffers);
    }
    
    void D3DHooks::RestoreD3DState(ID3D11DeviceContext* pContext, const RenderUtilities::SavedD3DState& state) {
        // Restore shader resources
        pContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, state.psShaderResources);
        pContext->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, state.vsShaderResources);
        
        // Restore samplers
        pContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, state.psSamplers);
        pContext->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, state.vsSamplers);
        
        // Restore blend state
        pContext->OMSetBlendState(state.blendState, state.blendFactor, state.sampleMask);
        
        // Restore depth-stencil state
        pContext->OMSetDepthStencilState(state.depthStencilState, state.stencilRef);
        
        // Restore rasterizer state
        pContext->RSSetState(state.rasterizerState);
        
        // Restore shaders
        pContext->VSSetShader(state.vertexShader, state.vsClassInstances, state.vsNumClassInstances);
        pContext->PSSetShader(state.pixelShader, nullptr, 0);
        pContext->GSSetShader(state.geometryShader, nullptr, 0);
        pContext->HSSetShader(state.hullShader, nullptr, 0);
        pContext->DSSetShader(state.domainShader, nullptr, 0);
        
        // Restore input layout and topology
        pContext->IASetInputLayout(state.inputLayout);
        pContext->IASetPrimitiveTopology(state.primitiveTopology);
        
        // Restore constant buffers
        pContext->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, state.vsConstantBuffers);
        pContext->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, state.psConstantBuffers);
        
        // Release references
        // Shader resources
        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
            if (state.psShaderResources[i]) state.psShaderResources[i]->Release();
            if (state.vsShaderResources[i]) state.vsShaderResources[i]->Release();
        }
        
        // Samplers
        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++) {
            if (state.psSamplers[i]) state.psSamplers[i]->Release();
            if (state.vsSamplers[i]) state.vsSamplers[i]->Release();
        }
        
        // States
        if (state.blendState) state.blendState->Release();
        if (state.depthStencilState) state.depthStencilState->Release();
        if (state.rasterizerState) state.rasterizerState->Release();
        
        // Shaders
        if (state.vertexShader) state.vertexShader->Release();
        if (state.pixelShader) state.pixelShader->Release();
        if (state.geometryShader) state.geometryShader->Release();
        if (state.hullShader) state.hullShader->Release();
        if (state.domainShader) state.domainShader->Release();
        
        // Input layout
        if (state.inputLayout) state.inputLayout->Release();
        
        // Constant buffers
        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
            if (state.vsConstantBuffers[i]) state.vsConstantBuffers[i]->Release();
            if (state.psConstantBuffers[i]) state.psConstantBuffers[i]->Release();
        }
        
        // Class instances
        for (UINT i = 0; i < state.vsNumClassInstances; i++) {
            if (state.vsClassInstances[i]) state.vsClassInstances[i]->Release();
        }
    }
}
