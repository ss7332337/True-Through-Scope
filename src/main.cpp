#include <synchapi.h>
#include <processthreadsapi.h>
#include "Hook.h"

#include "detours.h"
#include <d3d9.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <winternl.h>

#pragma comment(lib, "d3d9.lib")

using namespace RE;
using namespace BSScript;
using namespace std;
using namespace RE::BSGraphics;
using namespace RE::DrawWorld;
Hook* hook = nullptr;

// Our global variables
NiCamera* g_ScopeCamera = nullptr;
NiCamera* g_OriCamera = nullptr;

RenderTargetManager* gRtMan;
Renderer gRenderer;
ID3D11Device* gDevice;
NiTexture* g_ScopeNiTexture = nullptr;

//NiPointer<NiTexture> g_ScopeNiTexture = nullptr;
Texture* g_ScopeBSTexture = nullptr;
bool g_ScopeRTInitialized = false;
int g_ScopeRTIndex = 100;
bool g_ModifyFirstPersonRender = false;

RenderTarget* g_ScopeRenderTarget = nullptr;
#pragma region Func
#pragma region DrawWorld_MainRenderFn
	REL::Relocation<uintptr_t> DrawWorld_Render_PreUI_Ori{ REL::ID(984743) };
	REL::Relocation<uintptr_t> DrawWorld_MainAccum_Ori{ REL::ID(718911) };
	REL::Relocation<uintptr_t> DrawWorld_OcclusionMapRender_Ori{ REL::ID(426737) };
	REL::Relocation<uintptr_t> DrawWorld_MainRenderSetup_Ori{ REL::ID(339369) };
	REL::Relocation<uintptr_t> DrawWorld_OpaqueWireframe_Ori{ REL::ID(1268987) };
	REL::Relocation<uintptr_t> DrawWorld_DeferredPrePass_Ori{ REL::ID(56596) };
	REL::Relocation<uintptr_t> DrawWorld_DeferredLightsImpl_Ori{ REL::ID(1108521) };
	REL::Relocation<uintptr_t> DrawWorld_DeferredComposite_Ori{ REL::ID(728427) };
	REL::Relocation<uintptr_t> DrawWorld_Forward_Ori{ REL::ID(656535) };
	REL::Relocation<uintptr_t> DrawWorld_Refraction_Ori{ REL::ID(1572250) };
#pragma endregion


REL::Relocation<uintptr_t> DrawWorld_Begin_Ori{ REL::ID(502840) };

REL::Relocation<uintptr_t> DrawWorld_Add1stPersonGeomToCuller_Ori{ REL::ID(414086) };
REL::Relocation<uintptr_t> BSCullingGroup_Process_Ori{ REL::ID(1147875) };

REL::Relocation<uintptr_t> BSShaderAccumulator_RenderBatches_Ori{ REL::ID(1048494) };
REL::Relocation<uintptr_t> BSShaderAccumulator_RenderOpaqueDecals_Ori{ REL::ID(163409) };
REL::Relocation<uintptr_t> BSShaderAccumulator_RenderBlendedDecals_Ori{ REL::ID(761249) };


REL::Relocation<uintptr_t> Main_DrawWorldAndUI_Ori{ REL::ID(408683) };
REL::Relocation<uintptr_t> Main_Swap_Ori{ REL::ID(1075087) };

REL::Relocation<uintptr_t> Renderer_CreateaRenderTarget_Ori{ REL::ID(425575) };
REL::Relocation<uintptr_t> RenderTargetManager_CreateaRenderTarget_Ori{ REL::ID(43433) };
#pragma endregion

