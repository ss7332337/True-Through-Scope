#include "HookManager.h"
#include "Utilities.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "DataPersistence.h"
#include "GlobalTypes.h"
#include "rendering/LightBackupSystem.h"
#include <thread>
#include <chrono>

namespace ThroughScope
{
	using namespace Utilities;

	static HookManager* g_hookMgr = HookManager::GetSingleton();
	static LightBackupSystem* g_lightBackup = LightBackupSystem::GetSingleton();

	// 前向声明，实际定义在main.cpp中
	namespace FirstSpawnDelay
	{
		extern bool delayStarted;
		extern std::chrono::steady_clock::time_point delayStartTime;
		void Reset();
	}
	void ResetFirstSpawnState();

bool IsValidPointer(const void* ptr)
	{
		if (!ptr)
			return false;

		// 检查指针是否在合理的地址范围内
		uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
		if (addr < 0x10000 || addr == 0xFFFFFFFFFFFFFFFF)
			return false;

		// 尝试使用IsBadReadPtr检查内存可读性（Windows特定）
		__try {
			volatile char test = *reinterpret_cast<const char*>(ptr);
			(void)test;
			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	bool IsValidObject(NiAVObject* apObj)
	{
		if (!IsValidPointer(apObj))
			return false;

		if (apObj->refCount == 0)
			return false;

		uintptr_t vtable = *(uintptr_t*)apObj;
		if (vtable < 0x10000 || vtable == 0xFFFFFFFFFFFFFFFF) {
			return false;
		}

		// 检查虚函数指针
		uintptr_t funcPtr = *(uintptr_t*)(vtable + 0x40);
		return (funcPtr != 0 && funcPtr != 0xFFFFFFFFFFFFFFFF);
	}

	void hkBSCullingGroupAdd(BSCullingGroup* thisPtr,
		NiAVObject* apObj,
		const NiBound* aBound,
		const unsigned int aFlags)
	{
		// 验证thisPtr
		if (!thisPtr) {
			logger::error("BSCullingGroup thisPtr is null");
			return;
		}

		// 验证apObj
		if (!IsValidObject(apObj)) {
			logger::error("Invalid object: 0x{:X}", (uintptr_t)apObj);
			return;
		}

		// 验证aBound指针
		if (!IsValidPointer(aBound)) {
			const char* objName = (apObj && apObj->name.c_str()) ? apObj->name.c_str() : "unknown";
			logger::error("NiBound is invalid for object: {} (ptr: 0x{:X})", objName, reinterpret_cast<uintptr_t>(aBound));
			return;
		}

		// 在第二次渲染期间，对某些特殊对象进行过滤
		if (ScopeCamera::IsRenderingForScope()) {
			// 跳过天气相关对象，避免在瞄准镜场景中渲染
			if (apObj->name.c_str() && strstr(apObj->name.c_str(), "Weather")) {
				return;
			}
		}

		// 使用异常保护调用原始函数，防止内部崩溃
		__try {
		g_hookMgr->g_BSCullingGroupAdd(thisPtr, apObj, aBound, aFlags);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			const char* objName = (apObj && apObj->name.c_str()) ? apObj->name.c_str() : "unknown";
			logger::error("Exception in BSCullingGroupAdd for object: {} (flags: 0x{:X})", objName, aFlags);
		}
	}

	void hkDrawTriShape(BSGraphics::Renderer* thisPtr, BSGraphics::TriShape* apTriShape, unsigned int auiStartIndex, unsigned int auiNumTriangles)
	{
		auto trishape = reinterpret_cast<BSTriShape*>(apTriShape);

		if (trishape->numTriangles == 32 && trishape->numVertices == 33)
			return;

		g_hookMgr->g_DrawTriShape(thisPtr, apTriShape, auiStartIndex, auiNumTriangles);
	}

	void hkBSStreamLoad(BSStream* stream, const char* apFileName, NiBinaryStream* apStream)
	{
		logger::info("apFileName: {}", apFileName);
		g_hookMgr->g_BSStreamLoad(stream, apFileName, apStream);
	}

	void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld)
	{
		savedDrawWorld = ptr_drawWorld;
		D3DEventNode(g_hookMgr->g_RenderPreUIOriginal(ptr_drawWorld), L"Render_PreUI");
	}

	void __fastcall hkRenderZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::ZPrePassDrawData* aZPreData,
		unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode)
	{
		g_hookMgr->g_RenderZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode);
	}

	void __fastcall hkRenderAlphaTestZPrePass(BSGraphics::RendererShadowState* rshadowState,
		BSGraphics::AlphaTestZPrePassDrawData* aZPreData,
		unsigned __int64* aVertexDesc,
		unsigned __int16* aCullmode,
		unsigned __int16* aDepthBiasMode,
		ID3D11SamplerState** aCurSamplerState)
	{
		g_hookMgr->g_RenderAlphaTestZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode, aCurSamplerState);
	}

	void __fastcall hkRenderer_DoZPrePass(uint64_t thisPtr, NiCamera* apFirstPersonCamera, NiCamera* apWorldCamera, float afFPNear, float afFPFar, float afNear, float afFar)
	{
		if (ScopeCamera::IsRenderingForScope()) {
			*FPZPrePassDrawDataCount = 0;
			*FPAlphaTestZPrePassDrawDataCount = 0;
		}
		D3DEventNode(g_hookMgr->g_pDoZPrePassOriginal(thisPtr, apFirstPersonCamera, apWorldCamera, afFPNear, afFPFar, afNear, afFar), L"hkRenderer_DoZPrePass");
	}

	void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr)
	{
		if (ScopeCamera::IsRenderingForScope())
			return;
		g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);
	}

	void __fastcall hkBSShaderAccumulator_RenderBatches(
		BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup)
	{
		g_hookMgr->g_BSShaderAccumulatorRenderBatches(thisPtr, aiShader, abAlphaPass, aeGroup);
	}

	void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld)
	{
		if (ScopeCamera::IsRenderingForScope()) {
			// 在瞄具渲染过程中重新应用光源状态
			// 启用光源数量限制优化：最多使用16个最重要的光源
			g_lightBackup->ApplyLightStatesForScope(true, 16);
		}

		D3DEventNode(g_hookMgr->g_DeferredLightsImplOriginal(ptr_drawWorld), L"hkDeferredLightsImpl");
	}

	void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld)
	{
		D3DHooks::SetForwardStage(true);
		D3DEventNode(g_hookMgr->g_ForwardOriginal(ptr_drawWorld), L"hkDrawWorld_Forward");
		D3DHooks::SetForwardStage(false);
	}


	void __fastcall hkPCUpdateMainThread(PlayerCharacter* pChar)
	{
		SHORT keyPgDown = GetAsyncKeyState(VK_NEXT);
		SHORT keyPgUp = GetAsyncKeyState(VK_PRIOR);

		if (keyPgUp & 0x1)
		{
			BSScrapArray<NiPointer<Actor>> aRetArray;
			std::vector<Actor*> actorList{};
			ProcessLists::GetSingleton()->GetActorsWithinRangeOfReference(g_pchar, 20000, &aRetArray);
			auto imadThermal = Utilities::GetFormFromMod("WestTekTacticalOptics.esp", 0x811)->As<TESImageSpaceModifier>();
			for (size_t i = 0; i < aRetArray.size(); i++)
			{
				Actor* actor = aRetArray[i].get();
				if (actor->formID != 0x7 && actor->formID != 0x14)
				{
					std::string actorName = actor->GetDisplayFullName();
					logger::info("Found Actor: {}", actorName.c_str());
				}
				actorList.push_back(actor);
			}

			logger::info("actorList size: {}", actorList.size());

			for (auto actor : actorList)
			{
				auto actorNode = actor->Get3D()->IsNode();
				if (!actorNode) continue;

				auto actorBase = actor->GetActorBase();
				if (!actorBase) continue;

				LogNodeHierarchy(actorNode);
				auto actorNodeChildren = actorNode->children.data();
				for (size_t j = 0; j < actorNode->children.size(); j++)
				{
					auto child = actorNodeChildren[j].get();
					if (!child) continue;

					auto childGeo = child->IsTriShape();
					if (childGeo) {
						auto lightShader = childGeo->QShaderProperty();
						auto lightShader2 = (BSLightingShaderProperty*)lightShader;
						auto lightMat = (BSLightingShaderMaterial*)lightShader2->material;
					}
				}
			}
		}
		if (keyPgDown & 0x1)
		{
			Utilities::LogPlayerWeaponNodes();
		}

		auto weaponInfo = DataPersistence::GetCurrentWeaponInfo();

		if (!weaponInfo.currentConfig)
		{
			D3DHooks::SetEnableRender(false);
			return g_hookMgr->g_PCUpdateMainThread(pChar);
		}

		// 定期检查并恢复ZoomData（防止游戏重置）
		ScopeCamera::RestoreZoomDataForCurrentWeapon();

		if (!ScopeCamera::hasFirstSpawnNode) {
			if (!FirstSpawnDelay::delayStarted) {
				// 开始延迟计时
				FirstSpawnDelay::delayStarted = true;
				FirstSpawnDelay::delayStartTime = std::chrono::steady_clock::now();
			} else {
				// 检查是否已经过了500ms
				auto currentTime = std::chrono::steady_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - FirstSpawnDelay::delayStartTime);

				if (elapsed.count() >= 500) {
					// 延迟时间到，执行逻辑
					ScopeCamera::CleanupScopeResources();
					ScopeCamera::SetupScopeForWeapon(weaponInfo);
					d3dHooks->SetScopeTexture((ID3D11DeviceContext*)RE::BSGraphics::RendererData::GetSingleton()->context);
					ScopeCamera::hasFirstSpawnNode = true;
					logger::warn("FirstSpawn Finish");
					// 确保ZoomData被正确设置
					ScopeCamera::RestoreZoomDataForCurrentWeapon();
				}
			}
			return g_hookMgr->g_PCUpdateMainThread(pChar);
		}

		if (g_pchar->IsInThirdPerson())
			return g_hookMgr->g_PCUpdateMainThread(pChar);

		if (ScopeCamera::IsSideAim()
			|| UI::GetSingleton()->GetMenuOpen("PauseMenu")
			|| UI::GetSingleton()->GetMenuOpen("WorkshopMenu")
			|| UI::GetSingleton()->GetMenuOpen("CursorMenu")
			|| UI::GetSingleton()->GetMenuOpen("ScopeMenu")
			|| UI::GetSingleton()->GetMenuOpen("LooksMenu")
			) {
			D3DHooks::SetEnableRender(false);
		} else {
			if (IsInADS(g_pchar))
			{
				D3DHooks::HandleFOVInput();
				D3DHooks::SetEnableRender(true);
			}
		}

		if (!IsInADS(g_pchar)) {
			D3DHooks::SetEnableRender(false);
		}

		g_hookMgr->g_PCUpdateMainThread(pChar);
	}
}
