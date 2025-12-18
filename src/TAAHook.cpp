#include "HookManager.h"
#include "Utilities.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "GlobalTypes.h"
#include "rendering/RenderStateManager.h"
#include "rendering/LightBackupSystem.h"
#include "rendering/SecondPassRenderer.h"
#include "rendering/ScopeRenderingManager.h"
#include <cmath>
#include <algorithm>

namespace ThroughScope
{
	using namespace Utilities;

	static HookManager* g_hookMgr = HookManager::GetSingleton();
	static RenderStateManager* g_renderStateMgr = RenderStateManager::GetSingleton();
	static LightBackupSystem* g_lightBackup = LightBackupSystem::GetSingleton();
	static ScopeRenderingManager* g_scopeRenderMgr = ScopeRenderingManager::GetSingleton();

	void __fastcall hkTAA(ImageSpaceEffectTemporalAA* thisPtr, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param)
	{
		// 检查原始函数指针是否有效
		if (!g_hookMgr->g_TAA) {
			logger::error("g_hookMgr->g_TAA is null! Cannot call original function");
			return;
		}

		// 在执行TAA之前捕获LUT纹理
		ID3D11DeviceContext* context = d3dHooks->GetContext();
		if (context && D3DHooks::IsEnableRender()) {
			// 捕获当前绑定的LUT纹理 (t3, t4, t5, t6)
			D3DHooks::CaptureLUTTextures(context);
		}

		// 1. 先执行原本的TAA (添加调试标记)
		D3DPERF_BeginEvent(0xFFFF0000, L"ImageSpaceEffect_TAA");
		g_hookMgr->g_TAA(thisPtr, a_geometry, a_param);
		D3DPERF_EndEvent();

		// 检查是否可以执行第二次渲染
		if (!g_renderStateMgr->IsScopeReady() || !g_renderStateMgr->IsRenderReady() || !D3DHooks::IsEnableRender()) {
			return;
		}

		// 获取D3D资源
		ID3D11Device* device = d3dHooks->GetDevice();
		if (!device || !context) {
			logger::error("D3D device or context is null");
			return;
		}

		// ========== fo4test 兼容模式 ==========
		// 如果启用了 fo4test 兼容，场景渲染已在 Forward 阶段完成
		// 这里只需执行后处理（HDR、热成像等）
		if (g_scopeRenderMgr->IsFO4TestCompatibilityEnabled()) {
			if (g_scopeRenderMgr->IsSceneRenderingCompleteThisFrame()) {
				D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_PostProcessing_FO4TestCompat");
				g_scopeRenderMgr->ExecutePostProcessingPhase();
				D3DPERF_EndEvent();
			}
			// 不管成功与否，重置帧状态
			g_scopeRenderMgr->OnFrameEnd();
			return;
		}

		// ========== 标准模式 ==========
		// 没有 fo4test，使用原来的完整渲染流程
		logger::info("[TAAHook] Using STANDARD rendering mode");
		D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_SecondPass");
		SecondPassRenderer renderer(context, device, d3dHooks);
		if (!renderer.ExecuteSecondPass()){
			logger::warn("[HDR-DEBUG] hkTAA: ExecuteSecondPass returned FALSE");
		}
		D3DPERF_EndEvent();
	}
}
