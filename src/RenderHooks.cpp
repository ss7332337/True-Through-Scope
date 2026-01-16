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
#include "rendering/RenderTargetMerger.h"
#include "rendering/RenderStateManager.h"
#include "ENBIntegration.h"
#include "FGCompatibility.h"
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
	// Supports two modes:
	// 1. XifeiliAPI: Uses exported functions from fo4test_xifeili (preferred)
	// 2. VanillaHook: Uses FGCompatibility for vanilla fo4test (Hook-based)
	namespace FGInterop
	{
		enum class Mode {
			None,           // FG not detected
			XifeiliAPI,     // fo4test_xifeili with exported API
			VanillaHook     // vanilla fo4test with hook-based FGCompatibility
		};
		
		// Function pointers for xifeili API
		typedef void (*FnNotifyComplete)();
		typedef int (*FnRegister)();
		typedef int (*FnUnregister)();
		typedef ID3D11RenderTargetView* (*FnGetMVOverrideMaskRTV)();
		typedef int (*FnRegisterMVRegionOverride)();
		typedef int (*FnUnregisterMVRegionOverride)();
		
		static FnNotifyComplete NotifyComplete = nullptr;
		static FnRegister Register = nullptr;
		static FnUnregister Unregister = nullptr;
		static FnGetMVOverrideMaskRTV GetMVOverrideMaskRTV = nullptr;
		static FnRegisterMVRegionOverride RegisterMVRegionOverride = nullptr;
		static FnUnregisterMVRegionOverride UnregisterMVRegionOverride = nullptr;
		static bool g_Initialized = false;
		static HMODULE g_Module = nullptr;
		static bool g_TryInitFGInterop = false;
		static Mode g_Mode = Mode::None;
		
		void Initialize()
		{
			if (g_TryInitFGInterop) return;

			g_TryInitFGInterop = true;

			if (g_Initialized) return;
			
			g_Module = GetModuleHandleA("AAAFrameGeneration.dll");
			if (!g_Module) {
				logger::info("[FGInterop] AAAFrameGeneration.dll not loaded");
				g_Mode = Mode::None;
				return;
			}
			
			// Try to resolve fo4test_xifeili API first
			NotifyComplete = (FnNotifyComplete)GetProcAddress(g_Module, "AAAFG_NotifyMotionVectorUpdateComplete");
			Register = (FnRegister)GetProcAddress(g_Module, "AAAFG_RegisterMotionVectorDeferral");
			Unregister = (FnUnregister)GetProcAddress(g_Module, "AAAFG_UnregisterMotionVectorDeferral");
			
			GetMVOverrideMaskRTV = (FnGetMVOverrideMaskRTV)GetProcAddress(g_Module, "AAAFG_GetMVOverrideMaskRTV");
			RegisterMVRegionOverride = (FnRegisterMVRegionOverride)GetProcAddress(g_Module, "AAAFG_RegisterMVRegionOverride");
			UnregisterMVRegionOverride = (FnUnregisterMVRegionOverride)GetProcAddress(g_Module, "AAAFG_UnregisterMVRegionOverride");
			
			if (NotifyComplete && Register && Unregister) {
				// fo4test_xifeili detected - use its API
				int count = Register();
				g_Initialized = true;
				g_Mode = Mode::XifeiliAPI;
				logger::info("[FGInterop] Using XifeiliAPI mode, registered count: {}", count);
				
				if (RegisterMVRegionOverride && GetMVOverrideMaskRTV) {
					int overrideCount = RegisterMVRegionOverride();
					logger::info("[FGInterop] MV Region Override API available, registered count: {}", overrideCount);
				}
			} else {
				// Vanilla fo4test detected - use FGCompatibility hook
				logger::info("[FGInterop] Xifeili API not available, using VanillaHook mode with FGCompatibility");
				
				auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
				if (rendererData && rendererData->context) {
					auto context = (ID3D11DeviceContext*)rendererData->context;
					if (FGCompatibility::Initialize(context)) {
						g_Initialized = true;
						g_Mode = Mode::VanillaHook;
						logger::info("[FGInterop] VanillaHook mode initialized successfully");
					} else {
						logger::warn("[FGInterop] Failed to initialize VanillaHook mode");
						g_Mode = Mode::None;
					}
				}
			}
		}
		
		void Shutdown()
		{
			if (g_Initialized) {
				if (g_Mode == Mode::XifeiliAPI) {
					if (UnregisterMVRegionOverride) {
						UnregisterMVRegionOverride();
					}
					if (Unregister) {
						Unregister();
					}
				} else if (g_Mode == Mode::VanillaHook) {
					FGCompatibility::Shutdown();
				}
				logger::info("[FGInterop] Unregistered");
			}
			g_Initialized = false;
			g_Mode = Mode::None;
		}
		
		// Notify that motion vector updates are complete for this frame
		// For VanillaHook mode, this triggers the MV copy with mask
		void NotifyMVComplete()
		{
			if (!g_Initialized) return;
			
			if (g_Mode == Mode::XifeiliAPI && NotifyComplete) {
				NotifyComplete();
			} else if (g_Mode == Mode::VanillaHook) {
				// For VanillaHook mode, execute the MV copy with mask
				auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
				if (rendererData && rendererData->context) {
					auto context = (ID3D11DeviceContext*)rendererData->context;
					FGCompatibility::ExecuteMVCopyWithMask(context);
				}
			}
		}
		
		// Get the RTV for writing to the MV override mask
		ID3D11RenderTargetView* GetMaskRTV()
		{
			if (!g_Initialized) return nullptr;
			
			if (g_Mode == Mode::XifeiliAPI && GetMVOverrideMaskRTV) {
				return GetMVOverrideMaskRTV();
			} else if (g_Mode == Mode::VanillaHook) {
				return FGCompatibility::GetMaskRTV();
			}
			return nullptr;
		}
		
		// Clear the mask at the start of each frame (VanillaHook mode only)
		void ClearMask()
		{
			if (!g_Initialized) return;
			
			if (g_Mode == Mode::VanillaHook) {
				auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
				if (rendererData && rendererData->context) {
					auto context = (ID3D11DeviceContext*)rendererData->context;
					FGCompatibility::ClearMask(context);
				}
			}
			// XifeiliAPI mode: mask is cleared by fo4test automatically
		}
		
		bool IsActive() { return g_Initialized; }
		bool IsMaskAPIAvailable() { 
			if (!g_Initialized) return false;
			if (g_Mode == Mode::XifeiliAPI) return GetMVOverrideMaskRTV != nullptr;
			if (g_Mode == Mode::VanillaHook) return FGCompatibility::IsMaskAPIAvailable();
			return false;
		}
		Mode GetCurrentMode() { return g_Mode; }
	}


	// k1stPersonCullingGroup 地址
	// 注意：k1stPersonCullingGroup 是一个 BSCullingGroup 对象，不是指针
	// REL::ID(731482) 返回的是这个对象的地址，使用 .get() 获取地址进行比较
	static REL::Relocation<BSCullingGroup*> ptr_k1stPersonCullingGroup{ REL::ID(731482) };
	
	// 调试计数器
	static int g_DebugFilterCount = 0;

	// ========== 激光节点识别和添加 ==========

	// 检查是否是 AutoBeam 激光节点名称
	inline bool IsAutoBeamLaserName(const char* name)
	{
		if (!name || name[0] == '\0') return false;
		// AutoBeam 会将激光重命名为 "_LaserBeam" 或 "_LaserDot"
		return (strcmp(name, "_LaserBeam") == 0 || strcmp(name, "_LaserDot") == 0);
	}

	// 递归遍历场景树，查找并添加激光节点到裁剪组
	void AddLaserNodesToCullingGroup(NiAVObject* node, BSCullingGroup* cullingGroup)
	{
		if (!node || !cullingGroup) return;

		// 检查当前节点是否是激光
		const char* nodeName = node->name.c_str();
		if (IsAutoBeamLaserName(nodeName)) {
			// 找到激光节点，添加到裁剪组
			// 使用节点的 worldBound 成员
			g_hookMgr->g_BSCullingGroupAdd(cullingGroup, node, &node->worldBound, 0);
			return; // 激光节点不需要继续遍历子节点
		}

		// 如果是节点（有子对象），递归遍历子节点
		if (node->IsNode()) {
			NiNode* niNode = static_cast<NiNode*>(node);
			for (uint32_t i = 0; i < niNode->children.size(); ++i) {
				auto child = niNode->children[i].get();
				if (child) {
					AddLaserNodesToCullingGroup(child, cullingGroup);
				}
			}
		}
	}

	// 通用激光名称检查（保留用于其他用途）
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

	
	void __fastcall hkRenderer_DoZPrePass(uint64_t thisPtr, NiCamera* apFirstPersonCamera, NiCamera* apWorldCamera, float afFPNear, float afFPFar, float afNear, float afFar)
	{
		// [必需] 跳过第一人称深度预处理
		// 这是手型洞修复的必要前置条件，配合 hkBSShaderAccumulator_RenderBatches 的 Forward-stage 过滤
		// 详见 docs/手型洞修复方案.md
		if (ScopeCamera::IsRenderingForScope()) {
			*FPZPrePassDrawDataCount = 0;
			*FPAlphaTestZPrePassDrawDataCount = 0;
		}
		D3DEventNode(g_hookMgr->g_pDoZPrePassOriginal(thisPtr, apFirstPersonCamera, apWorldCamera, afFPNear, afFPFar, afNear, afFar), L"hkRenderer_DoZPrePass");
	}


	void hkBSCullingGroupAdd(BSCullingGroup* thisPtr,
		NiAVObject* apObj,
		const NiBound* aBound,
		const unsigned int aFlags)
	{
		if (!thisPtr || !IsValidObject(apObj) || !IsValidPointer(aBound)) {
			return;
		}

		// [NOTE] 第一人称裁剪组过滤已移至 hkBSShaderAccumulator_RenderBatches
		// 使用 Forward-stage aware 过滤实现：
		// - Deferred 阶段跳过第一人称累加器（阻止手部渲染）
		// - Forward 阶段允许第一人称累加器（保留激光渲染）
		// 详见 docs/手型洞修复方案.md
		
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
	 */
	void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr)
	{
		D3DPERF_BeginEvent(0xffffffff, L"Add1stPersonGeomToCuller");
		// 始终调用原函数
		// 第一人称过滤通过 hkRenderer_DoZPrePass + hkBSCullingGroupAdd 实现
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
		
		// 使用 SEH 捕获访问冲突
		__try {
			// 1. 验证 refCount
			if (apGeometry->refCount == 0) {
				return false;
			}

			// 2. 验证虚表 (VTable)
			// 如果指针指向已被释放并重新用于其他用途的内存，refCount 可能恰好不为0
			// 但虚表通常会不同或无效
			uintptr_t vtable = *reinterpret_cast<uintptr_t*>(apGeometry);
			
			// 检查虚表地址是否合理
			if (vtable < 0x10000 || vtable == 0xFFFFFFFFFFFFFFFF || (vtable & 0x7) != 0) {
				return false;
			}

			// 尝试读取虚表内容（确保虚表指向有效内存）
			// 检查第一个虚函数（通常是析构函数或 RTTI）
			uintptr_t funcPtr = *reinterpret_cast<uintptr_t*>(vtable);
			if (funcPtr < 0x10000 || funcPtr == 0xFFFFFFFFFFFFFFFF) {
				return false;
			}
			
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			// 指针无效或虚表无法访问，跳过注册
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
			
			// Clear the FG interpolation mask at the start of each frame (VanillaHook mode only)
			FGInterop::ClearMask();
			
			g_hookMgr->g_RenderPreUIOriginal(ptr_drawWorld);
			D3DPERF_EndEvent();

			// [FIX] 移动 Scope 渲染到 Render_PreUI 之后 (Post-Forward, Pre-ImageSpace)
			// 之前的 RenderEffectRange (aiLast=21) 太晚了，导致 Scope 画面缺失 ImageSpace 效果(如模糊)
			const auto renderStateMgr = RenderStateManager::GetSingleton();

			if (renderStateMgr->IsScopeReady() && 
				renderStateMgr->IsRenderReady() && 
				D3DHooks::IsEnableRender()) {

				ID3D11DeviceContext* context = nullptr;
				ID3D11Device* device = nullptr;
				
				// 尝试获取 Context/Device. 如果 d3dHooks 指针不可用，尝试通过 RendererData 获取
				if (d3dHooks) {
					context = d3dHooks->GetContext();
					device = d3dHooks->GetDevice();
				} else {
					// Fallback if d3dHooks global is not visible immediately
					auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
					if (rendererData) context = (ID3D11DeviceContext*)rendererData->context;
					// Device logic needed if d3dHooks missing... but assuming d3dHooks works for now based on other funcs.
				}

				if (context && d3dHooks) { // 需要 d3dHooks 实例来传递给 SecondPassRenderer
					device = d3dHooks->GetDevice(); // Ensure device matches
					
					D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_SecondPass");
					SecondPassRenderer renderer(context, device, d3dHooks);
					if (!renderer.ExecuteSecondPass()) {
						logger::warn("[Render_PreUI Hook] SecondPassRenderer failed");
					}

					// Merge Render Targets
					RenderTargetMerger::GetInstance().MergeRenderTargets(context, device);

					if (!FGInterop::IsActive()) FGInterop::Initialize();

					if (FGInterop::IsMaskAPIAvailable()) {
						ID3D11RenderTargetView* maskRTV = FGInterop::GetMaskRTV();
						if (maskRTV) {
							renderer.WriteToMVRegionOverrideMask(maskRTV);
						}
					}
					
					FGInterop::NotifyMVComplete();
					
					D3DPERF_EndEvent();
					g_scopeRenderMgr->OnFrameEnd();
				}
			}

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

	void __fastcall hkBSShaderAccumulator_RenderBatches(
		BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup)
	{
		// [FIX] Forward-stage aware 第一人称过滤
		// RenderDoc 分析显示：
		// - 手部在 Deferred 阶段渲染 (GBuffer 填充, Event ~8917)
		// - 激光在 Forward 阶段渲染 (透明/发光效果, Event ~25243)
		// 
		// 策略：
		// - Deferred 阶段：跳过第一人称累加器（阻止手部渲染）
		// - Forward 阶段：允许第一人称累加器（保留激光渲染）
		if (ScopeCamera::IsRenderingForScope()) {
			auto firstPersonAccum = *ptr_Draw1stPersonAccum;
			if (thisPtr == firstPersonAccum) {
				// 只在 Forward 阶段允许第一人称渲染（激光在这里）
				if (!D3DHooks::GetForwardStage()) {
					// Deferred 阶段 - 跳过第一人称（阻止手部）
					return;
				}
				// Forward 阶段 - 允许第一人称（保留激光）
			}
		}
		
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

		// Capture RT4 viewport for Upscaling compatibility
		// This works for both Upscaling (DLSS/FSR3) and non-Upscaling cases:
		// - With Upscaling: RT4 viewport = internal render resolution (lower than output)
		// - Without Upscaling: RT4 viewport = backbuffer resolution (full resolution)
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (rendererData && rendererData->context) {
			auto context = (ID3D11DeviceContext*)rendererData->context;

			// Get RT4's texture description to determine the actual rendering resolution
			ID3D11Texture2D* rt4Texture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
			if (rt4Texture) {
				D3D11_TEXTURE2D_DESC texDesc;
				rt4Texture->GetDesc(&texDesc);

				// Create viewport from RT4 texture dimensions
				D3D11_VIEWPORT rt4Viewport = {};
				rt4Viewport.TopLeftX = 0.0f;
				rt4Viewport.TopLeftY = 0.0f;
				rt4Viewport.Width = static_cast<float>(texDesc.Width);
				rt4Viewport.Height = static_cast<float>(texDesc.Height);
				rt4Viewport.MinDepth = 0.0f;
				rt4Viewport.MaxDepth = 1.0f;

				// Store for later use in SetScopeTexture and SecondPassRenderer
				RenderUtilities::SetFirstPassViewport(rt4Viewport);
			}
		}

		auto scopeRenderMgr = ScopeRenderingManager::GetSingleton();
		if (scopeRenderMgr->IsUpscalingActive()) {
			D3DPERF_BeginEvent(0xFF00AAFF, L"[FO4TEST_TIMING] After_Forward_CopyBuffersToShared");
			D3DPERF_EndEvent();
		}
	}

	void __fastcall hkPCUpdateMainThread(PlayerCharacter* pChar)
	{
		// [FIX] 每帧更新 Chameleon 隐身效果状态
		// 当玩家处于隐身状态时，禁用 scope 渲染相关的几何体过滤以避免 D3D11 崩溃
		g_scopeRenderMgr->UpdateChameleonStatus();

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
				// [FIX] 检查 Chameleon 透明效果是否激活
				// 只在透明效果实际激活时禁用，允许蹲下但尚未透明时正常使用
				if (g_scopeRenderMgr->IsChameleonEffectActive()) {
					D3DHooks::SetEnableRender(false);
					enbIntegration->SetAiming(false);
				} else {
					D3DHooks::HandleFOVInput();
					D3DHooks::SetEnableRender(true);
					enbIntegration->SetAiming(true);  // 通知 ENB 正在开镜，禁用鬼影效果
				}
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

		// 调用原始函数
		g_hookMgr->g_ImageSpaceManager_RenderEffectRange(thisPtr, aiFirst, aiLast, aiSourceTarget, aiDestTarget);

		D3DPERF_EndEvent();
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
