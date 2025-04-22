#include <synchapi.h>
#include <processthreadsapi.h>
#include "Hook.h"

#include "detours.h"
#include <d3d9.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d9.lib")

using namespace RE;
using namespace BSScript;
using namespace std;
using namespace RE::BSGraphics;
Hook* hook = nullptr;

// Our global variables
NiCamera* g_ScopeCamera = nullptr;
NiCamera* g_OriCamera = nullptr;

REL::Relocation<uintptr_t> DrawWorld_Render_PreUI_Ori{ REL::ID(984743) };
REL::Relocation<uintptr_t> DrawWorld_Begin_Ori{ REL::ID(502840) };
REL::Relocation<uintptr_t> DrawWorld_Forward_Ori{ REL::ID(656535) };
REL::Relocation<uintptr_t> DrawWorld_Refraction_Ori{ REL::ID(1572250) };

REL::Relocation<uintptr_t> Main_DrawWorldAndUI_Ori{ REL::ID(408683) };
REL::Relocation<uintptr_t> Main_Swap_Ori{ REL::ID(1075087) };

REL::Relocation<NiCamera**> ptr_DrawWorldCamera{ REL::ID(1444212) };
REL::Relocation<uintptr_t**	> ptr_DrawWorldShadowNode{ REL::ID(1327069) };
REL::Relocation<NiAVObject**> ptr_DrawWorld1stPerson{ REL::ID(1491228) };

REL::Relocation<uintptr_t> Renderer_CreateaRenderTarget_Ori{ REL::ID(425575) };
REL::Relocation<uintptr_t> RenderTargetManager_CreateaRenderTarget_Ori{ REL::ID(43433) };
REL::Relocation<BSShaderManagerState*> ptr_BSShaderManager_State{ REL::ID(1327069) };


// Flag to ensure we initialize only once
bool g_bInitialized = false;

void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld);
void __fastcall hkBegin(uint64_t ptr_drawWorld);
void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground);
void __fastcall hkMain_Swap();
RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties);
void __fastcall hkRTManager_CreateRenderTarget(int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent);
void hkDrawWorld_Refraction(uint64_t this_ptr);

std::vector<int> aidList; 

void Init_Hook()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)DrawWorld_Render_PreUI_Ori, hkRender_PreUI);
	DetourAttach(&(PVOID&)DrawWorld_Begin_Ori, hkBegin);
	DetourAttach(&(PVOID&)Main_DrawWorldAndUI_Ori, hkMain_DrawWorldAndUI);
	//DetourAttach(&(PVOID&)Main_Swap_Ori, hkMain_Swap);
	DetourAttach(&(PVOID&)DrawWorld_Refraction_Ori, hkDrawWorld_Refraction);
	DetourTransactionCommit();
}

static void SetCameraFov(float fov)
{
	using func_t = decltype(&SetCameraFov);
	static REL::Relocation<func_t> func{ REL::ID(1473890) };
	return func(fov);
}

static void SetCamera(NiCamera* cam)
{
	using func_t = decltype(&SetCamera);
	static REL::Relocation<func_t> func{ REL::ID(850430) };
	return func(cam);
}

static void SetUpdateCameraFOV(bool isUpdate)
{
	using func_t = decltype(&SetUpdateCameraFOV);
	static REL::Relocation<func_t> func{ REL::ID(1102729) };
	return func(isUpdate);
}

inline RE::NiNode* CreateBone(const char* name)
{
	RE::NiNode* newbone = new RE::NiNode(0);
	newbone->name = name;
	return newbone;
}

NiNode* InsertBone(NiAVObject* root, NiNode* node, const char* name)
{
	NiNode* parent = node->parent;
	NiNode* inserted = (NiNode*)root->GetObjectByName(name);
	if (!inserted) {
		inserted = CreateBone(name);
		//_MESSAGE("%s (%llx) created.", name, inserted);
		if (parent) {
			parent->AttachChild(inserted, true);
			inserted->parent = parent;
		} else {
			parent = node;
		}
		inserted->local.translate = NiPoint3();
		inserted->local.rotate.MakeIdentity();
		inserted->AttachChild(node, true);
		//_MESSAGE("%s (%llx) inserted to %s (%llx).", name, inserted, parent->name.c_str(), parent);
		return inserted;
	} else {
		if (!inserted->GetObjectByName(node->name)) {
			//_MESSAGE("%s (%llx) created.", name, inserted);
			if (parent) {
				parent->AttachChild(inserted, true);
				inserted->parent = parent;
			} else {
				parent = node;
			}
			inserted->AttachChild(node, true);
			return inserted;
		}
	}
	return nullptr;
}

void CreateScopeCamera()
{
	// Get the player camera
	auto playerCamera = RE::PlayerCamera::GetSingleton();

	// Create a clone of the player camera for our scope view
	g_ScopeCamera = new NiCamera();

	auto weaponNode = PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon");
	auto TCamInsert = InsertBone(weaponNode, (NiNode*)g_ScopeCamera, "TCamInsert");

	g_ScopeCamera->world.rotate.MakeIdentity();
	g_ScopeCamera->local.rotate.MakeIdentity();
	g_ScopeCamera->viewFrustum = ((NiCamera*)playerCamera->cameraRoot.get())->viewFrustum;
}

void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr)
{
	typedef void (*FnDrawWorld_Refraction)(uint64_t);
	FnDrawWorld_Refraction fn = (FnDrawWorld_Refraction)DrawWorld_Refraction_Ori.address();

	// Call original function first
	(*fn)(this_ptr);
}


RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties)
{
	typedef RenderTarget* (*hkRenderer_CreateRenderTarget)(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties);
	hkRenderer_CreateRenderTarget fn = (hkRenderer_CreateRenderTarget)Renderer_CreateaRenderTarget_Ori.address();
	if (!fn)
		return nullptr;
	return (*fn)(renderer, aId, apName, aProperties);
}

void __fastcall hkRTManager_CreateRenderTarget(int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent)
{
	typedef void (*hkRTManager_CreateaRenderTarget)(int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent);
	hkRTManager_CreateaRenderTarget fn = (hkRTManager_CreateaRenderTarget)RenderTargetManager_CreateaRenderTarget_Ori.address();
	if (!fn) return;
	(*fn)(aIndex, arProperties, aPersistent);
}

void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld)
{
	typedef void (*FnRender_PreUI)(uint64_t ptr_drawWorld);
	FnRender_PreUI fn = (FnRender_PreUI)DrawWorld_Render_PreUI_Ori.address();

	if (!fn) return;
	(*fn)(ptr_drawWorld);

	D3DPERF_BeginEvent(0xffffffff, L"hkRender_PreUI");
	NiCloningProcess tempP{};
	NiCamera* originalCamera = (NiCamera*)((*ptr_DrawWorldCamera)->CreateClone(tempP));


	if (g_ScopeCamera->parent) {
		SetCamera(g_ScopeCamera);
		(*fn)(ptr_drawWorld);
		SetCamera(originalCamera);
	}
	
	D3DPERF_EndEvent();
}

void __fastcall hkBegin(uint64_t ptr_drawWorld)
{
	typedef void (*hkBegin)(uint64_t ptr_drawWorld);
	hkBegin fn = (hkBegin)DrawWorld_Begin_Ori.address();
	if (!fn) return;
	(*fn)(ptr_drawWorld);
}

void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground)
{
	typedef void (*FnMain_DrawWorldAndUI)(uint64_t, bool);
	FnMain_DrawWorldAndUI fn = (FnMain_DrawWorldAndUI)Main_DrawWorldAndUI_Ori.address();
	if (!fn)
		return;
	(*fn)(ptr_drawWorld, abBackground);
}

void hkMain_Swap()
{
	typedef void (*hkMain_Swap)();
	hkMain_Swap fn = (hkMain_Swap)Main_Swap_Ori.address();

	if (!fn) return;
	(*fn)();
}

void InitScopeSystem()
{
	CreateScopeCamera();

	logger::info("Scope system initialized successfully");
}

DWORD WINAPI MainThread(HMODULE hModule) 
{
	while (!BSGraphics::RendererData::GetSingleton() 
		|| !BSGraphics::RendererData::GetSingleton()->renderWindow 
		|| !BSGraphics::RendererData::GetSingleton()->renderWindow->hwnd 
		|| !BSGraphics::RendererData::GetSingleton()->renderWindow->swapChain 
		|| !RE::BSGraphics::RendererData::GetSingleton()->device
		|| !RE::BSGraphics::RendererData::GetSingleton()->context
		|| !RE::PlayerCharacter::GetSingleton()
		|| !RE::PlayerCharacter::GetSingleton()->Get3D()
		|| !RE::PlayerControls::GetSingleton()
		|| !RE::PlayerCamera::GetSingleton()
		|| !RE::Main::WorldRootCamera()
		) {
		Sleep(10);
	}
	hook->HookDX11_Init();

	InitScopeSystem();

	return 0;
}

void InitializePlugin()
{
	Init_Hook();
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, (HMODULE)REX::W32::GetCurrentModule(), 0, NULL);
}


F4SE_EXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "TrueThroughScope";
	a_info->version = 1;

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}.{}.{}"), Version::PROJECT, Version::MAJOR, Version::MINOR, Version::PATCH);

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if ((REL::Module::IsF4() && ver < F4SE::RUNTIME_1_10_163) ||
		(REL::Module::IsVR() && ver < F4SE::RUNTIME_LATEST_VR)) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	logger::info("FakeThroughScope Loaded!");

	

	return true;
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
#ifdef _DEBUG
	while (!IsDebuggerPresent()) {
	}
	Sleep(1000);
#endif

	F4SE::Init(a_f4se);
	F4SE::AllocTrampoline(8 * 8);
	hook = new Hook(RE::PlayerCamera::GetSingleton());
	
	

	hook->InitRenderDoc();
	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kPostLoad) {
		} else if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		} else if (msg->type == F4SE::MessagingInterface::kPostLoadGame) {
			//ResetScopeStatus();
		} else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			//ResetScopeStatus();
		} else if (msg->type == F4SE::MessagingInterface::kPostSaveGame) {
			//reshadeImpl->SetRenderEffect(true);
		} else if (msg->type == F4SE::MessagingInterface::kGameLoaded) {
			//reshadeImpl->SetRenderEffect(false);
		}
	});

	return true;
}

F4SE_EXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.AuthorName(Version::AUTHOR);
	data.PluginName(Version::PROJECT);
	data.PluginVersion(Version::VERSION);

	data.UsesAddressLibrary(true);
	data.IsLayoutDependent(true);
	data.UsesSigScanning(false);
	data.HasNoStructUse(false);

	data.CompatibleVersions({ F4SE::RUNTIME_1_10_163 });
	return data;
}();
