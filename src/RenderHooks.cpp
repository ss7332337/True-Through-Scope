#include "D3DHooks.h"
#include "DataPersistence.h"
#include "GlobalTypes.h"
#include "HookManager.h"
#include "RenderUtilities.h"
#include "ScopeCamera.h"
#include "Utilities.h"
#include "rendering/LightBackupSystem.h"
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

	// ========== 调试模式配置 ==========
	static bool g_DebugLaserInvestigation = false;
	static bool g_HasLoggedLaserInfo = false;
	static int g_LaserLogFrameCount = 0;
	constexpr int LASER_LOG_FRAMES = 3;

	// ========== 激光节点识别 ==========

	/**
	 * @brief 检查节点名称是否为激光相关节点
	 * @param name 节点名称
	 * @return 如果是激光节点返回 true
	 *
	 * 识别规则：名称包含 Laser/laser/Beam/beam/Dot
	 */
	inline bool IsLaserNodeName(const char* name)
	{
		if (!name || name[0] == '\0') return false;
		return (strstr(name, "Laser") != nullptr) ||
		       (strstr(name, "laser") != nullptr) ||
		       (strstr(name, "Beam") != nullptr) ||
		       (strstr(name, "beam") != nullptr) ||
		       (strstr(name, "Dot") != nullptr);
	}

	/**
	 * @brief 用于临时分离的节点信息
	 */
	struct DetachedNodeInfo {
		NiAVObject* node;
		NiNode* parent;
	};

	/**
	 * @brief 递归收集需要分离的非激光几何体
	 * @param node 当前节点
	 * @param parentNode 父节点
	 * @param depth 当前深度
	 * @param outDetached 输出：需要分离的节点列表
	 * @param outLaserCount 输出：找到的激光节点数量
	 */
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
			if (g_DebugLaserInvestigation && g_LaserLogFrameCount < LASER_LOG_FRAMES) {
				logger::info("[Laser] Found: {} (depth: {})", name, depth);
			}
		} else {
			// 非激光几何体需要分离
			auto geom = node->IsGeometry();
			if (geom && parentNode) {
				outDetached.push_back({ node, parentNode });
			}
		}

		// 递归处理子节点
		auto niNode = node->IsNode();
		if (niNode) {
			for (auto& child : niNode->children) {
				if (child.get()) {
					CollectNonLaserGeometries(child.get(), niNode, depth + 1, outDetached, outLaserCount);
				}
			}
		}
	}

	// ========== 调试日志函数 ==========

	void SetLaserInvestigationMode(bool enable)
	{
		g_DebugLaserInvestigation = enable;
		g_HasLoggedLaserInfo = false;
		g_LaserLogFrameCount = 0;
		if (enable) {
			logger::info("[Laser] Investigation mode ENABLED");
		}
	}

	bool IsLaserInvestigationEnabled()
	{
		return g_DebugLaserInvestigation;
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
		// 验证thisPtr
		if (!thisPtr) {
			logger::error("BSCullingGroup thisPtr is null");
			D3DPERF_EndEvent();
			return;
		}

		// 验证apObj
		if (!IsValidObject(apObj)) {
			logger::error("Invalid object: 0x{:X}", (uintptr_t)apObj);
			D3DPERF_EndEvent();
			return;
		}

		// 验证aBound指针
		if (!IsValidPointer(aBound)) {
			const char* objName = (apObj && apObj->name.c_str()) ? apObj->name.c_str() : "unknown";
			logger::error("NiBound is invalid for object: {} (ptr: 0x{:X})", objName, reinterpret_cast<uintptr_t>(aBound));
			D3DPERF_EndEvent();
			return;
		}

		// 在瞄具渲染期间，跳过天气对象
		if (ScopeCamera::IsRenderingForScope()) {
			const char* objName = apObj->name.c_str();
			if (objName && strstr(objName, "Weather")) {
				D3DPERF_EndEvent();
				return;
			}

			// 激光调查日志
			if (g_DebugLaserInvestigation && g_LaserLogFrameCount < LASER_LOG_FRAMES) {
				if (objName && (strstr(objName, "Laser") || strstr(objName, "Beam"))) {
					logger::info("[LaserInvestigate] BSCullingGroup::Add laser: {}", objName);
				}
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
		logger::info("apFileName: {}", apFileName);
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
			// 第一次渲染（主相机）
			D3DPERF_BeginEvent(0xFF00FF00, L"TrueThroughScope_FirstPass");
			g_hookMgr->g_RenderPreUIOriginal(ptr_drawWorld);
			D3DPERF_EndEvent();
		} else {
			// 第二次渲染（瞄具相机）- 在 SecondPassRenderer 中已有标记
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

		// === 瞄具渲染：只添加激光几何体 ===

		auto p1stPerson = *ThroughScope::ptr_DrawWorld1stPerson;
		if (!p1stPerson) {
			g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);
			D3DPERF_EndEvent();
			return;
		}

		// 收集需要分离的非激光几何体
		std::vector<DetachedNodeInfo> detachedNodes;
		int laserCount = 0;

		// 处理第一人称节点树
		CollectNonLaserGeometries(p1stPerson, nullptr, 0, detachedNodes, laserCount);

		// 处理武器节点（激光可能挂载在此）
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

		// 调试日志
		if (g_DebugLaserInvestigation && g_LaserLogFrameCount < LASER_LOG_FRAMES) {
			logger::info("[Laser] Scope render: {} laser nodes, detaching {} geometries",
				laserCount, detachedNodes.size());
		}

		// 步骤1：从场景图分离非激光几何体
		for (auto& info : detachedNodes) {
			if (info.parent && info.node) {
				info.parent->DetachChild(info.node);
			}
		}

		// 步骤2：调用原函数（此时只能看到激光几何体）
		g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);

		// 步骤3：重新附加所有分离的几何体
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
			// 在瞄具渲染过程中重新应用光源状态
			// 启用光源数量限制优化：最多使用16个最重要的光源
			g_lightBackup->ApplyLightStatesForScope(false, 16);
		}

		D3DEventNode(g_hookMgr->g_DeferredLightsImplOriginal(ptr_drawWorld), L"hkDeferredLightsImpl");
	}

	/**
	 * @brief DrawWorld Forward 阶段钩子
	 *
	 * Forward 阶段渲染前向渲染的物体（如激光等半透明/发光效果）
	 */
	void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld)
	{
		// 调试日志
		if (g_DebugLaserInvestigation && g_LaserLogFrameCount < LASER_LOG_FRAMES) {
			bool isScope = ScopeCamera::IsRenderingForScope();
			logger::info("[Laser] Forward {} - Frame {}",
				isScope ? "Scope" : "Main", g_LaserLogFrameCount);
		}

		D3DHooks::SetForwardStage(true);
		D3DEventNode(g_hookMgr->g_ForwardOriginal(ptr_drawWorld), L"hkDrawWorld_Forward");
		D3DHooks::SetForwardStage(false);

		// 更新调试帧计数
		if (g_DebugLaserInvestigation && g_LaserLogFrameCount < LASER_LOG_FRAMES) {
			if (ScopeCamera::IsRenderingForScope()) {
				g_LaserLogFrameCount++;
			}
		}
	}

	void __fastcall hkPCUpdateMainThread(PlayerCharacter* pChar)
	{
		SHORT keyPgDown = GetAsyncKeyState(VK_NEXT);
		SHORT keyPgUp = GetAsyncKeyState(VK_PRIOR);

		if (keyPgUp & 0x1) {
			BSScrapArray<NiPointer<Actor>> aRetArray;
			std::vector<Actor*> actorList{};
			ProcessLists::GetSingleton()->GetActorsWithinRangeOfReference(g_pchar, 20000, &aRetArray);
			auto imadThermal = Utilities::GetFormFromMod("WestTekTacticalOptics.esp", 0x811)->As<TESImageSpaceModifier>();
			for (size_t i = 0; i < aRetArray.size(); i++) {
				Actor* actor = aRetArray[i].get();
				if (actor->formID != 0x7 && actor->formID != 0x14) {
					std::string actorName = actor->GetDisplayFullName();
					logger::info("Found Actor: {}", actorName.c_str());
				}
				actorList.push_back(actor);
			}

			logger::info("actorList size: {}", actorList.size());

			for (auto actor : actorList) {
				auto actorNode = actor->Get3D()->IsNode();
				if (!actorNode)
					continue;

				auto actorBase = actor->GetActorBase();
				if (!actorBase)
					continue;

				LogNodeHierarchy(actorNode);
				auto actorNodeChildren = actorNode->children.data();
				for (size_t j = 0; j < actorNode->children.size(); j++) {
					auto child = actorNodeChildren[j].get();
					if (!child)
						continue;

					auto childGeo = child->IsTriShape();
					if (childGeo) {
						auto lightShader = childGeo->QShaderProperty();
						auto lightShader2 = (BSLightingShaderProperty*)lightShader;
						auto lightMat = (BSLightingShaderMaterial*)lightShader2->material;
					}
				}
			}
		}
		if (keyPgDown & 0x1) {
			Utilities::LogPlayerWeaponNodes();
		}

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
}
