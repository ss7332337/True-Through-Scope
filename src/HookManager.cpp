#include "HookManager.h"
#include "Utilities.h"

#include <MinHook.h>
#include <thread>
#include <chrono>

namespace ThroughScope
{
	using namespace Utilities;

	// Forward declarations for hook functions
	extern void __fastcall hkTAA(ImageSpaceEffectTemporalAA* thisPtr, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param);
	extern void hkDrawWorld_Move1stPersonToOrigin(uint64_t thisPtr);
	extern void hkBSBatchRenderer_Draw(BSRenderPass* apRenderPass);
	extern void hkBSCullingGroup_SetCompoundFrustum(BSCullingGroup* thisPtr, BSCompoundFrustum* apCompoundFrustum);
	extern void hkBSCullingGroupAdd(BSCullingGroup* thisPtr, NiAVObject* apObj, const NiBound* aBound, const unsigned int aFlags);
	extern void hkDrawTriShape(BSGraphics::Renderer* thisPtr, BSGraphics::TriShape* apTriShape, unsigned int auiStartIndex, unsigned int auiNumTriangles);
	extern void hkBSStreamLoad(BSStream* stream, const char* apFileName, NiBinaryStream* apStream);
	extern void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld);
	extern void __fastcall hkRenderZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::ZPrePassDrawData* aZPreData, unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode);
	extern void __fastcall hkRenderAlphaTestZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::AlphaTestZPrePassDrawData* aZPreData, unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode, ID3D11SamplerState** aCurSamplerState);
	extern void __fastcall hkRenderer_DoZPrePass(uint64_t thisPtr, NiCamera* apFirstPersonCamera, NiCamera* apWorldCamera, float afFPNear, float afFPFar, float afNear, float afFar);
	extern void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr);
	extern void __fastcall hkBSShaderAccumulator_RenderBatches(BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup);
	extern void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld);
	extern void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld);
	extern void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr);
	extern void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground);
	extern void __fastcall hkBSDistantObjectInstanceRenderer_Render(uint64_t thisPtr);
	extern void __fastcall hkBSShaderAccumulator_ResetSunOcclusion(BSShaderAccumulator* thisPtr);
	extern void __fastcall hkDecompressDepthStencilTarget(RenderTargetManager* thisPtr, int index);
	extern void __fastcall hkMainAccum(uint64_t ptr_drawWorld);
	extern void __fastcall hkOcclusionMapRender();
	extern void __fastcall hkMainRenderSetup(uint64_t ptr_drawWorld);
	extern void __fastcall hkOpaqueWireframe(uint64_t ptr_drawWorld);
	extern void __fastcall hkDeferredPrePass(uint64_t ptr_drawWorld);
	extern void __fastcall hkDeferredComposite(uint64_t ptr_drawWorld);
	extern RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties);
	extern void __fastcall hkMainPreRender(Main* thisPtr, int auiDestination);
	extern void __fastcall hkBegin(uint64_t ptr_drawWorld);
	extern void hkMapDynamicTriShapeDynamicData(Renderer* renderer, BSDynamicTriShape* bsDynamicTriShape, DynamicTriShape* dynamicTriShape, DynamicTriShapeDrawData* drawdata, unsigned int auiSize);
	extern void __fastcall hkDrawWorld_LightUpdate(uint64_t ptr_drawWorld);
	extern void __fastcall hkBSShaderAccumulator_RenderBlendedDecals(BSShaderAccumulator* thisPtr);
	extern void __fastcall hkBSShaderAccumulator_RenderOpaqueDecals(BSShaderAccumulator* thisPtr);
	extern void __fastcall hkBSCullingGroup_Process(BSCullingGroup* thisPtr, bool abFirstStageOnly);
	extern void __fastcall hkRTManager_CreateRenderTarget(RenderTargetManager rtm, int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent);
	extern void __fastcall hkPCUpdateMainThread(PlayerCharacter* pChar);
extern void __fastcall hkBSSkyShader_SetupGeometry(void* thisPtr, BSRenderPass* apCurrentPass);
	extern void __fastcall hkImageSpaceManager_RenderEffectRange(void* thisPtr, int aiFirst, int aiLast, int aiSourceTarget, int aiDestTarget);
	extern void __fastcall hkDrawWorld_Render_UI(uint64_t thisPtr);
	extern void __fastcall hkUI_BeginRender();
	extern void __fastcall hkSetUseDynamicResolutionViewport(void* thisPtr, bool a_useDynamicResolution);
