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



		// 1. 先执行原本的TAA (添加调试标记)
		D3DPERF_BeginEvent(0xFFFF0000, L"ImageSpaceEffect_TAA");
		g_hookMgr->g_TAA(thisPtr, a_geometry, a_param);
		D3DPERF_EndEvent();

		// ========== 瞄具渲染已移至 ImageSpaceManager::RenderEffectRange(15-21) ==========
		// 原因: ImageSpaceManager::RenderEffectRange(15-21) 在所有 3D 渲染（包括 HDR, TAA, DOF）完成后、
		//       UI 渲染开始前调用，这是一劳永逸的渲染时机
		// 参见: RenderHooks.cpp 中的 hkUI_BeginRender 函数
		
		// [DISABLED] 以下代码已禁用，保留作为参考
		// 检查是否可以执行第二次渲染
		// 现在由 hkUI_BeginRender 处理
		/*
		if (!g_renderStateMgr->IsScopeReady() || !g_renderStateMgr->IsRenderReady() || !D3DHooks::IsEnableRender()) {
			return;
		}

		// 获取D3D资源
		ID3D11Device* device = d3dHooks->GetDevice();
		if (!device || !context) {
			logger::error("D3D device or context is null");
			return;
		}

		// fo4test 兼容模式 / 标准模式渲染已移至 UI::BeginRender
		D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_SecondPass");
		SecondPassRenderer renderer(context, device, d3dHooks);
		if (!renderer.ExecuteSecondPass()){
			logger::warn("[HDR-DEBUG] hkTAA: ExecuteSecondPass returned FALSE");
		}
		D3DPERF_EndEvent();
		*/
	}
}
