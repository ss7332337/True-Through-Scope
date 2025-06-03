#include "D3DHooks.h"
#include "Utilities.h"
#include <detours.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <ScopeCamera.h>
#include <wrl/client.h>

#include <DDSTextureLoader11.h>
#include "ImGuiManager.h"

#include <xinput.h>
#pragma comment(lib, "xinput.lib")

static constexpr UINT MAX_VIEWPORTS = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
typedef HRESULT (*D3D11Create)(
	_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_In_opt_ CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	_COM_Outptr_opt_ IDXGISwapChain** ppSwapChain,
	_COM_Outptr_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext);

D3D11Create m_D3D11CreateDeviceAndSwapChain_O;

//ID3D11Device* m_Device = nullptr;
//IDXGISwapChain* m_SwapChain = nullptr;
//ID3D11DeviceContext* m_Context = nullptr;

namespace ThroughScope {
    
	using namespace Microsoft::WRL;
    ID3D11ShaderResourceView* D3DHooks::s_ScopeTextureView = nullptr;
	ComPtr<IDXGISwapChain> D3DHooks::s_SwapChain = nullptr;
	WNDPROC D3DHooks::s_OriginalWndProc = nullptr;
	HRESULT(WINAPI* D3DHooks::s_OriginalPresent)(IDXGISwapChain*, UINT, UINT) = nullptr;
	RECT D3DHooks::oldRect{};
	IAStateCache D3DHooks::s_CachedIAState;
	VSStateCache D3DHooks::s_CachedVSState;
	RSStateCache D3DHooks::s_CachedRSState;  // 新增
	float D3DHooks::s_ReticleScale = 1.0f;
	float D3DHooks::s_ReticleOffsetX = 0.5f;
	float D3DHooks::s_ReticleOffsetY = 0.5f;
	bool D3DHooks::s_HasCachedState = false;

	bool D3DHooks::s_isForwardStage = false;
	bool D3DHooks::s_EnableFOVAdjustment = true;
	float D3DHooks::s_FOVAdjustmentSensitivity = 1.0f;
	DWORD64 D3DHooks::s_LastGamepadInputTime = 0;

	// 为瞄准镜创建和管理资源的静态变量
	static ID3D11Texture2D* stagingTexture = nullptr;
	static ID3D11ShaderResourceView* stagingSRV = nullptr;
	static ID3D11PixelShader* scopePixelShader = nullptr;
	static ID3D11SamplerState* samplerState = nullptr;
	static ID3D11BlendState* blendState = nullptr;
	static ID3D11Buffer* constantBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> D3DHooks::s_ReticleTexture = nullptr;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> D3DHooks::s_ReticleSRV = nullptr;

	bool D3DHooks::s_EnableRender = false;
	bool D3DHooks::s_InPresent = false;


	constexpr UINT MAX_SRV_SLOTS = 128;
	constexpr UINT MAX_SAMPLER_SLOTS = 16;
	constexpr UINT MAX_CB_SLOTS = 14; 

	float D3DHooks::s_CurrentRelativeFogRadius = 0.5f;
	float D3DHooks::s_CurrentScopeSwayAmount = 0.1f;
	float D3DHooks::s_CurrentMaxTravel = 0.05f;
	float D3DHooks::s_CurrentRadius = 0.3f;

	// 夜视效果参数初始化
	float D3DHooks::s_NightVisionIntensity = 1.0f;
	float D3DHooks::s_NightVisionNoiseScale = 0.05f;
	float D3DHooks::s_NightVisionNoiseAmount = 0.05f;
	float D3DHooks::s_NightVisionGreenTint = 1.2f;
	int D3DHooks::s_EnableNightVision = 0;

	// 热成像效果参数初始化
	float D3DHooks::s_ThermalIntensity = 1.0f;
	float D3DHooks::s_ThermalThreshold = 0.5f;
	float D3DHooks::s_ThermalContrast = 1.2f;
	float D3DHooks::s_ThermalNoiseAmount = 0.03f;
	int D3DHooks::s_EnableThermalVision = 0;

