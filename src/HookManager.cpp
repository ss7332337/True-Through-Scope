#include "HookManager.h"
#include "Utilities.h"
#include <MinHook.h>
#include <thread>
#include <chrono>

namespace ThroughScope
{
	using namespace Utilities;

	void HookManager::RegisterAllHooks()
	{
		logger::info("Registering hooks...");

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

		CreateAndEnableHook((LPVOID)DrawWorld_MainAccum_Ori.address(), &hkMainAccum,
			reinterpret_cast<LPVOID*>(&g_MainAccumOriginal), "MainAccum");

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

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RenderBatches_Ori.address(), &hkBSShaderAccumulator_RenderBatches,
			reinterpret_cast<LPVOID*>(&g_BSShaderAccumulatorRenderBatches), "BSShaderAccumulator_RenderBatches");

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RenderBlendedDecals_Ori.address(), &hkBSShaderAccumulator_RenderBlendedDecals,
			reinterpret_cast<LPVOID*>(&g_BSShaderAccumulator_RenderBlendedDecals), "BSShaderAccumulator_RenderBlendedDecals");

		CreateAndEnableHook((LPVOID)BSShaderAccumulator_RenderOpaqueDecals_Ori.address(), &hkBSShaderAccumulator_RenderOpaqueDecals,
			reinterpret_cast<LPVOID*>(&g_BSShaderAccumulator_RenderOpaqueDecals), "BSShaderAccumulator_RenderOpaqueDecals");

		CreateAndEnableHook((LPVOID)Renderer_CreateRenderTarget_Ori.address(), &hkRenderer_CreateRenderTarget,
			reinterpret_cast<LPVOID*>(&g_Renderer_CreateRenderTarget), "Renderer_CreateRenderTarget");

		RegisterTAAHook();

		logger::info("Hooks registered successfully");
	}

	void HookManager::RegisterTAAHook()
	{
		REL::Relocation<std::uintptr_t> TAAFunc(REL::ID(528052));
		void* targetAddr = reinterpret_cast<void*>(TAAFunc.address());

		uint8_t* funcBytes = reinterpret_cast<uint8_t*>(targetAddr);
		bool alreadyHooked = false;

		if (funcBytes[0] == 0xE9 || funcBytes[0] == 0xFF || funcBytes[0] == 0x48) {
			logger::warn("TAA function may already be hooked by another mod. First bytes: {:02X} {:02X} {:02X} {:02X} {:02X}",
				funcBytes[0], funcBytes[1], funcBytes[2], funcBytes[3], funcBytes[4]);
			alreadyHooked = true;
		}

		if (alreadyHooked) {
			logger::info("Attempting to remove existing hook...");
			MH_DisableHook(targetAddr);
			MH_RemoveHook(targetAddr);
		}

		logger::info("Target TAA function address: {:X}", reinterpret_cast<uintptr_t>(targetAddr));
		logger::info("Hook function (hkTAA) address: {:X}", reinterpret_cast<uintptr_t>(&hkTAA));

		MH_STATUS status = MH_CreateHook(targetAddr,
			reinterpret_cast<void*>(&hkTAA),
			reinterpret_cast<void**>(&g_TAA));

		if (status != MH_OK) {
			logger::error("Failed to create TAA hook. Status: {}", static_cast<int>(status));

			logger::info("Attempting vtable hook as fallback...");
			REL::Relocation<std::uintptr_t> vtable_TAA(RE::ImageSpaceEffectTemporalAA::VTABLE[0]);
			void** vtablePtr = reinterpret_cast<void**>(vtable_TAA.address());
			void* originalFunc = vtablePtr[1];

			if (reinterpret_cast<uintptr_t>(originalFunc) == TAAFunc.address()) {
				logger::info("Vtable points to expected function, modifying vtable directly");
			} else {
				logger::warn("Vtable function differs from expected. Vtable: {:X}, Expected: {:X}",
					reinterpret_cast<uintptr_t>(originalFunc), TAAFunc.address());
			}

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
			logger::info("TAA hook created successfully at address: {:X} (ID: 528052)", TAAFunc.address());
			logger::info("Original function (g_TAA) points to: {:X}", reinterpret_cast<uintptr_t>(g_TAA));

			funcBytes = reinterpret_cast<uint8_t*>(targetAddr);
			logger::info("After hook - First bytes: {:02X} {:02X} {:02X} {:02X} {:02X}",
				funcBytes[0], funcBytes[1], funcBytes[2], funcBytes[3], funcBytes[4]);

			if (g_TAA) {
				logger::info("g_TAA is valid and points to trampoline");
			} else {
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
