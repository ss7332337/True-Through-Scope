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
#include "ENBIntegration.h"
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

	// ========== fo4test Frame Generation Interop ==========
	// Motion Vector Deferral API - prevents FG ghosting by coordinating MV copy timing
	namespace FGInterop
	{
		typedef void (*FnNotifyComplete)();
		typedef int (*FnRegister)();
		typedef int (*FnUnregister)();
		
		static FnNotifyComplete NotifyComplete = nullptr;
		static FnRegister Register = nullptr;
		static FnUnregister Unregister = nullptr;
		static bool g_Initialized = false;
		static HMODULE g_Module = nullptr;
		
		void Initialize()
		{
			if (g_Initialized) return;
			
			g_Module = GetModuleHandleA("AAAFrameGeneration.dll");
			if (!g_Module) {
				logger::info("[FGInterop] AAAFrameGeneration.dll not loaded");
				return;
			}
			
			NotifyComplete = (FnNotifyComplete)GetProcAddress(g_Module, "AAAFG_NotifyMotionVectorUpdateComplete");
			Register = (FnRegister)GetProcAddress(g_Module, "AAAFG_RegisterMotionVectorDeferral");
			Unregister = (FnUnregister)GetProcAddress(g_Module, "AAAFG_UnregisterMotionVectorDeferral");
			
			if (NotifyComplete && Register && Unregister) {
				int count = Register();
				g_Initialized = true;
				logger::info("[FGInterop] Registered for MV deferral, count: {}", count);
			} else {
				logger::warn("[FGInterop] Failed to resolve API exports");
			}
		}
		
		void Shutdown()
		{
			if (g_Initialized && Unregister) {
				Unregister();
				logger::info("[FGInterop] Unregistered");
			}
			g_Initialized = false;
		}
		
		// Call after ApplyMotionVectorMask each frame
		void NotifyMVComplete()
		{
			if (g_Initialized && NotifyComplete) {
				NotifyComplete();
			}
		}
		
		bool IsActive() { return g_Initialized; }
	}

	// k1stPersonCullingGroup 地址，用于 O(1) 过滤第一人称几何体
	static REL::Relocation<BSCullingGroup*> ptr_k1stPersonCullingGroup{ REL::ID(731482) };

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

	// 检查几何体是否属于第一人称武器节点（通过遍历父节点链）
	static bool IsFirstPersonWeaponGeometry(NiAVObject* obj)
	{
		if (!obj) return false;
		
		// 遍历父节点链，查找是否在 "Weapon" 节点下
		NiNode* parent = obj->parent;
		while (parent) {
			const char* parentName = parent->name.c_str();
			if (parentName && strcmp(parentName, "Weapon") == 0) {
				return true;
			}
			parent = parent->parent;
		}
		return false;
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
		if (!thisPtr || !IsValidObject(apObj) || !IsValidPointer(aBound)) {
			return;
		}

		// O(1) 过滤：在 scope 渲染时跳过第一人称裁剪组的所有 Add 调用
		// 这允许 StartAdding 正常执行（初始化组），但阻止几何体被添加
		// 解决了之前的问题：
		// - 跳过整个 Add1stPersonGeomToCuller 导致组未初始化 -> BSMTAManager 崩溃
		// - 使用 DetachChild/AttachChild 导致竞态条件 -> DoBuildGeomArray 崩溃
		// - 遍历父节点链过滤导致性能问题
		if (ScopeCamera::IsRenderingForScope() && 
			thisPtr == ptr_k1stPersonCullingGroup.get()) {
			D3DPERF_EndEvent();
			return;
		}

		g_hookMgr->g_BSCullingGroupAdd(thisPtr, apObj, aBound, aFlags);
	}

	/**
	 * @brief Add1stPersonGeomToCuller 钩子
	 *
	 * 注意：此函数不再修改场景图结构！
	 * 
	 * 之前的实现使用 DetachChild/AttachChild 临时分离非激光几何体，
	 * 但这会导致竞态条件崩溃：
	 * - 主线程在此处修改场景图
	 * - 工作线程 (JobListManager::ServingThread) 在 BSFadeNode::ComputeGeomArray 中遍历场景图
	 * - 当镜头移动较快时，两者同时操作导致 DoBuildGeomArray::Recurse 访问 null 指针
	 *
	 * 现在改为在 hkBSCullingGroupAdd 中过滤非激光几何体，不修改场景图结构。
	 */
	void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr)
	{
		D3DPERF_BeginEvent(0xffffffff, L"Add1stPersonGeomToCuller");
		// 始终调用原函数：
		// - StartAdding 会正常执行，初始化 k1stPersonCullingGroup
		// - Add 调用会被 hkBSCullingGroupAdd 中的 O(1) 过滤拦截
		g_hookMgr->g_Add1stPersonGeomToCullerOriginal(thisPtr);
		D3DPERF_EndEvent();
	}

	/**
	 * @brief BSShaderAccumulator::RegisterObject 验证 hook
	 *
	 * 防止 BSMTAManager 工作线程因悬空指针崩溃。
	 * 当敌人死亡（特别是爆头）时，NPC 几何体被销毁，但 BSMTAManager 任务队列
	 * 可能仍持有这些几何体的指针。此 hook 在注册前验证几何体指针有效性。
	 *
	 * 崩溃特征：
	 * - 地址 0xFFFFFFFFFFFFFFFF（无效指针标记）
	 * - 调用栈包含 BSMTAManager::RegisterObjectTask::Execute
	 * - 几何体名称如 "FemaleNeckGore"（伤口网格）
	 */
	bool __fastcall hkBSShaderAccumulator_RegisterObject(BSShaderAccumulator* thisPtr, BSGeometry* apGeometry)
	{
		// 快速验证：null 指针或明显无效的指针值
		if (!apGeometry) {
			return false;
		}
		
		uintptr_t addr = reinterpret_cast<uintptr_t>(apGeometry);
		
		// 检查常见的无效指针值
		if (addr == 0xFFFFFFFFFFFFFFFF || 
			addr < 0x10000 ||  // 太低，可能是小整数误用
			(addr & 0x7) != 0) {  // 未对齐，BSGeometry 应该是8字节对齐
			return false;
		}
		
		// 使用 SEH 捕获访问冲突（可能导致的性能开销很小）
		__try {
			// 尝试读取 refCount 验证指针可访问
			volatile uint32_t refCount = apGeometry->refCount;
			(void)refCount;
			
			// 检查 refCount 是否合理（0 表示对象可能已销毁）
			if (apGeometry->refCount == 0) {
				return false;
			}
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			// 指针无效，跳过注册
			return false;
		}
		
		// 指针有效，调用原函数
		return g_hookMgr->g_RegisterObjectOriginal(thisPtr, apGeometry);
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


	void __fastcall hkBSShaderAccumulator_RenderBatches(
		BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup)
	{
		g_hookMgr->g_BSShaderAccumulatorRenderBatches(thisPtr, aiShader, abAlphaPass, aeGroup);
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
		auto enbIntegration = ENBIntegration::GetSingleton();

		if (!weaponInfo.currentConfig) {
			D3DHooks::SetEnableRender(false);
			enbIntegration->SetAiming(false);  // 通知 ENB 不在开镜
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
			enbIntegration->SetAiming(false);  // 通知 ENB 不在开镜
		} else {
			if (IsInADS(g_pchar)) {
				D3DHooks::HandleFOVInput();
				D3DHooks::SetEnableRender(true);
				enbIntegration->SetAiming(true);  // 通知 ENB 正在开镜，禁用鬼影效果
			}
		}

		if (!IsInADS(g_pchar)) {
			D3DHooks::SetEnableRender(false);
			enbIntegration->SetAiming(false);  // 通知 ENB 不在开镜
		}

		g_hookMgr->g_PCUpdateMainThread(pChar);
	}

	// TLS 相关的静态重定位对象
	static REL::Relocation<uint32_t*> ptr_tls_index_sky{ REL::ID(842564) };
	static REL::Relocation<uintptr_t*> ptr_DefaultContext_sky{ REL::ID(33539) };

	// 星星世界缩放因子（硬编码）
	// 这个值用于补偿瞄具场景中 viewProjMat 与主相机的缩放差异
	// 经测试，101 倍可以使星星大小和位置移动与主场景一致
	static constexpr float STAR_WORLD_SCALE_FACTOR = 150.0f;

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
	// 当 aiLast=13 时（原始函数调用前）是渲染瞄具的理想时机，可以获得 ENB 效果
	void __fastcall hkImageSpaceManager_RenderEffectRange(void* thisPtr, int aiFirst, int aiLast, int aiSourceTarget, int aiDestTarget)
	{
		// 延迟初始化 FG interop（只执行一次）
		static bool s_fgInteropInitialized = false;
		if (!s_fgInteropInitialized) {
			FGInterop::Initialize();
			s_fgInteropInitialized = true;
		}

		// 用 D3DPERF 标记显示调用参数
		wchar_t markerName[128];
		swprintf_s(markerName, L"RenderEffectRange(%d-%d, src=%d, dst=%d)", aiFirst, aiLast, aiSourceTarget, aiDestTarget);
		D3DPERF_BeginEvent(0xFFFF00FF, markerName);

		// 在效果 13 之前（线性空间/HDR阶段结束）渲染瞄具
		// 这样瞄具内容会被后续的 Post-Processing (ENB, TAA, ToneMapping) 处理
		if (aiLast == 13) {
			auto renderStateMgr = RenderStateManager::GetSingleton();
			auto enbIntegration = ENBIntegration::GetSingleton();
			
			if (renderStateMgr->IsScopeReady() && 
				renderStateMgr->IsRenderReady() && 
				D3DHooks::IsEnableRender() &&
				!ScopeCamera::IsRenderingForScope()) {
				
				// 此时必须使用直接渲染，因为我们需要修改 G-Buffer/HDR Buffer 供 ENB 使用
				// 不需要 ENB 回调，因为我们是在 ENB 处理之前渲染
				ID3D11DeviceContext* context = d3dHooks->GetContext();
				ID3D11Device* device = d3dHooks->GetDevice();
				
				if (context && device) {
					D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_SecondPass_PreEffects");
					SecondPassRenderer renderer(context, device, d3dHooks);
					if (!renderer.ExecuteSecondPass()) {
						logger::warn("[RenderEffectRange Hook] SecondPassRenderer failed");
					}
					// 渲染后立即清除瞄具区域的 Motion Vectors，防止 TAA 鬼影
					renderer.ApplyMotionVectorMask(); 
					
					// 通知 fo4test MV 更新完成（Frame Generation 兼容）
					FGInterop::NotifyMVComplete();
					
					D3DPERF_EndEvent();
					g_scopeRenderMgr->OnFrameEnd();
				}
			}
		}

		// 调用原始函数
		g_hookMgr->g_ImageSpaceManager_RenderEffectRange(thisPtr, aiFirst, aiLast, aiSourceTarget, aiDestTarget);

		D3DPERF_EndEvent();
		// 用 D3DPERF 标记显示调用参数
		//wchar_t markerName[128];
		//swprintf_s(markerName, L"RenderEffectRange(%d-%d, src=%d, dst=%d)", aiFirst, aiLast, aiSourceTarget, aiDestTarget);
		//D3DPERF_BeginEvent(0xFFFF00FF, markerName);

		//// 调用原始函数
		//g_hookMgr->g_ImageSpaceManager_RenderEffectRange(thisPtr, aiFirst, aiLast, aiSourceTarget, aiDestTarget);

		//D3DPERF_EndEvent();
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

	// ========== DrawWorld::Render_PostUI Debug Hook ==========
	void __fastcall hkDrawWorld_Render_PostUI()
	{
		D3DPERF_BeginEvent(0xFF00FF00, L"DrawWorld_Render_PostUI");
		g_hookMgr->g_DrawWorld_Render_PostUI();
		
		// MV 调试可视化 - 在右上角显示 RT_29
		SecondPassRenderer::RenderMVDebugOverlay();
		
		D3DPERF_EndEvent();
	}

	// ========== UI::ScreenSpace_RenderMenus Debug Hook ==========
	// 用于渲染菜单（比如暂停菜单、工作台菜单、物品菜单等）
	void __fastcall hkUI_ScreenSpace_RenderMenus(void* thisPtr)
	{
		D3DPERF_BeginEvent(0xFFFF00FF, L"UI_ScreenSpace_RenderMenus");
		g_hookMgr->g_UI_ScreenSpace_RenderMenus(thisPtr);
		D3DPERF_EndEvent();
	}
}