	static constexpr UINT TARGET_STRIDE = 28;
	static constexpr UINT TARGET_INDEX_COUNT = 96;
	static constexpr UINT TARGET_BUFFER_SIZE = 0x0000000008000000;
	typedef void(__stdcall* D3D11DrawIndexedHook)(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
	typedef void(__stdcall* D3D11RSSetViewportsHook)(ID3D11DeviceContext* pContext, UINT NumViewports, const D3D11_VIEWPORT* pViewports);
	using ClipCur = decltype(&ClipCursor);

	D3D11DrawIndexedHook phookD3D11DrawIndexed = nullptr;
	ClipCur phookClipCursor = nullptr;
	D3D11RSSetViewportsHook phookD3D11RSSetViewports = nullptr;

	D3DHooks* D3DInstance = D3DHooks::GetSington();
	ImGuiManager* imguiMgr;
	ID3D11DeviceContext* m_Context = nullptr;
	ID3D11Device* m_Device = nullptr;
	IDXGISwapChain* m_SwapChain = nullptr;

	bool D3DHooks::isSelfDrawCall = false;

	HRESULT D3DHooks::CreateShaderFromFile(const WCHAR* csoFileNameInOut, const WCHAR* hlslFileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppBlobOut)
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

	void D3DHooks::UpdateScopeParallaxSettings(float relativeFogRadius, float scopeSwayAmount, float maxTravel, float radius)
	{
		s_CurrentRelativeFogRadius = relativeFogRadius;
		s_CurrentScopeSwayAmount = scopeSwayAmount;
		s_CurrentMaxTravel = maxTravel;
		s_CurrentRadius = radius;

		//logger::info("Updated D3D scope settings - Parallax: {:.3f}, {:.3f}, {:.3f}, {:.3f}", relativeFogRadius, scopeSwayAmount, maxTravel, radius);
	}

	void D3DHooks::UpdateNightVisionSettings(float intensity, float noiseScale, float noiseAmount, float greenTint, int enable)
	{
		s_NightVisionIntensity = intensity;
		s_NightVisionNoiseScale = noiseScale;
		s_NightVisionNoiseAmount = noiseAmount;
		s_NightVisionGreenTint = greenTint;
		s_EnableNightVision = enable;
	}

	void D3DHooks::UpdateThermalVisionSettings(float intensity, float threshold, float contrast, float noiseAmount, int enable)
	{
		s_ThermalIntensity = intensity;
		s_ThermalThreshold = threshold;
		s_ThermalContrast = contrast;
		s_ThermalNoiseAmount = noiseAmount;
		s_EnableThermalVision = enable;
	}

	D3DHooks* D3DHooks::GetSington()
	{
		static D3DHooks instance;
		return &instance;
	}

	ID3D11DeviceContext* D3DHooks::GetContext() 
	{
		return m_Context;
	}

	HRESULT WINAPI D3DHooks::D3D11CreateDeviceAndSwapChain_Hook(
		_In_opt_ IDXGIAdapter* pAdapter,
		D3D_DRIVER_TYPE DriverType,
		HMODULE Software,
		UINT Flags,
		_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
		UINT FeatureLevels,
		UINT SDKVersion,
		_In_opt_ CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
		_COM_Outptr_opt_ IDXGISwapChain** ppSwapChain,
		_COM_Outptr_opt_ ID3D11Device** ppDevice,
		_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
		_COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext)
	{
		auto hr = m_D3D11CreateDeviceAndSwapChain_O(
			pAdapter,
			DriverType,
			Software,
			Flags,
			pFeatureLevels,
			FeatureLevels,
			SDKVersion,
			pSwapChainDesc,
			ppSwapChain,
			ppDevice,
			pFeatureLevel,
			ppImmediateContext);

		m_SwapChain = *ppSwapChain;
		m_Device = *ppDevice;
		m_Device->GetImmediateContext(&m_Context);

		GetSington()->HookAllContexts();

		logger::info("Washoi!");

		return hr;
	}

	std::vector<ID3D11DeviceContext*> allContexts;

	void D3DHooks::HookContext(ID3D11DeviceContext* context)
	{
		// 获取正确的虚函数索引
		static UINT drawIndexedIndex = GetDrawIndexedIndex(context);

		void** vtable = *reinterpret_cast<void***>(context);
		void* drawIndexedFunc = vtable[drawIndexedIndex];

		// 避免重复hook
		static std::unordered_set<void*> hookedVtables;
		if (hookedVtables.find(vtable) != hookedVtables.end())
			return;

		hookedVtables.insert(vtable);

		Utilities::CreateAndEnableHook(drawIndexedFunc,
			reinterpret_cast<void*>(hkDrawIndexed),
			reinterpret_cast<LPVOID*>(&phookD3D11DrawIndexed),
			"DrawIndexedHook");
	}

	// 在设备创建后调用
	void D3DHooks::HookAllContexts()
	{
		// Hook主上下文
		HookContext(m_Context);

		// Hook延迟上下文
		/*ID3D11DeviceContext* deferredContext = nullptr;
		for (UINT i = 0; i < 2; i++) {
			if (SUCCEEDED(m_Device->CreateDeferredContext(0, &deferredContext))) {
				HookContext(deferredContext);
				deferredContext->Release();
			}
		}*/
	}

	
	UINT D3DHooks::GetDrawIndexedIndex(ID3D11DeviceContext* context)
	{
		// 创建测试资源
		D3D11_BUFFER_DESC desc = { 0 };
		desc.ByteWidth = 1024;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		ID3D11Buffer* testBuffer = nullptr;
		m_Device->CreateBuffer(&desc, nullptr, &testBuffer);

		// 设置测试状态
		context->IASetIndexBuffer(testBuffer, DXGI_FORMAT_R16_UINT, 0);

		// 特征码扫描
		const BYTE drawCallPattern[] = { 0x48, 0x8B, 0x01, 0xFF, 0x90 };  // mov rax,[rcx]; call [rax+...]

		for (UINT i = 0; i < 100; i++) {
			void* func = reinterpret_cast<void**>(*reinterpret_cast<void***>(context))[i];

			if (memcmp(func, drawCallPattern, sizeof(drawCallPattern)) == 0) {
				logger::info("Found DrawIndexed at index {}", i);
				testBuffer->Release();
				return i;
			}
		}

		testBuffer->Release();
		return 12;  // 默认值
	}

	bool D3DHooks::PreInit()
	{
		REL::Relocation<uintptr_t> D3D11CreateDeviceAndSwapChainAddress{ REL::ID(438126) };
		Utilities::CreateAndEnableHook((LPVOID)D3D11CreateDeviceAndSwapChainAddress.address(), &D3DHooks::D3D11CreateDeviceAndSwapChain_Hook,
			reinterpret_cast<LPVOID*>(&m_D3D11CreateDeviceAndSwapChain_O), "D3D11CreateDeviceAndSwapChainAddress");
		return true;
	}
    
    bool D3DHooks::Initialize() 
	{
		logger::info("Initializing D3D11 hooks...");

		//auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		//if (!rendererData || !rendererData->device || !rendererData->context) {
		//	logger::error("Failed to get D3D11 device or context");
		//	return false;
		//}

		// 先获取SwapChain
		//m_SwapChain = reinterpret_cast<IDXGISwapChain*>(rendererData->renderWindow->swapChain);
		if (!m_SwapChain) {
			logger::error("Failed to get SwapChain");
			return false;
		}

		/*m_Device = reinterpret_cast<ID3D11Device*>(rendererData->device);
		m_Context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);*/
		// 获取窗口句柄并设置窗口过程
		DXGI_SWAP_CHAIN_DESC sd;
		m_SwapChain->GetDesc(&sd);
		::GetWindowRect(sd.OutputWindow, &oldRect);

		s_OriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(sd.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hkWndProc)));

