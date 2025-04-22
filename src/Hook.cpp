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
using namespace RE::BSGraphics;




Hook::Hook(RE::PlayerCamera* pcam) :
	m_playerCamera(pcam),
	m_mirrorConstantBuffer(nullptr),
	m_cbSize(0)
{
	_instance = this;
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

bool isOuputDebugText = false;

void __stdcall Hook::DrawIndexedHook(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	return _instance->oldFuncs.phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}


HRESULT __stdcall Hook::PresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (GetAsyncKeyState(VK_F4) & 1) {
		logger::info("Frame capture requested");
		if (rdoc_api)
			rdoc_api->TriggerCapture();
	}
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

	//REL::Relocation<std::uintptr_t> vtable_DrawWorld(RE::ImageSpaceEffectTemporalAA::VTABLE[0]);
	//const auto oldFuncTAA = vtable_TAA.write_vfunc(1, reinterpret_cast<std::uintptr_t>(&HookedRender_TAA::thunk));
	//HookedRender_TAA::func = REL::Relocation<decltype(HookedRender_TAA::thunk)*>(oldFuncTAA);


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
bool Hook::mirror_firstClear = true;
