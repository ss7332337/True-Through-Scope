#include "D3DHooks.h"
#include "DataPersistence.h"
#include "GlobalTypes.h"
#include "HookManager.h"
#include "RenderUtilities.h"
#include "ScopeCamera.h"
#include "Utilities.h"
#include "rendering/LightBackupSystem.h"
#include "rendering/ScopeRenderingManager.h"
#include "rendering/SecondPassRenderer.h"
#include "rendering/RenderStateManager.h"
#include <d3d9.h>  // for D3DPERF_BeginEvent / D3DPERF_EndEvent
#include <DirectXMath.h>
#include <chrono>
#include <cmath>
#include <thread>
#include <winternl.h>
#include <xmmintrin.h>

namespace ThroughScope
{
	using namespace Utilities;
	using namespace RE;

	static HookManager* g_hookMgr = HookManager::GetSingleton();
	static LightBackupSystem* g_lightBackup = LightBackupSystem::GetSingleton();
	static ScopeRenderingManager* g_scopeRenderMgr = ScopeRenderingManager::GetSingleton();

	// ========== 激光节点识别 ==========

	inline bool IsLaserNodeName(const char* name)
	{
		if (!name || name[0] == '\0') return false;
		return (strstr(name, "Laser") != nullptr) ||
		       (strstr(name, "laser") != nullptr) ||
		       (strstr(name, "Beam") != nullptr) ||
		       (strstr(name, "beam") != nullptr) ||
		       (strstr(name, "Dot") != nullptr);
	}

	struct DetachedNodeInfo {
		NiAVObject* node;
		NiNode* parent;
	};

	void CollectNonLaserGeometries(
		NiAVObject* node,
		NiNode* parentNode,
		int depth,
		std::vector<DetachedNodeInfo>& outDetached,
		int& outLaserCount)
	{
		if (!node || depth > 20) return;

		const char* name = node->name.c_str();
		bool isLaser = IsLaserNodeName(name);

		if (isLaser) {
			outLaserCount++;
		} else {
			auto geom = node->IsGeometry();
			if (geom && parentNode) {
				outDetached.push_back({ node, parentNode });
			}
		}

		auto niNode = node->IsNode();
		if (niNode) {
			for (auto& child : niNode->children) {
				if (child.get()) {
					CollectNonLaserGeometries(child.get(), niNode, depth + 1, outDetached, outLaserCount);
				}
			}
		}
	}



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
		D3DPERF_BeginEvent(0xffffffff, L"BSCullingGroup_Add");
		if (!thisPtr || !IsValidObject(apObj) || !IsValidPointer(aBound)) {
			D3DPERF_EndEvent();
			return;
		}

		if (ScopeCamera::IsRenderingForScope()) {
			const char* objName = apObj->name.c_str();
			if (objName && strstr(objName, "Weather")) {
				D3DPERF_EndEvent();
				return;
			}
		}