#include "RE/Bethesda/TaskQueueInterface.hpp"
	
	HookManager::FnWaitForJobs HookManager::g_WaitForThreads = nullptr;
	HookManager::FnProcessQueues HookManager::g_ProcessQueues = nullptr;

	void HookManager::RegisterAllHooks()
	{
		logger::info("Registering hooks...");

		// REL::ID(790315) - JobListManager::WaitForThreads
		REL::Relocation<FnWaitForJobs> WaitForThreads(REL::ID(790315));
		g_WaitForThreads = WaitForThreads.get();
		
		// REL::ID(882638) - TaskQueueInterface::ProcessQueues
		// Used to flush background tasks on the main thread to prevent race conditions during scene traversal
		REL::Relocation<FnProcessQueues> ProcessQueues(REL::ID(882638));
		g_ProcessQueues = ProcessQueues.get();
		
		logger::info("Locating sync functions...");
		logger::info("  WaitForThreads: {:X}", (uintptr_t)g_WaitForThreads);
		logger::info("  ProcessQueues: {:X}", (uintptr_t)g_ProcessQueues);

		CreateAndEnableHook((LPVOID)DrawWorld_LightUpdate_Ori.address(), &hkDrawWorld_LightUpdate, reinterpret_cast<LPVOID*>(&g_DrawWorldLightUpdateOriginal), "DrawWorld_LightUpdate");

		CreateAndEnableHook((LPVOID)Renderer_DoZPrePass_Ori.address(), &hkRenderer_DoZPrePass, reinterpret_cast<LPVOID*>(&g_pDoZPrePassOriginal), "DoZPrePass");
		CreateAndEnableHook((LPVOID)BSGraphics_RenderZPrePass_Ori.address(), &hkRenderZPrePass, reinterpret_cast<LPVOID*>(&g_RenderZPrePassOriginal), "RenderZPrePass");

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_ResetSunOcclusion_Ori.address(), &hkBSShaderAccumulator_ResetSunOcclusion,
			reinterpret_cast<LPVOID*>(&g_ResetSunOcclusionOriginal), "ResetSunOcclusion");

		CreateAndEnableHook((LPVOID)DrawWorld_Render_PreUI_Ori.address(), &hkRender_PreUI,
			reinterpret_cast<LPVOID*>(&g_RenderPreUIOriginal), "Render_PreUI");

		CreateAndEnableHook((LPVOID)DrawWorld_Begin_Ori.address(), &hkBegin,
			reinterpret_cast<LPVOID*>(&g_BeginOriginal), "Begin");

		CreateAndEnableHook((LPVOID)Main_DrawWorldAndUI_Ori.address(), &hkMain_DrawWorldAndUI,
			reinterpret_cast<LPVOID*>(&g_DrawWorldAndUIOriginal), "Main_DrawWorldAndUI");

		/*CreateAndEnableHook((LPVOID)DrawWorld_MainAccum_Ori.address(), &hkMainAccum,
			reinterpret_cast<LPVOID*>(&g_MainAccumOriginal), "MainAccum");*/

		CreateAndEnableHook((LPVOID)DrawWorld_MainRenderSetup_Ori.address(), &hkMainRenderSetup,
			reinterpret_cast<LPVOID*>(&g_MainRenderSetupOriginal), "MainRenderSetup");

		CreateAndEnableHook((LPVOID)DrawWorld_DeferredLightsImpl_Ori.address(), &hkDeferredLightsImpl,
			reinterpret_cast<LPVOID*>(&g_DeferredLightsImplOriginal), "DeferredLightsImpl");

		CreateAndEnableHook((LPVOID)DrawWorld_Forward_Ori.address(), &hkDrawWorld_Forward,
			reinterpret_cast<LPVOID*>(&g_ForwardOriginal), "DrawWorld_Forward");

		CreateAndEnableHook((LPVOID)DrawWorld_Refraction_Ori.address(), &hkDrawWorld_Refraction,
			reinterpret_cast<LPVOID*>(&g_RefractionOriginal), "DrawWorld_Refraction");

		CreateAndEnableHook((LPVOID)DrawWorld_Add1stPersonGeomToCuller_Ori.address(), &hkAdd1stPersonGeomToCuller,
			reinterpret_cast<LPVOID*>(&g_Add1stPersonGeomToCullerOriginal), "Add1stPersonGeomToCuller");

		CreateAndEnableHook((LPVOID)BSBatchRenderer_Draw_Ori.address(), &hkBSBatchRenderer_Draw,
			reinterpret_cast<LPVOID*>(&g_originalBSBatchRendererDraw), "BSBatchRenderer_Draw");

		CreateAndEnableHook((LPVOID)PCUpdateMainThread_Ori.address(), &hkPCUpdateMainThread,
			reinterpret_cast<LPVOID*>(&g_PCUpdateMainThread), "PCUpdateMainThread");

		CreateAndEnableHook((LPVOID)MainPreRender_Ori.address(), &hkMainPreRender,
			reinterpret_cast<LPVOID*>(&g_MainPreRender), "MainPreRender");

		CreateAndEnableHook((LPVOID)BSCullingGroupAdd_Ori.address(), &hkBSCullingGroupAdd,
			reinterpret_cast<LPVOID*>(&g_BSCullingGroupAdd), "BSCullingGroupAdd");

		CreateAndEnableHook((LPVOID)BSCullingGroup_SetCompoundFrustum_Ori.address(), &hkBSCullingGroup_SetCompoundFrustum,
			reinterpret_cast<LPVOID*>(&g_BSCullingGroup_SetCompoundFrustum), "BSCullingGroup_SetCompoundFrustum");

		CreateAndEnableHook((LPVOID)DrawWorld_Move1stPersonToOrigin_Ori.address(), &hkDrawWorld_Move1stPersonToOrigin,
			reinterpret_cast<LPVOID*>(&g_DrawWorld_Move1stPersonToOrigin), "DrawWorld_Move1stPersonToOrigin");

		CreateAndEnableHook((LPVOID)DrawWorld_DoUmbraQuery_Ori.address(), &hkDoUmbraQuery,
			reinterpret_cast<LPVOID*>(&g_DoUmbraQuery), "DoUmbraQuery");

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RenderBatches_Ori.address(), &hkBSShaderAccumulator_RenderBatches,
			reinterpret_cast<LPVOID*>(&g_BSShaderAccumulatorRenderBatches), "BSShaderAccumulator_RenderBatches");

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RenderBlendedDecals_Ori.address(), &hkBSShaderAccumulator_RenderBlendedDecals,
			reinterpret_cast<LPVOID*>(&g_BSShaderAccumulator_RenderBlendedDecals), "BSShaderAccumulator_RenderBlendedDecals");

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RenderOpaqueDecals_Ori.address(), &hkBSShaderAccumulator_RenderOpaqueDecals,
			reinterpret_cast<LPVOID*>(&g_BSShaderAccumulator_RenderOpaqueDecals), "BSShaderAccumulator_RenderOpaqueDecals");

		CreateAndEnableHook((LPVOID)Renderer_CreateRenderTarget_Ori.address(), &hkRenderer_CreateRenderTarget,
			reinterpret_cast<LPVOID*>(&g_Renderer_CreateRenderTarget), "Renderer_CreateRenderTarget");

		CreateAndEnableHook((LPVOID)BSSkyShader_SetupGeometry_Ori.address(), &hkBSSkyShader_SetupGeometry,
			reinterpret_cast<LPVOID*>(&g_BSSkyShader_SetupGeometry), "BSSkyShader_SetupGeometry");

		CreateAndEnableHook((LPVOID)ImageSpaceManager_RenderEffectRange_Ori.address(), &hkImageSpaceManager_RenderEffectRange,
			reinterpret_cast<LPVOID*>(&g_ImageSpaceManager_RenderEffectRange), "ImageSpaceManager_RenderEffectRange");

		CreateAndEnableHook((LPVOID)DrawWorld_Render_UI_Ori.address(), &hkDrawWorld_Render_UI,
			reinterpret_cast<LPVOID*>(&g_DrawWorld_Render_UI), "DrawWorld_Render_UI");

		CreateAndEnableHook((LPVOID)UI_BeginRender_Ori.address(), &hkUI_BeginRender,
			reinterpret_cast<LPVOID*>(&g_UI_BeginRender), "UI_BeginRender");

		CreateAndEnableHook((LPVOID)SetUseDynamicResolutionViewport_Ori.address(), &hkSetUseDynamicResolutionViewport,
			reinterpret_cast<LPVOID*>(&g_SetUseDynamicResolutionViewport), "SetUseDynamicResolutionViewport");

		CreateAndEnableHook((LPVOID)DrawWorld_Render_PostUI_Ori.address(), &hkDrawWorld_Render_PostUI,
			reinterpret_cast<LPVOID*>(&g_DrawWorld_Render_PostUI), "DrawWorld_Render_PostUI");

		CreateAndEnableHook((LPVOID)UI_ScreenSpace_RenderMenus_Ori.address(), &hkUI_ScreenSpace_RenderMenus,
			reinterpret_cast<LPVOID*>(&g_UI_ScreenSpace_RenderMenus), "UI_ScreenSpace_RenderMenus");

		// 几何体验证 hook - 防止 BSMTAManager 因悬空指针崩溃
		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RegisterObject_Ori.address(), &hkBSShaderAccumulator_RegisterObject,
			reinterpret_cast<LPVOID*>(&g_RegisterObjectOriginal), "BSShaderAccumulator_RegisterObject");

		CreateAndEnableHook((LPVOID)DrawTriShape_Ori.address(), &hkDrawTriShape,
			reinterpret_cast<LPVOID*>(&g_DrawTriShape), "DrawTriShape");

		RegisterTAAHook();


		logger::info("Hooks registered successfully");
	}

	void HookManager::FlushBackgroundTasks()
	{
		if (g_ProcessQueues) {
			auto singleton = RE::TaskQueueInterface::GetSingleton();
			if (singleton) {
				// [FIX] 使用 0xFFFFFFFF 处理所有类型的任务队列
				// 之前使用 8 (kQueueUpdate) 可能只处理了部分任务(如 Detach)，导致 Attach 未执行，
				// 从而使 NPC 在那一帧处于"消失"状态(闪烁)。
				// 处理所有队列应该能保证操作的原子性(Detach + Attach 都执行)。
				g_ProcessQueues(singleton, 0.0f, 0xFFFFFFFF);
			}
		}
	}

	void HookManager::RegisterTAAHook()
	{
		REL::Relocation<std::uintptr_t> TAAFunc(REL::ID(528052));
		void* targetAddr = reinterpret_cast<void*>(TAAFunc.address());

		uint8_t* funcBytes = reinterpret_cast<uint8_t*>(targetAddr);
		bool alreadyHooked = (funcBytes[0] == 0xE9 || funcBytes[0] == 0xFF || funcBytes[0] == 0x48);

		if (alreadyHooked) {
			logger::warn("TAA function may already be hooked by another mod");
			MH_DisableHook(targetAddr);
			MH_RemoveHook(targetAddr);
		}

		MH_STATUS status = MH_CreateHook(targetAddr,
			reinterpret_cast<void*>(&hkTAA),
			reinterpret_cast<void**>(&g_TAA));

		if (status != MH_OK) {
			logger::error("Failed to create TAA hook. Status: {}", static_cast<int>(status));

			// Fallback to vtable hook
			REL::Relocation<std::uintptr_t> vtable_TAA(RE::ImageSpaceEffectTemporalAA::VTABLE[0]);
			void** vtablePtr = reinterpret_cast<void**>(vtable_TAA.address());
			void* originalFunc = vtablePtr[1];

			g_TAA = reinterpret_cast<FnTAA>(originalFunc);

			DWORD oldProtect;
			if (VirtualProtect(&vtablePtr[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
				vtablePtr[1] = reinterpret_cast<void*>(&hkTAA);
				VirtualProtect(&vtablePtr[1], sizeof(void*), oldProtect, &oldProtect);
				logger::info("TAA vtable hook applied successfully");
			}
		} else if (MH_EnableHook(targetAddr) != MH_OK) {
			logger::error("Failed to enable TAA hook");
		} else {
			logger::info("TAA hook created successfully");
			if (!g_TAA) {
				logger::error("g_TAA is null after successful hook!");
			}
		}

		if (g_TAA == nullptr) {
			logger::warn("Initial TAA hook failed, starting delayed hook attempts...");
			std::thread([this]() {
				std::this_thread::sleep_for(std::chrono::seconds(3));
				DelayedTAAHook();
			}).detach();
		}
	}

	void HookManager::DelayedTAAHook()
	{
		static int retryCount = 0;
		const int maxRetries = 5;

		REL::Relocation<std::uintptr_t> TAAFunc(REL::ID(528052));
		void* targetAddr = reinterpret_cast<void*>(TAAFunc.address());

		if (g_TAA != nullptr) {
			logger::info("TAA hook already active");
			return;
		}

		retryCount++;
		logger::info("Attempting TAA hook (attempt {}/{})", retryCount, maxRetries);

		MH_STATUS status = MH_CreateHook(targetAddr,
			reinterpret_cast<void*>(&hkTAA),
			reinterpret_cast<void**>(&g_TAA));

		if (status == MH_OK && MH_EnableHook(targetAddr) == MH_OK) {
			logger::info("TAA hook successfully created on attempt {}", retryCount);
		} else if (retryCount < maxRetries) {
			std::thread([this, targetAddr]() {
				std::this_thread::sleep_for(std::chrono::seconds(2));
				DelayedTAAHook();
			}).detach();
		} else {
			logger::error("Failed to create TAA hook after {} attempts", maxRetries);
		}
	}
}
