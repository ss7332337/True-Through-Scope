#include "HookManager.h"
#include "Utilities.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "GlobalTypes.h"
#include "rendering/RenderStateManager.h"
#include "rendering/LightBackupSystem.h"
#include "rendering/SecondPassRenderer.h"
#include <cmath>
#include <algorithm>

namespace ThroughScope
{
	using namespace Utilities;

	static HookManager* g_hookMgr = HookManager::GetSingleton();
	static RenderStateManager* g_renderStateMgr = RenderStateManager::GetSingleton();
	static LightBackupSystem* g_lightBackup = LightBackupSystem::GetSingleton();

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

		// 1. 先执行原本的TAA
		g_hookMgr->g_TAA(thisPtr, a_geometry, a_param);

		//if (ScopeCamera::IsRenderingForScope())
		//	return;

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

		// 创建第二次渲染器并执行渲染
		SecondPassRenderer renderer(context, device, d3dHooks);
		if (!renderer.ExecuteSecondPass()) {
			// logger::warn("Second pass rendering failed or was skipped");
		}
	}
}