		g_hookMgr->g_BSCullingGroupAdd(thisPtr, apObj, aBound, aFlags);
		D3DPERF_EndEvent();
	}

	void hkDrawTriShape(BSGraphics::Renderer* thisPtr, BSGraphics::TriShape* apTriShape, unsigned int auiStartIndex, unsigned int auiNumTriangles)
	{
		D3DPERF_BeginEvent(0xffffffff, L"DrawTriShape");
		auto trishape = reinterpret_cast<BSTriShape*>(apTriShape);

		if (trishape->numTriangles == 32 && trishape->numVertices == 33) {
			D3DPERF_EndEvent();
			return;
		}

		g_hookMgr->g_DrawTriShape(thisPtr, apTriShape, auiStartIndex, auiNumTriangles);
		D3DPERF_EndEvent();
	}

	void hkBSStreamLoad(BSStream* stream, const char* apFileName, NiBinaryStream* apStream)
	{
		D3DPERF_BeginEvent(0xffffffff, L"BSStream_Load");
		g_hookMgr->g_BSStreamLoad(stream, apFileName, apStream);
		D3DPERF_EndEvent();
	}

	void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld)
	{
		savedDrawWorld = ptr_drawWorld;

		// 在非瞄具渲染时捕获主相机 FOV
		if (!ScopeCamera::IsRenderingForScope()) {
			const auto playerCamera = RE::PlayerCamera::GetSingleton();
			if (playerCamera) {
				g_MainCameraFOV = playerCamera->firstPersonFOV;
			}

			D3DPERF_BeginEvent(0xFF00FF00, L"TrueThroughScope_FirstPass");
			g_hookMgr->g_RenderPreUIOriginal(ptr_drawWorld);
			D3DPERF_EndEvent();

		} else {
			g_hookMgr->g_RenderPreUIOriginal(ptr_drawWorld);
		}
	}


	void __fastcall hkRenderZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::ZPrePassDrawData* aZPreData,
		unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode)
	{
		D3DPERF_BeginEvent(0xffffffff, L"Render_ZPrePass");
		if (ScopeCamera::IsRenderingForScope()) {
			__try {
				g_hookMgr->g_RenderZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				// 静默跳过会导致崩溃的绘制调用
			}
		} else {
			g_hookMgr->g_RenderZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode);
		}
		D3DPERF_EndEvent();
	}

	void __fastcall hkRenderAlphaTestZPrePass(BSGraphics::RendererShadowState* rshadowState,
		BSGraphics::AlphaTestZPrePassDrawData* aZPreData,
		unsigned __int64* aVertexDesc,
		unsigned __int16* aCullmode,
		unsigned __int16* aDepthBiasMode,
		ID3D11SamplerState** aCurSamplerState)
	{
		D3DPERF_BeginEvent(0xffffffff, L"Render_AlphaTestZPrePass");
		// 仅在瞄具渲染期间验证 pTriShape 有效性
		if (ScopeCamera::IsRenderingForScope()) {
			__try {
				g_hookMgr->g_RenderZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				// 静默跳过会导致崩溃的绘制调用
			}
		} else {
			g_hookMgr->g_RenderZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode);
		}
		D3DPERF_EndEvent();
	}

	void __fastcall hkRenderer_DoZPrePass(uint64_t thisPtr, NiCamera* apFirstPersonCamera, NiCamera* apWorldCamera, float afFPNear, float afFPFar, float afNear, float afFar)
	{
		if (ScopeCamera::IsRenderingForScope()) {
			*FPZPrePassDrawDataCount = 0;
			*FPAlphaTestZPrePassDrawDataCount = 0;
		}
		D3DEventNode(g_hookMgr->g_pDoZPrePassOriginal(thisPtr, apFirstPersonCamera, apWorldCamera, afFPNear, afFPFar, afNear, afFar), L"hkRenderer_DoZPrePass");
	}

	/**
	 * @brief Add1stPersonGeomToCuller 钩子
	 *
	 * 在瞄具渲染时，只允许激光几何体被添加到渲染队列。
	 * 实现方式：临时从场景图中分离非激光几何体，调用原函数后再重新附加。
	 *
	 * 为什么不用 AppCulled：
	 * 测试发现引擎在 Forward 渲染阶段不检查 AppCulled 标志，
	 * 几何体在设置 AppCulled 之前已被添加到渲染队列。
	 */
	void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr)
	{
		D3DPERF_BeginEvent(0xffffffff, L"Add1stPersonGeomToCuller");
		// 非瞄具渲染：直接调用原函数
		if (!ScopeCamera::IsRenderingForScope()) {
			g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);
			D3DPERF_EndEvent();
			return;
		}

		auto p1stPerson = *ThroughScope::ptr_DrawWorld1stPerson;
		if (!p1stPerson) {
			g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);
			D3DPERF_EndEvent();
			return;
		}

		std::vector<DetachedNodeInfo> detachedNodes;
		int laserCount = 0;

		CollectNonLaserGeometries(p1stPerson, nullptr, 0, detachedNodes, laserCount);

		NiAVObject* weaponNode = p1stPerson->GetObjectByName("Weapon");
		if (!weaponNode) {
			auto playerChar = RE::PlayerCharacter::GetSingleton();
			if (playerChar && playerChar->Get3D(false)) {
				weaponNode = playerChar->Get3D(false)->GetObjectByName("Weapon");
			}
		}
		if (weaponNode) {
			CollectNonLaserGeometries(weaponNode, nullptr, 0, detachedNodes, laserCount);
		}

		for (auto& info : detachedNodes) {
			if (info.parent && info.node) {
				info.parent->DetachChild(info.node);
			}
		}

		g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);

		for (auto& info : detachedNodes) {
			if (info.parent && info.node) {
				info.parent->AttachChild(info.node, true);
			}
		}
		D3DPERF_EndEvent();
	}

	void __fastcall hkBSShaderAccumulator_RenderBatches(
		BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup)
	{
		D3DEventNode(g_hookMgr->g_BSShaderAccumulatorRenderBatches(thisPtr, aiShader, abAlphaPass, aeGroup), L"BSShaderAccumulator_RenderBatches");
	}

	void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld)
	{
		if (ScopeCamera::IsRenderingForScope()) {
			g_lightBackup->ApplyLightStatesForScope(false, 16);
		}
		D3DEventNode(g_hookMgr->g_DeferredLightsImplOriginal(ptr_drawWorld), L"hkDeferredLightsImpl");
	}

	void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld)
	{
		D3DHooks::SetForwardStage(true);
		D3DEventNode(g_hookMgr->g_ForwardOriginal(ptr_drawWorld), L"hkDrawWorld_Forward");
		D3DHooks::SetForwardStage(false);

		auto scopeRenderMgr = ScopeRenderingManager::GetSingleton();
		if (scopeRenderMgr->IsUpscalingActive()) {
			D3DPERF_BeginEvent(0xFF00AAFF, L"[FO4TEST_TIMING] After_Forward_CopyBuffersToShared");
			D3DPERF_EndEvent();
		}
	}

	void __fastcall hkPCUpdateMainThread(PlayerCharacter* pChar)
	{

		auto weaponInfo = DataPersistence::GetCurrentWeaponInfo();

		if (!weaponInfo.currentConfig) {
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
				ScopeCamera::CleanupScopeResources();
				ScopeCamera::SetupScopeForWeapon(weaponInfo);
				d3dHooks->SetScopeTexture((ID3D11DeviceContext*)RE::BSGraphics::RendererData::GetSingleton()->context);
				ScopeCamera::hasFirstSpawnNode = true;
				ScopeCamera::RestoreZoomDataForCurrentWeapon();
			}
			}
			return g_hookMgr->g_PCUpdateMainThread(pChar);
		}

		if (g_pchar->IsInThirdPerson())
			return g_hookMgr->g_PCUpdateMainThread(pChar);

		if (ScopeCamera::IsSideAim() || UI::GetSingleton()->GetMenuOpen("PauseMenu") || UI::GetSingleton()->GetMenuOpen("WorkshopMenu") || UI::GetSingleton()->GetMenuOpen("CursorMenu") || UI::GetSingleton()->GetMenuOpen("ScopeMenu") || UI::GetSingleton()->GetMenuOpen("LooksMenu")) {
			D3DHooks::SetEnableRender(false);
		} else {
			if (IsInADS(g_pchar)) {
				D3DHooks::HandleFOVInput();
				D3DHooks::SetEnableRender(true);
			}
		}

		if (!IsInADS(g_pchar)) {
			D3DHooks::SetEnableRender(false);
		}

		g_hookMgr->g_PCUpdateMainThread(pChar);
	}

	// TLS 相关的静态重定位对象
	static REL::Relocation<uint32_t*> ptr_tls_index_sky{ REL::ID(842564) };
	static REL::Relocation<uintptr_t*> ptr_DefaultContext_sky{ REL::ID(33539) };

	// 星星世界缩放因子（硬编码）
	// 这个值用于补偿瞄具场景中 viewProjMat 与主相机的缩放差异
	// 经测试，101 倍可以使星星大小和位置移动与主场景一致
	static constexpr float STAR_WORLD_SCALE_FACTOR = 120.0f;

	// 检查几何体是否是星星
	static bool IsStarGeometry(BSRenderPass* apCurrentPass)
	{
		if (!apCurrentPass || !apCurrentPass->pGeometry) {
			return false;
		}

		// 获取几何体名称
		const char* geomName = apCurrentPass->pGeometry->name.c_str();
		if (!geomName) {
			return false;
		}

		// 检查名称是否包含 "Star" 或 "star"
		// 注意：需要根据实际游戏中星星几何体的名称进行调整
		if (strstr(geomName, "Star") != nullptr || strstr(geomName, "star") != nullptr) {
			return true;
		}

		return false;
	}

	void __fastcall hkBSSkyShader_SetupGeometry(void* thisPtr, BSRenderPass* apCurrentPass)
	{
		D3DPERF_BeginEvent(0xffffffff, L"BSSkyShader_SetupGeometry");
		// 如果不在瞄具渲染模式，直接调用原始函数
		if (!ScopeCamera::IsRenderingForScope()) {
			g_hookMgr->g_BSSkyShader_SetupGeometry(thisPtr, apCurrentPass);
			D3DPERF_EndEvent();
			return;
		}

		// 检查是否有保存的正确矩阵
		if (!g_ScopeViewProjMatValid) {
			// 没有保存的矩阵，直接调用原始函数
			g_hookMgr->g_BSSkyShader_SetupGeometry(thisPtr, apCurrentPass);
			D3DPERF_EndEvent();
			return;
		}

		// 从 TLS 获取 BSGraphics::Context
		_TEB* teb = NtCurrentTeb();
		auto tls_index = *ptr_tls_index_sky;
		uintptr_t contextPtr = *(uintptr_t*)(*((uint64_t*)teb->Reserved1[11] + tls_index) + 2848i64);
		if (!contextPtr) {
			contextPtr = *ptr_DefaultContext_sky;
		}

		if (!contextPtr) {
			g_hookMgr->g_BSSkyShader_SetupGeometry(thisPtr, apCurrentPass);
			D3DPERF_EndEvent();
			return;
		}

		auto context = (RE::BSGraphics::Context*)contextPtr;
		auto& cameraData = context->shadowState.CameraData;

		// 备份当前的投影矩阵（这是被主相机覆盖的错误矩阵）
		__m128 backupViewProjMat[4];
		backupViewProjMat[0] = cameraData.viewProjMat[0];
		backupViewProjMat[1] = cameraData.viewProjMat[1];
		backupViewProjMat[2] = cameraData.viewProjMat[2];
		backupViewProjMat[3] = cameraData.viewProjMat[3];

		// 检测是否正在渲染星星
		bool isRenderingStars = IsStarGeometry(apCurrentPass);

		// 用保存的正确矩阵替换（这是在 SetCameraData 后用瞄具相机生成的矩阵）
		cameraData.viewProjMat[0] = g_ScopeViewProjMat[0];
		cameraData.viewProjMat[1] = g_ScopeViewProjMat[1];
		cameraData.viewProjMat[2] = g_ScopeViewProjMat[2];
		cameraData.viewProjMat[3] = g_ScopeViewProjMat[3];

		// 备份并修改星星几何体的世界缩放
		// 使用 101 倍缩放来补偿 viewProjMat 的缩放差异
		float originalWorldScale = 1.0f;
		if (isRenderingStars && apCurrentPass->pGeometry) {
			originalWorldScale = apCurrentPass->pGeometry->world.scale;
			apCurrentPass->pGeometry->world.scale = originalWorldScale * STAR_WORLD_SCALE_FACTOR;
		}

		// 调用原始函数（使用修改后的矩阵和世界缩放）
		g_hookMgr->g_BSSkyShader_SetupGeometry(thisPtr, apCurrentPass);

		// 恢复星星几何体的原始世界缩放
		if (isRenderingStars && apCurrentPass->pGeometry) {
			apCurrentPass->pGeometry->world.scale = originalWorldScale;
		}

		// 恢复原来的 viewProjMat
		cameraData.viewProjMat[0] = backupViewProjMat[0];
		cameraData.viewProjMat[1] = backupViewProjMat[1];
		cameraData.viewProjMat[2] = backupViewProjMat[2];
		cameraData.viewProjMat[3] = backupViewProjMat[3];
		D3DPERF_EndEvent();
	}

	// ========== ImageSpaceManager::RenderEffectRange Hook ==========
	// 此函数在 Render_UI 中被调用多次：
	// - 效果 0-13: HDR 等
	// - 效果 15-21: TAA, DOF 等
	// 当 aiLast=21 时表示所有效果完成，是渲染瞄具的理想时机
	void __fastcall hkImageSpaceManager_RenderEffectRange(void* thisPtr, int aiFirst, int aiLast, int aiSourceTarget, int aiDestTarget)
	{
		// 用 D3DPERF 标记显示调用参数
		wchar_t markerName[128];
		swprintf_s(markerName, L"RenderEffectRange(%d-%d, src=%d, dst=%d)", aiFirst, aiLast, aiSourceTarget, aiDestTarget);
		D3DPERF_BeginEvent(0xFFFF00FF, markerName);
		
		// 调用原始函数
		g_hookMgr->g_ImageSpaceManager_RenderEffectRange(thisPtr, aiFirst, aiLast, aiSourceTarget, aiDestTarget);
		
		D3DPERF_EndEvent();
		
		// 当效果范围 15-21 完成后渲染瞄具
		// 这是在 HDR/TAA/DOF 之后的时机
		if (aiLast == 21) {
			auto renderStateMgr = RenderStateManager::GetSingleton();
			
			if (renderStateMgr->IsScopeReady() && 
				renderStateMgr->IsRenderReady() && 
				D3DHooks::IsEnableRender() &&
				!ScopeCamera::IsRenderingForScope()) {
				
				ID3D11DeviceContext* context = d3dHooks->GetContext();
				ID3D11Device* device = d3dHooks->GetDevice();
				
				if (context && device) {
					D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_SecondPass_PostEffects");
					SecondPassRenderer renderer(context, device, d3dHooks);
					if (!renderer.ExecuteSecondPass()) {
						logger::warn("[RenderEffectRange Hook] SecondPassRenderer failed");
					}
					D3DPERF_EndEvent();
					g_scopeRenderMgr->OnFrameEnd();
				}
			}
		}
	}

	// ========== DrawWorld::Render_UI Debug Hook ==========
	// 仅用于 RenderDoc 调试，显示 Render_UI 的开始和结束位置
	void __fastcall hkDrawWorld_Render_UI(uint64_t thisPtr)
	{
		D3DPERF_BeginEvent(0xFF00FF00, L"DrawWorld_Render_UI");
		g_hookMgr->g_DrawWorld_Render_UI(thisPtr);
		D3DPERF_EndEvent();
	}

	// ========== UI::BeginRender Debug Hook ==========
	// 仅用于 RenderDoc 调试，显示 UI::BeginRender 的位置
	// 此处使用D3DPERF会崩溃
	void __fastcall hkUI_BeginRender()
	{
		//D3DPERF_BeginEvent(0xFFFF00FF, L"UI_BeginRender");
		g_hookMgr->g_UI_BeginRender();
		//D3DPERF_EndEvent();
	}

	// ========== SetUseDynamicResolutionViewport Debug Hook ==========
	// fo4test 在此函数参数为 false 时调用 PostDisplay()
	// PostDisplay 将 kFrameBuffer 复制到 HUDLessBufferShared 供 FSR3 处理
	void __fastcall hkSetUseDynamicResolutionViewport(void* thisPtr, bool a_useDynamicResolution)
	{
		if (a_useDynamicResolution) {
			D3DPERF_BeginEvent(0xFF0088FF, L"SetUseDynamicResolutionViewport(TRUE)");
		} else {
			// 这是 fo4test 调用 PostDisplay 的时机点！
			D3DPERF_BeginEvent(0xFFFF0000, L"SetUseDynamicResolutionViewport(FALSE)_PostDisplay_Timing");
		}
		
		g_hookMgr->g_SetUseDynamicResolutionViewport(thisPtr, a_useDynamicResolution);
		
		D3DPERF_EndEvent();
	}

	// ========== DrawWorld::Imagespace Debug Hook ==========
	// Upscaling/TAA 发生处 - 这是 FSR3/DLSS 处理的核心位置
	void __fastcall hkDrawWorld_Imagespace()
	{
		D3DPERF_BeginEvent(0xFFFF8800, L"DrawWorld_Imagespace (Upscaling/TAA)");
		g_hookMgr->g_DrawWorld_Imagespace();
		D3DPERF_EndEvent();
	}

	// ========== DrawWorld::Render_PostUI Debug Hook ==========
	// 在 Upscaling 后，UI 前 - 这是瞄具渲染的理想位置！
	void __fastcall hkDrawWorld_Render_PostUI()
	{
		D3DPERF_BeginEvent(0xFF00FF00, L"DrawWorld_Render_PostUI (IDEAL_SCOPE_RENDER_POINT)");
		g_hookMgr->g_DrawWorld_Render_PostUI();
		D3DPERF_EndEvent();
	}

	// ========== UI::ScreenSpace_RenderMenus Debug Hook ==========
	// 实际 UI 覆盖层渲染
	void __fastcall hkUI_ScreenSpace_RenderMenus(void* thisPtr)
	{
		D3DPERF_BeginEvent(0xFFFF00FF, L"UI_ScreenSpace_RenderMenus");
		g_hookMgr->g_UI_ScreenSpace_RenderMenus(thisPtr);
		D3DPERF_EndEvent();
	}
}