#pragma region Pointer
REL::Relocation<uintptr_t**> ptr_DrawWorldShadowNode{ REL::ID(1327069) };
REL::Relocation<NiAVObject**> ptr_DrawWorld1stPerson{ REL::ID(1491228) };
REL::Relocation<BSShaderManagerState*> ptr_BSShaderManager_State{ REL::ID(1327069) };
REL::Relocation<bool*> ptr_DrawWorld_b1stPersonEnable{ REL::ID(922366) };
//REL::Relocation<bool*> ptr_DrawWorld_b1stPersonInWorld{ REL::ID(34473) };
REL::Relocation<BSShaderAccumulator**> ptr_Draw1stPersonAccum{ REL::ID(1430301) };
REL::Relocation<BSShaderAccumulator**> ptr_DrawWorldAccum{ REL::ID(1211381) };
REL::Relocation<BSCullingGroup**> ptr_k1stPersonCullingGroup{ REL::ID(731482) };
REL::Relocation<NiCamera**> ptr_BSShaderManagerSpCamera{ REL::ID(543218) };
REL::Relocation<NiCamera**> ptr_DrawWorldCamera{ REL::ID(1444212) };
REL::Relocation<NiCamera**> ptr_DrawWorldSpCamera{ REL::ID(543218) };
REL::Relocation<Context**> ptr_DefaultContext{ REL::ID(33539) };
REL::Relocation<uint32_t*> ptr_tls_index{ REL::ID(842564) };
#pragma endregion



REL::Relocation<uintptr_t> RenderTargetManager_SetCurrentRenderTarget_Ori{ REL::ID(1502425) };

// Flag to ensure we initialize only once
bool g_bInitialized = false;
bool g_IsRenderingForScope = false;
bool g_OverrideFirstPersonCulling = false;

bool g_AdjustmentMode = false;
float g_AdjustmentSpeed = 1.0f;
NiPoint3 deltaPos = {0,0,0};
NiMatrix3 deltaRot;
int g_targetFov = 90;
enum AdjustmentTarget
{
	POSITION,
	ROTATION
};
AdjustmentTarget g_CurrentTarget = POSITION;
int g_CurrentAxis = 0;  // 0 = X, 1 = Y, 2 = Z

void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld);
void __fastcall hkBegin(uint64_t ptr_drawWorld);
void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground);
void __fastcall hkMain_Swap();
RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties);
void __fastcall hkRTManager_CreateRenderTarget(int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent);

void __fastcall hkSetCurrentRenderTarget(RenderTargetManager* manager,int aIndex,int aRenderTarget,SetRenderTargetMode aMode);

void __fastcall hkBSCullingGroup_Process(BSCullingGroup* thisPtr, bool someFlag);

void __fastcall hkAdd1stPersonGeomToCuller(uint64_t ptr_drawWorld);
void __fastcall hkBSShaderAccumulator_RenderBatches(BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup);
void __fastcall hkBSShaderAccumulator_RenderOpaqueDecals(BSShaderAccumulator* thisPtr);
void __fastcall hkBSShaderAccumulator_RenderBlendedDecals(BSShaderAccumulator* thisPtr);
//in Render_PreUI
void __fastcall hkMainAccum(uint64_t ptr_drawWorld);
void __fastcall hkOcclusionMapRender();
void __fastcall hkMainRenderSetup(uint64_t ptr_drawWorld);
void __fastcall hkOpaqueWireframe(uint64_t ptr_drawWorld);
void __fastcall hkDeferredPrePass(uint64_t ptr_drawWorld);
void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld);
void __fastcall hkDeferredComposite(uint64_t ptr_drawWorld);
void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld);
void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr);