		// 获取虚函数表
		void** contextVTable = *(void***)m_Context;
		void** swapChainVTable = *(void***)m_SwapChain;

		void* drawIndexedFunc = contextVTable[12];
		void* presentFunc = swapChainVTable[8];  // 注意：Present是SwapChain的方法，不是Context的
		void* rsSetViewportsFunc = contextVTable[44];  // RSSetViewports - 索引44

		// 初始化ImGui管理器
		imguiMgr = ImGuiManager::GetSingleton();

		// 创建Hook

		//Utilities::CreateAndEnableHook(drawIndexedFunc, reinterpret_cast<void*>(hkDrawIndexed), reinterpret_cast<LPVOID*>(&phookD3D11DrawIndexed), "DrawIndexedHook");
		Utilities::CreateAndEnableHook(presentFunc, reinterpret_cast<void*>(hkPresent), reinterpret_cast<void**>(&s_OriginalPresent), "Present");
		Utilities::CreateAndEnableHook(&ClipCursor, ClipCursorHook, reinterpret_cast<LPVOID*>(&phookClipCursor), "ClipCursorHook");
		
		Utilities::CreateAndEnableHook(rsSetViewportsFunc, reinterpret_cast<void*>(hkRSSetViewports), reinterpret_cast<void**>(&phookD3D11RSSetViewports), "RSSetViewportsHook");