void Init_Hook()
{
	gRtMan = RenderTargetManager::GetSingleton();
	gRenderer = Renderer::GetSingleton();

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)DrawWorld_Render_PreUI_Ori, hkRender_PreUI);
	DetourAttach(&(PVOID&)DrawWorld_Begin_Ori, hkBegin);
	DetourAttach(&(PVOID&)Main_DrawWorldAndUI_Ori, hkMain_DrawWorldAndUI);
	DetourAttach(&(PVOID&)Main_Swap_Ori, hkMain_Swap);
	DetourAttach(&(PVOID&)BSCullingGroup_Process_Ori, hkBSCullingGroup_Process);

	DetourAttach(&(PVOID&)DrawWorld_MainAccum_Ori, hkMainAccum);
	DetourAttach(&(PVOID&)DrawWorld_OcclusionMapRender_Ori, hkOcclusionMapRender);
	DetourAttach(&(PVOID&)DrawWorld_MainRenderSetup_Ori, hkMainRenderSetup);
	DetourAttach(&(PVOID&)DrawWorld_OpaqueWireframe_Ori, hkOpaqueWireframe);
	DetourAttach(&(PVOID&)DrawWorld_DeferredPrePass_Ori, hkDeferredPrePass);
	DetourAttach(&(PVOID&)DrawWorld_DeferredLightsImpl_Ori, hkDeferredLightsImpl);
	DetourAttach(&(PVOID&)DrawWorld_DeferredComposite_Ori, hkDeferredComposite);
	DetourAttach(&(PVOID&)DrawWorld_Forward_Ori, hkDrawWorld_Forward);
	DetourAttach(&(PVOID&)DrawWorld_Refraction_Ori, hkDrawWorld_Refraction);
	
	DetourAttach(&(PVOID&)DrawWorld_Add1stPersonGeomToCuller_Ori, hkAdd1stPersonGeomToCuller);
	DetourAttach(&(PVOID&)BSShaderAccumulator_RenderBatches_Ori, hkBSShaderAccumulator_RenderBatches);
	DetourAttach(&(PVOID&)BSShaderAccumulator_RenderOpaqueDecals_Ori, hkBSShaderAccumulator_RenderOpaqueDecals);
	DetourAttach(&(PVOID&)BSShaderAccumulator_RenderBlendedDecals_Ori, hkBSShaderAccumulator_RenderBlendedDecals);
	DetourTransactionCommit();

}

void UpdateCameraWorldTransform()
{
	if (!g_ScopeCamera || !g_ScopeCamera->parent)
		return;

	// 强制更新世界变换矩阵
	NiUpdateData updateData;
	g_ScopeCamera->UpdateWorldData(&updateData);

	// 检查是否有特定位置要覆盖
	if (g_ScopeCamera->local.translate.x != 0.0f ||
		g_ScopeCamera->local.translate.y != 0.0f ||
		g_ScopeCamera->local.translate.z != 0.0f) {
		// 计算父节点世界位置上添加本地位置的效果
		NiPoint3 worldPos;
		NiMatrix3 parentRotation = g_ScopeCamera->parent->world.rotate;

		// 将本地位置通过父节点旋转转换到世界空间
		worldPos.x = parentRotation.entry[0].x * g_ScopeCamera->local.translate.x +
		             parentRotation.entry[0].y * g_ScopeCamera->local.translate.y +
		             parentRotation.entry[0].z * g_ScopeCamera->local.translate.z;

		worldPos.y = parentRotation.entry[1].x * g_ScopeCamera->local.translate.x +
		             parentRotation.entry[1].y * g_ScopeCamera->local.translate.y +
		             parentRotation.entry[1].z * g_ScopeCamera->local.translate.z;

		worldPos.z = parentRotation.entry[2].x * g_ScopeCamera->local.translate.x +
		             parentRotation.entry[2].y * g_ScopeCamera->local.translate.y +
		             parentRotation.entry[2].z * g_ScopeCamera->local.translate.z;

		// 添加父节点世界位置
		worldPos += g_ScopeCamera->parent->world.translate;

		// 设置新的世界位置
		g_ScopeCamera->world.translate = worldPos;
	}
}

void ProcessCameraAdjustment()
{
	// Handle adjustment mode toggle (F3 key)
	if (GetAsyncKeyState(VK_NUMPAD0) & 0x1) {
		g_AdjustmentMode = !g_AdjustmentMode;
		logger::info("Camera adjustment mode: {}", g_AdjustmentMode ? "ON" : "OFF");
		return;
	}

	// Skip if not in adjustment mode or scope camera doesn't exist
	if (!g_AdjustmentMode || !g_ScopeCamera || !g_ScopeCamera->parent)
		return;

	// Get keyboard state
	SHORT keyUp = GetAsyncKeyState(VK_UP);
	SHORT keyDown = GetAsyncKeyState(VK_DOWN);
	SHORT keyLeft = GetAsyncKeyState(VK_LEFT);
	SHORT keyRight = GetAsyncKeyState(VK_RIGHT);
	SHORT keyPageUp = GetAsyncKeyState(VK_PRIOR);
	SHORT keyPageDown = GetAsyncKeyState(VK_NEXT);
	SHORT keyPage9 = GetAsyncKeyState(VK_NUMPAD9);
	SHORT keyPage3 = GetAsyncKeyState(VK_NUMPAD3);

	// Toggle between position and rotation adjustment (F4 key)
	if (GetAsyncKeyState(VK_DIVIDE) & 0x1) {
		g_CurrentTarget = (g_CurrentTarget == POSITION) ? ROTATION : POSITION;
		logger::info("Adjusting camera {}", (g_CurrentTarget == POSITION) ? "POSITION" : "ROTATION");
		return;
	}

	// Switch axis (F5 key)
	if (GetAsyncKeyState(VK_MULTIPLY) & 0x1) {
		g_CurrentAxis = (g_CurrentAxis + 1) % 3;
		const char* axisNames[] = { "X", "Y", "Z" };
		logger::info("Current axis: {}", axisNames[g_CurrentAxis]);
		return;
	}

	// Adjust speed (F6/F7 keys)
	if (GetAsyncKeyState(VK_OEM_MINUS) & 0x1) {
		g_AdjustmentSpeed /= 2.0f;
		logger::warn("Adjustment speed: {}", g_AdjustmentSpeed);
	}
	if (GetAsyncKeyState(VK_OEM_PLUS) & 0x1) {
		g_AdjustmentSpeed *= 2.0f;
		logger::warn("Adjustment speed: {}", g_AdjustmentSpeed);
	}


	if (keyPage9 & 0x8000)
		g_targetFov += 1;
	if (keyPage3 & 0x8000)
		g_targetFov -= 1;


	 // Apply adjustments based on target (position or rotation)
	if (g_CurrentTarget == POSITION) {
		// Adjust position on all axes simultaneously
		

		// X-axis (left/right)
		if (keyRight & 0x8000)
			deltaPos.x += g_AdjustmentSpeed;
		if (keyLeft & 0x8000)
			deltaPos.x -= g_AdjustmentSpeed;

		// Y-axis (up/down)
		if (keyUp & 0x8000)
			deltaPos.y += g_AdjustmentSpeed;
		if (keyDown & 0x8000)
			deltaPos.y -= g_AdjustmentSpeed;

		// Z-axis (page up/down)
		if (keyPageUp & 0x8000)
			deltaPos.z += g_AdjustmentSpeed;
		if (keyPageDown & 0x8000)
			deltaPos.z -= g_AdjustmentSpeed;

		// Apply position changes if any
		if (deltaPos.x != 0.0f || deltaPos.y != 0.0f || deltaPos.z != 0.0f) {
			g_ScopeCamera->local.translate += deltaPos;

			logger::info("Camera position: [{:.3f}, {:.3f}, {:.3f}]",
				g_ScopeCamera->local.translate.x,
				g_ScopeCamera->local.translate.y,
				g_ScopeCamera->local.translate.z);
		}
	} else {
		// Adjust rotation on all axes simultaneously
		NiMatrix3 rotMat = g_ScopeCamera->local.rotate;
		bool hasRotation = false;

		// Create rotation matrices for each axis
		NiMatrix3 xRotMat, yRotMat, zRotMat;
		xRotMat.MakeIdentity();
		yRotMat.MakeIdentity();
		zRotMat.MakeIdentity();

		// X-axis rotation (up/down)
		float xAngle = 0.0f;
		if (keyPageUp & 0x8000)
			xAngle += g_AdjustmentSpeed * 0.01f;
		if (keyPageDown & 0x8000)
			xAngle -= g_AdjustmentSpeed * 0.01f;
		if (xAngle != 0.0f) {
			xRotMat.FromEulerAnglesXYZ(xAngle, 0.0f, 0.0f);
			hasRotation = true;
		}

		// Y-axis rotation (left/right)
		float yAngle = 0.0f;
		if (keyRight & 0x8000)
			yAngle += g_AdjustmentSpeed * 0.01f;
		if (keyLeft & 0x8000)
			yAngle -= g_AdjustmentSpeed * 0.01f;
		if (yAngle != 0.0f) {
			yRotMat.FromEulerAnglesXYZ(0.0f, yAngle, 0.0f);
			hasRotation = true;
		}

		// Z-axis rotation (page up/down)
		float zAngle = 0.0f;
		if (keyUp & 0x8000)
			zAngle += g_AdjustmentSpeed * 0.01f;
		if (keyDown & 0x8000)
			zAngle -= g_AdjustmentSpeed * 0.01f;
		if (zAngle != 0.0f) {
			zRotMat.FromEulerAnglesXYZ(0.0f, 0.0f, zAngle);
			hasRotation = true;
		}

		// Apply all rotations in sequence if any changes
		if (hasRotation) {
			// Order matters for rotations, applying X, then Y, then Z
			deltaRot = zRotMat * yRotMat * xRotMat;
			g_ScopeCamera->local.rotate = deltaRot * rotMat;

			// Get Euler angles for display
			float pitch, yaw, roll;
			g_ScopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			logger::info("Camera rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);
		}
	}

		NiUpdateData tempData{};
		tempData.camera = g_ScopeCamera;
		// Update the camera's world transform
		g_ScopeCamera->Update(tempData);

	// Print current values (F8 key)
	if (GetAsyncKeyState(VK_F8) & 0x1) {
		float pitch, yaw, roll;
		g_ScopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);

		logger::info("[Print current values]");
		logger::info("Camera local position: [{:.3f}, {:.3f}, {:.3f}]",
			g_ScopeCamera->local.translate.x,
			g_ScopeCamera->local.translate.y,
			g_ScopeCamera->local.translate.z);
		logger::info("Camera world position: [{:.3f}, {:.3f}, {:.3f}]",
			g_ScopeCamera->world.translate.x,
			g_ScopeCamera->world.translate.y,
			g_ScopeCamera->world.translate.z);
		logger::info("Camera local rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);
		g_ScopeCamera->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
		logger::info("Camera world rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);
		logger::info("[Print End]");
	}

	// Reset camera (F9 key)
	if (GetAsyncKeyState(VK_F9) & 0x1) {
		g_ScopeCamera->local.translate = NiPoint3();
		g_ScopeCamera->local.rotate.MakeIdentity();
		NiUpdateData tempData{};
		tempData.camera = g_ScopeCamera;
		g_ScopeCamera->Update(tempData);
		logger::info("Camera position/rotation reset");
	}
}

static void BSCullingGroupCleanup(BSCullingGroup* thisptr, bool abCleanP1, bool abCleanP2)
{
	using func_t = decltype(&BSCullingGroupCleanup);
	static REL::Relocation<func_t> func{ REL::ID(1102729) };
	return func(thisptr, abCleanP1, abCleanP2);
}

void CreateScopeCamera()
{
	// Get the player camera
	const auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	//g_Swapchain = (IDXGISwapChain*)static_cast<void*>(rendererData->renderWindow->swapChain);
	gDevice = (ID3D11Device*)static_cast<void*>(rendererData->device);

	auto playerCamera = RE::PlayerCamera::GetSingleton();

	// Create a clone of the player camera for our scope view
	g_ScopeCamera = new NiCamera();
	auto weaponNode = PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon");
	playerCamera->cameraRoot.get()->AttachChild(g_ScopeCamera, true);
	g_ScopeCamera->viewFrustum = ((NiCamera*)playerCamera->cameraRoot.get())->viewFrustum;
	g_ScopeCamera->port = ((NiCamera*)playerCamera->cameraRoot.get())->port;
}

void RenderForScope(uint64_t ptr_drawWorld)
{
	// Save original state
	g_IsRenderingForScope = true;

	// Call the render function
	typedef void (*FnRender_PreUI)(uint64_t);
	FnRender_PreUI fn = (FnRender_PreUI)DrawWorld_Render_PreUI_Ori.address();
	(*fn)(ptr_drawWorld);

	g_IsRenderingForScope = false;
}

void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr)
{
	if (g_IsRenderingForScope) {
		//Renderer::GetSingleton().ClearDepthStencil(ClearFlag::CLEAR_DEPTH_STENCIL_TARGET_ALL);
		return;
	}
	if (g_IsRenderingForScope) {
		// We'll let the culling happen, but we'll skip actual rendering later
		g_ModifyFirstPersonRender = true;
	}

	typedef void (*FnhkAdd1stPersonGeomToCuller)(uint64_t);
	FnhkAdd1stPersonGeomToCuller fn = (FnhkAdd1stPersonGeomToCuller)DrawWorld_Add1stPersonGeomToCuller_Ori.address();
	if (!fn) return;
	(*fn)(thisPtr);
	return;
}

void __fastcall hkBSShaderAccumulator_RenderBatches(
	BSShaderAccumulator* thisPtr,int aiShader,bool abAlphaPass,int aeGroup)
{
	// Original function pointer
	typedef void (*FnRenderBatches)(BSShaderAccumulator*, int, bool, int);
	FnRenderBatches fn = (FnRenderBatches)BSShaderAccumulator_RenderBatches_Ori.address();

	if (g_IsRenderingForScope && thisPtr == *ptr_Draw1stPersonAccum) {
		(*fn)(thisPtr, aiShader, abAlphaPass, aeGroup);
		return;
	}
	if(!fn) return;
	(*fn)(thisPtr, aiShader, abAlphaPass, aeGroup);
}


void __fastcall hkBSShaderAccumulator_RenderBlendedDecals(BSShaderAccumulator* thisPtr)
{
	typedef void (*Fn)(BSShaderAccumulator*);
	Fn fn = (Fn)BSShaderAccumulator_RenderBlendedDecals_Ori.address();
	if (g_IsRenderingForScope && thisPtr == *ptr_Draw1stPersonAccum) {
		(*fn)(thisPtr);
		return;
	}
	if(!fn) return;
	(*fn)(thisPtr);
}

void __fastcall hkBSShaderAccumulator_RenderOpaqueDecals(BSShaderAccumulator* thisPtr)
{
	typedef void (*Fn)(BSShaderAccumulator*);
	Fn fn = (Fn)BSShaderAccumulator_RenderOpaqueDecals_Ori.address();
	if (g_IsRenderingForScope && thisPtr == *ptr_Draw1stPersonAccum) {
		(*fn)(thisPtr);
		return;
	}
	if(!fn) return;
	(*fn)(thisPtr);
}

void __fastcall hkBSCullingGroup_Process(BSCullingGroup* thisPtr, bool someFlag)
{
	typedef void (*FnBSCullingGroup_Process)(BSCullingGroup*, bool);
	FnBSCullingGroup_Process fn = (FnBSCullingGroup_Process)BSCullingGroup_Process_Ori.address();
	(*fn)(thisPtr, someFlag);
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
	D3DPERF_BeginEvent(0xffffffff, L"First Render_PreUI");
	(*fn)(ptr_drawWorld);
	D3DPERF_EndEvent();

	if (!g_ScopeCamera || !g_ScopeCamera->parent)
		return;

	NiCloningProcess tempP{};
	NiCamera* originalCamera = (NiCamera*)((*ptr_DrawWorldCamera)->CreateClone(tempP));
	g_ScopeCamera->local.translate = originalCamera->local.translate;
	g_ScopeCamera->local.rotate = originalCamera->local.rotate;
	ProcessCameraAdjustment();

	
	D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");
	DrawWorld::SetCamera(g_ScopeCamera);
	DrawWorld::SetUpdateCameraFOV(true);
	DrawWorld::SetAdjusted1stPersonFOV(g_targetFov);
	DrawWorld::SetCameraFov(g_targetFov);

	//(*ptr_DrawWorld_b1stPersonEnable) = false;

	RenderForScope(ptr_drawWorld);

	DrawWorld::SetCamera(originalCamera);
	DrawWorld::SetUpdateCameraFOV(true);
	DrawWorld::SetAdjusted1stPersonFOV(90);
	DrawWorld::SetCameraFov(90);
	D3DPERF_EndEvent();
}

void __fastcall hkMainAccum(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_MainAccum_Ori.address();
	if (g_IsRenderingForScope) {
		(*fn)(ptr_drawWorld);
		auto accum = *ptr_Draw1stPersonAccum;
		return;
	}

	(*fn)(ptr_drawWorld);
}

void __fastcall hkOcclusionMapRender()
{
	typedef void (*Fn)();
	Fn fn = (Fn)DrawWorld_OcclusionMapRender_Ori.address();
	if (g_IsRenderingForScope) {
		D3DPERF_BeginEvent(0xffffffff, L"hkOcclusionMapRender");
		(*fn)();
		D3DPERF_EndEvent();
		return;
	}

	(*fn)();
}

void __fastcall hkMainRenderSetup(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_MainRenderSetup_Ori.address();

	if (g_IsRenderingForScope) {
		D3DPERF_BeginEvent(0xffffffff, L"hkMainRenderSetup");
		(*fn)(ptr_drawWorld);
		D3DPERF_EndEvent();
		return;
	}
	(*fn)(ptr_drawWorld);
}

void __fastcall hkOpaqueWireframe(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_OpaqueWireframe_Ori.address();

	if (g_IsRenderingForScope) {
		(*fn)(ptr_drawWorld);
		return;
	}
	(*fn)(ptr_drawWorld);
}


void __fastcall hkDeferredPrePass(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_DeferredPrePass_Ori.address();
	// Check if we're rendering for scope and this is the 1st person accumulator

	bool wasInScopeRendering = g_IsRenderingForScope;

	// Set render state to write depth only
	D3DPERF_BeginEvent(0xffffffff, L"hkDeferredPrePass");
	(*fn)(ptr_drawWorld);

	if (wasInScopeRendering) {
		// Ensure the flag is reset before we leave
		g_ModifyFirstPersonRender = false;
	}

	D3DPERF_EndEvent();

	(*fn)(ptr_drawWorld);
}

void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_DeferredLightsImpl_Ori.address();
	if (g_IsRenderingForScope) {
		D3DPERF_BeginEvent(0xffffffff, L"hkDeferredLightsImpl");
		(*fn)(ptr_drawWorld);
		D3DPERF_EndEvent();
		return;
	}

	(*fn)(ptr_drawWorld);
}