		logger::info("D3D11 hooks initialized successfully");
		return true;
    }

    void WINAPI D3DHooks::hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
	{
		if (isSelfDrawCall)
			return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

		bool isScopeQuad = IsScopeQuadBeingDrawn(pContext, IndexCount);
		if (isScopeQuad) {

			CacheAllStates();
			if (ImGuiManager::GetSingleton()->IsMenuOpen()) {
				return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
			}
			return;
		} else {
			return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
		}
	}

	bool D3DHooks::LoadAimTexture(const std::string& path)
	{
		if (path.empty())
			return false;

		const wchar_t* tempPath = Utilities::GetWC(path.c_str());
		std::wstring defaultPath = L"Data/Textures/TTS/Reticle/Empty.dds";

		D3DInstance->s_ReticleTexture.Reset();
		HRESULT hr = CreateDDSTextureFromFile(
			m_Device,
			tempPath ? tempPath : defaultPath.c_str(),
			nullptr,
			D3DInstance->s_ReticleSRV.ReleaseAndGetAddressOf());

		if (FAILED(hr)) {
			logger::error("Failed to load reticle texture from path: {}", path);
			return false;
		}

		if (s_ReticleSRV.Get()) {
			m_Context->GenerateMips(s_ReticleSRV.Get());
		}

		if (tempPath)
			free((void*)tempPath);

		return true;
	}

	ID3D11ShaderResourceView* D3DHooks::LoadAimSRV(const std::string& path)
	{
		if (path.empty())
			return nullptr;

		const wchar_t* tempPath = Utilities::GetWC(path.c_str());
		std::wstring defaultPath = L"Data/Textures/TTS/Reticle/Empty.dds";

		D3DInstance->s_ReticleTexture.Reset();
		HRESULT hr = CreateDDSTextureFromFile(
			m_Device,
			tempPath ? tempPath : defaultPath.c_str(),
			nullptr,
			D3DInstance->s_ReticleSRV.ReleaseAndGetAddressOf());

		if (FAILED(hr)) {
			logger::error("Failed to load reticle texture from path: {}", path);
			return nullptr;
		}

		if (s_ReticleSRV.Get()) {
			m_Context->GenerateMips(s_ReticleSRV.Get());
		}

		if (tempPath)
			free((void*)tempPath);

		return s_ReticleSRV.Get();
	}

	void D3DHooks::CacheAllStates()
	{
		CacheIAState(m_Context);
		CacheVSState(m_Context);
		CacheRSState(m_Context);
		s_HasCachedState = true;
	}

	void D3DHooks::RestoreAllCachedStates()
	{
		if (s_HasCachedState) {
			RestoreIAState(m_Context);
			RestoreVSState(m_Context);
			RestoreRSState(m_Context);

			// 清除缓存
			s_CachedIAState.Clear();
			s_CachedVSState.Clear();
			s_CachedRSState.Clear();
			s_HasCachedState = false;
		}
	}

	bool D3DHooks::IsTargetDrawCall(const BufferInfo& vertexInfo, const BufferInfo& indexInfo, UINT indexCount)
	{
		int scopeNodeIndexCount = ScopeCamera::GetScopeNodeIndexCount();
		if (scopeNodeIndexCount <= 0)
			return false;

		return vertexInfo.stride == TARGET_STRIDE && indexCount == scopeNodeIndexCount
		       //&& indexInfo.offset == 2133504
		       && indexInfo.desc.ByteWidth == TARGET_BUFFER_SIZE && vertexInfo.desc.ByteWidth == TARGET_BUFFER_SIZE;
	}

	bool D3DHooks::IsTargetDrawCall(std::vector<BufferInfo> vertexInfos, const BufferInfo& indexInfo, UINT indexCount)
	{
		UINT strideCount = 0;
		for (auto& info : vertexInfos) {
			strideCount += info.stride;
		}
		return strideCount == TARGET_STRIDE && indexCount == TARGET_INDEX_COUNT
		       //&& indexInfo.offset == 2133504
		       && indexInfo.desc.ByteWidth == TARGET_BUFFER_SIZE && vertexInfos[0].desc.ByteWidth == TARGET_BUFFER_SIZE;
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
		if (!s_isForwardStage)
			return false;

		// Check if player exists
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D())
			return false;

		// Check vertex buffer info
		std::vector<BufferInfo> vertexInfo;
		BufferInfo indexInfo;
		if (!GetVertexBuffersInfo(pContext, vertexInfo) || !GetIndexBufferInfo(pContext, indexInfo))
			return false;

		if (IsTargetDrawCall(vertexInfo[0], indexInfo, IndexCount))
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DrawIndexedSRV;
			pContext->PSGetShaderResources(0, 1, DrawIndexedSRV.GetAddressOf());

			if (!DrawIndexedSRV.Get() || DrawIndexedSRV.Get() == stagingSRV)
			return true;
		}

		return false;
	}
	
	bool D3DHooks::IsScopeQuadBeingDrawnShape(ID3D11DeviceContext* pContext, UINT IndexCount)
	{
		if (s_isForwardStage)
			return false;

		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D())
			return false;

		std::vector<BufferInfo> vertexInfo;
		BufferInfo indexInfo;
		if (!GetVertexBuffersInfo(pContext, vertexInfo) || !GetIndexBufferInfo(pContext, indexInfo))
			return false;

		 // Get pixel shader resources to check for the textures
		ID3D11ShaderResourceView* psSRVs[3] = { nullptr };
		pContext->PSGetShaderResources(0, 3, psSRVs);

		bool foundTargetTextures = true;

		// Check the three SRVs
		for (int i = 0; i < 3; i++) {
			if (!psSRVs[i]) {
				foundTargetTextures = false;
				break;
			}

			// Get the resource from SRV
			ID3D11Resource* resource = nullptr;
			psSRVs[i]->GetResource(&resource);

			if (!resource) {
				foundTargetTextures = false;
				break;
			}

			// Check if it's a texture 2D
			ID3D11Texture2D* texture = nullptr;
			HRESULT hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
			resource->Release();

			if (FAILED(hr) || !texture) {
				foundTargetTextures = false;
				break;
			}

			// Get texture description to check dimensions and format
			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);
			texture->Release();

			// Check if dimensions are 1x1 and format is R8G8B8A8_UNORM
			if (desc.Width != 1 || desc.Height != 1 ||
				desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) {
				foundTargetTextures = false;
				break;
			}
		}

		// Release the SRVs
		for (int i = 0; i < 3; i++) {
			if (psSRVs[i]) {
				psSRVs[i]->Release();
			}
		}

		// Return true if this is our target draw call
		return foundTargetTextures;
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

			hr = CreateShaderFromFile(L"Data\\Shaders\\XiFeiLi\\TrueScopeShader.cso", L"HLSL\\TrueScopeShader.hlsl", "main", "ps_5_0", &psBlob);

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

			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = FALSE;
			blendDesc.IndependentBlendEnable = FALSE;

			// 设置第一个渲染目标的混合状态
			blendDesc.RenderTarget[0].BlendEnable = TRUE;

			// 源混合因子：使用源颜色的Alpha
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

			// Alpha混合：直接使用源Alpha覆盖
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

			// 写入所有颜色通道
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			hr = device->CreateBlendState(&blendDesc, &blendState);
			if (FAILED(hr)) {
				logger::error("Failed to create blend state: 0x{:X}", hr);
				// 清理其他已创建的资源...
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

			cbData->reticleScale = s_ReticleScale;
			cbData->reticleOffsetX = s_ReticleOffsetX;
			cbData->reticleOffsetY = s_ReticleOffsetY;

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

			cbData->parallax_Radius = s_CurrentRadius;                        // 折射强度
			cbData->parallax_relativeFogRadius = s_CurrentRelativeFogRadius;  // 视差强度
			cbData->parallax_scopeSwayAmount = s_CurrentScopeSwayAmount;      // 暗角强度
			cbData->parallax_maxTravel = s_CurrentMaxTravel;                  // 折射强度

			// 更新夜视效果参数
			cbData->nightVisionIntensity = s_NightVisionIntensity;
			cbData->nightVisionNoiseScale = s_NightVisionNoiseScale;
			cbData->nightVisionNoiseAmount = s_NightVisionNoiseAmount;
			cbData->nightVisionGreenTint = s_NightVisionGreenTint;
			cbData->enableNightVision = s_EnableNightVision;

			// 更新热成像效果参数
			cbData->thermalIntensity = s_EnableThermalVision ? s_ThermalIntensity : 0.0f;
			cbData->thermalThreshold = s_ThermalThreshold;
			cbData->thermalContrast = s_ThermalContrast;
			cbData->thermalNoiseAmount = s_ThermalNoiseAmount;
			cbData->enableThermalVision = s_EnableThermalVision;

			auto camMat = RE::PlayerCamera::GetSingleton()->cameraRoot->local.rotate.entry[0];
			memcpy_s(&cbData->CameraRotation, sizeof(cbData->CameraRotation), &rotationMatrix, sizeof(rotationMatrix));
			pContext->Unmap(constantBuffer, 0);
		}

		FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		pContext->OMSetBlendState(blendState, blendFactor, 0xffffffff);
		pContext->PSSetConstantBuffers(0, 1, &constantBuffer);
		// 设置我们的像素着色器
		pContext->PSSetShader(scopePixelShader, nullptr, 0);

		// 设置纹理资源和采样器
		pContext->PSSetShaderResources(0, 1, &stagingSRV);
		pContext->PSSetShaderResources(1, 1, s_ReticleSRV.GetAddressOf());
		pContext->PSSetSamplers(0, 1, &samplerState);

		device->Release();
	}

	HRESULT WINAPI D3DHooks::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
	{
		// 防止递归调用
		if (s_InPresent) {
			return s_OriginalPresent(pSwapChain, SyncInterval, Flags);
		}

		s_InPresent = true;
		HRESULT result = S_OK;

		if (imguiMgr && imguiMgr->IsInitialized()) {
			imguiMgr->Render();
		}

		// Call original Present
		result = s_OriginalPresent(pSwapChain, SyncInterval, Flags);

		s_InPresent = false;
		return result;
	}

	LRESULT CALLBACK D3DHooks::hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (imguiMgr && imguiMgr->IsInitialized() && imguiMgr->IsMenuOpen()) {
			if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
				return true;
			}

			// Block game input when ImGui is capturing keyboard/mouse
			ImGuiIO& io = ImGui::GetIO();
			if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
				// Block specific messages that should not be passed to the game
				switch (uMsg) {
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
				case WM_MOUSEWHEEL:
				case WM_MOUSEMOVE:
				case WM_KEYDOWN:
				case WM_KEYUP:
				case WM_CHAR:
					return true;
				}
			}
		}

		auto playerChar = RE::PlayerCharacter::GetSingleton();
		if (uMsg == WM_MOUSEWHEEL && s_EnableFOVAdjustment && playerChar && Utilities::IsInADS(playerChar)) {
			short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			ProcessMouseWheelFOVInput(wheelDelta);
			return true;
		}

		// Pass to the original window procedure
		return CallWindowProcA(s_OriginalWndProc, hWnd, uMsg, wParam, lParam);
	}

	void WINAPI D3DHooks::hkRSSetViewports(ID3D11DeviceContext* pContext, UINT NumViewports, const D3D11_VIEWPORT* pViewports)
	{
		return phookD3D11RSSetViewports(pContext, NumViewports, pViewports);
	}

	void D3DHooks::ProcessGamepadFOVInput()
	{
		// 获取当前时间，防止输入过于频繁
		DWORD64 currentTime = GetTickCount64();
		if (currentTime - s_LastGamepadInputTime < 100)  // 100ms防抖
			return;

		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		// 检查第一个手柄
		if (XInputGetState(0, &state) == ERROR_SUCCESS) {
			// 使用右摇杆Y轴或肩键来调整FOV
			SHORT rightThumbY = state.Gamepad.sThumbRY;

			// 设置死区
			const SHORT THUMB_DEADZONE = 8000;

			if (abs(rightThumbY) > THUMB_DEADZONE) {
				// 将摇杆值转换为FOV调整量
				float normalizedInput = (float)rightThumbY / 32767.0f;
				float fovDelta = normalizedInput * s_FOVAdjustmentSensitivity;

				ScopeCamera::SetTargetFOV(ScopeCamera::GetTargetFOV() + fovDelta);
				s_LastGamepadInputTime = currentTime;
				return;
			}

			// 检查肩键 (LB/RB)
			if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
				ScopeCamera::SetTargetFOV(ScopeCamera::GetTargetFOV() - s_FOVAdjustmentSensitivity * 2);
				s_LastGamepadInputTime = currentTime;
			} else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
				ScopeCamera::SetTargetFOV(ScopeCamera::GetTargetFOV() + s_FOVAdjustmentSensitivity * 2);
				s_LastGamepadInputTime = currentTime;
			}
		}
	}

	void D3DHooks::ProcessMouseWheelFOVInput(short wheelDelta)
	{
		auto playerChar = RE::PlayerCharacter::GetSingleton();

		if (!playerChar || !s_EnableFOVAdjustment || !Utilities::IsInADS(playerChar))
			return;

		// 滚轮向上为正值，向下为负值
		// 每个滚轮单位通常是120
		float fovDelta = (wheelDelta / 120.0f) * s_FOVAdjustmentSensitivity;
		ScopeCamera::SetTargetFOV(ScopeCamera::GetTargetFOV() - fovDelta);
	}

	void D3DHooks::HandleFOVInput()
	{
		auto playerChar = RE::PlayerCharacter::GetSingleton();
		if (!playerChar || !s_EnableFOVAdjustment || !Utilities::IsInADS(playerChar))
			return;

		// 处理手柄输入
		ProcessGamepadFOVInput();
	}

	BOOL __stdcall D3DHooks::ClipCursorHook(RECT* lpRect)
	{
		if (imguiMgr->IsMenuOpen()) {
			if (!lpRect)
				return false;
			*lpRect = oldRect;
		}
		return phookClipCursor(lpRect);
	}

	void D3DHooks::CacheIAState(ID3D11DeviceContext* pContext)
	{
		// 缓存Input Layout
		pContext->IAGetInputLayout(s_CachedIAState.inputLayout.GetAddressOf());

		// 缓存Vertex Buffers
		pContext->IAGetVertexBuffers(0, IAStateCache::MAX_VERTEX_BUFFERS,
			reinterpret_cast<ID3D11Buffer**>(s_CachedIAState.vertexBuffers),
			s_CachedIAState.strides,
			s_CachedIAState.offsets);

		// 缓存Index Buffer
		pContext->IAGetIndexBuffer(s_CachedIAState.indexBuffer.GetAddressOf(),
			&s_CachedIAState.indexFormat,
			&s_CachedIAState.indexOffset);

		// 缓存Primitive Topology
		pContext->IAGetPrimitiveTopology(&s_CachedIAState.topology);
	}

	void D3DHooks::CacheVSState(ID3D11DeviceContext* pContext)
	{
		// 获取D3D11设备
		ID3D11Device* device = nullptr;
		pContext->GetDevice(&device);
		if (!device) {
			logger::error("Failed to get D3D11 device in CacheVSState");
			return;
		}

		// 缓存Vertex Shader
		ID3D11ClassInstance* classInstances[256];
		UINT numClassInstances = 256;
		pContext->VSGetShader(s_CachedVSState.vertexShader.GetAddressOf(),
			classInstances, &numClassInstances);

		// 释放class instances（如果有的话）
		for (UINT i = 0; i < numClassInstances; ++i) {
			if (classInstances[i]) {
				classInstances[i]->Release();
			}
		}

		// 缓存Constant Buffers - 获取原始指针并创建副本
		pContext->VSGetConstantBuffers(0, VSStateCache::MAX_CONSTANT_BUFFERS,
			reinterpret_cast<ID3D11Buffer**>(s_CachedVSState.originalConstantBuffers));

		// 为每个绑定的常量缓冲区创建副本并复制数据
		for (UINT i = 0; i < VSStateCache::MAX_CONSTANT_BUFFERS; ++i) {
			if (s_CachedVSState.originalConstantBuffers[i].Get()) {
				// 获取原始缓冲区描述
				D3D11_BUFFER_DESC originalDesc;
				s_CachedVSState.originalConstantBuffers[i]->GetDesc(&originalDesc);

				// 创建可读的副本缓冲区
				D3D11_BUFFER_DESC copyDesc = originalDesc;
				copyDesc.Usage = D3D11_USAGE_DEFAULT;
				copyDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				copyDesc.CPUAccessFlags = 0;
				copyDesc.MiscFlags = 0;

				HRESULT hr = device->CreateBuffer(&copyDesc, nullptr,
					s_CachedVSState.copiedConstantBuffers[i].GetAddressOf());

				if (SUCCEEDED(hr)) {
					// 复制缓冲区内容
					pContext->CopyResource(s_CachedVSState.copiedConstantBuffers[i].Get(),
						s_CachedVSState.originalConstantBuffers[i].Get());
				} else {
					logger::error("Failed to create copy of constant buffer {}: 0x{:X}", i, hr);
					s_CachedVSState.copiedConstantBuffers[i].Reset();
				}
			}
		}

		// 缓存Shader Resources
		pContext->VSGetShaderResources(0, VSStateCache::MAX_SHADER_RESOURCES,
			reinterpret_cast<ID3D11ShaderResourceView**>(s_CachedVSState.shaderResources));

		// 缓存Samplers
		pContext->VSGetSamplers(0, VSStateCache::MAX_SAMPLERS,
			reinterpret_cast<ID3D11SamplerState**>(s_CachedVSState.samplers));

		device->Release();
	}

	void D3DHooks::RestoreIAState(ID3D11DeviceContext* pContext)
	{
		if (!s_HasCachedState)
			return;

		// 恢复Input Layout
		pContext->IASetInputLayout(s_CachedIAState.inputLayout.Get());

		// 恢复Vertex Buffers
		ID3D11Buffer* vertexBuffers[IAStateCache::MAX_VERTEX_BUFFERS];
		for (int i = 0; i < IAStateCache::MAX_VERTEX_BUFFERS; ++i) {
			vertexBuffers[i] = s_CachedIAState.vertexBuffers[i].Get();
		}
		pContext->IASetVertexBuffers(0, IAStateCache::MAX_VERTEX_BUFFERS,
			vertexBuffers, s_CachedIAState.strides, s_CachedIAState.offsets);

		// 恢复Index Buffer
		pContext->IASetIndexBuffer(s_CachedIAState.indexBuffer.Get(),
			s_CachedIAState.indexFormat, s_CachedIAState.indexOffset);

		// 恢复Primitive Topology
		pContext->IASetPrimitiveTopology(s_CachedIAState.topology);
	}

	void D3DHooks::RestoreVSState(ID3D11DeviceContext* pContext)
	{
		if (!s_HasCachedState)
			return;

		// 恢复Vertex Shader
		pContext->VSSetShader(s_CachedVSState.vertexShader.Get(), nullptr, 0);

		// 恢复Constant Buffers - 使用复制的缓冲区
		ID3D11Buffer* constantBuffers[VSStateCache::MAX_CONSTANT_BUFFERS];
		for (int i = 0; i < VSStateCache::MAX_CONSTANT_BUFFERS; ++i) {
			// 优先使用复制的缓冲区，如果没有则使用原始指针
			if (s_CachedVSState.copiedConstantBuffers[i].Get()) {
				constantBuffers[i] = s_CachedVSState.copiedConstantBuffers[i].Get();
			} else {
				constantBuffers[i] = s_CachedVSState.originalConstantBuffers[i].Get();
			}
		}
		pContext->VSSetConstantBuffers(0, VSStateCache::MAX_CONSTANT_BUFFERS, constantBuffers);

		// 恢复Shader Resources
		ID3D11ShaderResourceView* shaderResources[VSStateCache::MAX_SHADER_RESOURCES];
		for (int i = 0; i < VSStateCache::MAX_SHADER_RESOURCES; ++i) {
			shaderResources[i] = s_CachedVSState.shaderResources[i].Get();
		}
		pContext->VSSetShaderResources(0, VSStateCache::MAX_SHADER_RESOURCES, shaderResources);

		// 恢复Samplers
		ID3D11SamplerState* samplers[VSStateCache::MAX_SAMPLERS];
		for (int i = 0; i < VSStateCache::MAX_SAMPLERS; ++i) {
			samplers[i] = s_CachedVSState.samplers[i].Get();
		}
		pContext->VSSetSamplers(0, VSStateCache::MAX_SAMPLERS, samplers);
	}

	void D3DHooks::CacheRSState(ID3D11DeviceContext* pContext)
	{
		// 缓存Rasterizer State
		pContext->RSGetState(s_CachedRSState.rasterizerState.GetAddressOf());
	}

	void D3DHooks::RestoreRSState(ID3D11DeviceContext* pContext)
	{
		if (!s_HasCachedState)
			return;

		// 恢复Rasterizer State
		pContext->RSSetState(s_CachedRSState.rasterizerState.Get());
	}
}