void __fastcall hkDeferredComposite(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_DeferredComposite_Ori.address();

	if (g_IsRenderingForScope) {
		D3DPERF_BeginEvent(0xffffffff, L"hkDeferredComposite");
		(*fn)(ptr_drawWorld);
		D3DPERF_EndEvent();
		return;
	}
	(*fn)(ptr_drawWorld);
}

void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_Forward_Ori.address();

	if (g_IsRenderingForScope) {
		D3DPERF_BeginEvent(0xffffffff, L"hkDrawWorld_Forward");
		(*fn)(ptr_drawWorld);
		D3DPERF_EndEvent();
		return;
	}
	(*fn)(ptr_drawWorld);
}

void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr)
{
	typedef void (*Fn)(uint64_t);
	Fn fn = (Fn)DrawWorld_Refraction_Ori.address();
	if (g_IsRenderingForScope) {
		D3DPERF_BeginEvent(0xffffffff, L"hkDrawWorld_Refraction");
		(*fn)(this_ptr);
		D3DPERF_EndEvent();
		return;
	}
	(*fn)(this_ptr);
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

void __fastcall hkSetCurrentRenderTarget(
	RenderTargetManager* manager,
	int aIndex,
	int aRenderTarget,
	SetRenderTargetMode aMode)
{
	typedef void (*FnSetCurrentRenderTarget)(RenderTargetManager*, int, int, SetRenderTargetMode);
	FnSetCurrentRenderTarget fn = (FnSetCurrentRenderTarget)RenderTargetManager_SetCurrentRenderTarget_Ori.address();
	if (!fn) return;
	(*fn)(manager, aIndex, aRenderTarget, aMode);
}

void hkMain_Swap()
{
	typedef void (*hkMain_Swap)();
	hkMain_Swap fn = (hkMain_Swap)Main_Swap_Ori.address();

	if (!fn) return;
	(*fn)();
}

DWORD WINAPI MainThread(HMODULE hModule) 
{
	while (!RE::PlayerCharacter::GetSingleton()
		|| !RE::PlayerCharacter::GetSingleton()->Get3D()
		|| !RE::PlayerControls::GetSingleton()
		|| !RE::PlayerCamera::GetSingleton()
		|| !RE::Main::WorldRootCamera()
		) 
	{
		Sleep(10);
	}
	hook->HookDX11_Init();
	CreateScopeCamera();

	return 0;
}

void InitializePlugin()
{
	//InitScopeRenderTargetDirect();
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
