#include "HookManager.h"
#include "Utilities.h"
#include "ScopeCamera.h"

namespace ThroughScope
{
	using namespace Utilities;

	static HookManager* g_hookMgr = HookManager::GetSingleton();

	// ===== 简单转发的Hook函数 =====

	void hkDrawWorld_Move1stPersonToOrigin(uint64_t thisPtr)
	{
		D3DEventNode(g_hookMgr->g_DrawWorld_Move1stPersonToOrigin(thisPtr), L"DrawWorld_Move1stPersonToOrigin");
	}

	void hkBSBatchRenderer_Draw(BSRenderPass* apRenderPass)
	{
		D3DEventNode(g_hookMgr->g_originalBSBatchRendererDraw(apRenderPass), L"BSBatchRenderer_Draw");
	}

	void hkBSCullingGroup_SetCompoundFrustum(BSCullingGroup* thisPtr, BSCompoundFrustum* apCompoundFrustum)
	{
		D3DEventNode(g_hookMgr->g_BSCullingGroup_SetCompoundFrustum(thisPtr, apCompoundFrustum), L"BSCullingGroup_SetCompoundFrustum");
	}

	void __fastcall hkMainPreRender(Main* thisPtr, int auiDestination)
	{
		D3DEventNode(g_hookMgr->g_MainPreRender(thisPtr, auiDestination), L"Main_PreRender");
	}

	void __fastcall hkBegin(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_BeginOriginal(ptr_drawWorld), L"DrawWorld_Begin");
	}

	void hkMapDynamicTriShapeDynamicData(Renderer* renderer, BSDynamicTriShape* bsDynamicTriShape, DynamicTriShape* dynamicTriShape, DynamicTriShapeDrawData* drawdata, unsigned int auiSize)
	{
		D3DEventNode(g_hookMgr->g_MapDynamicTriShapeDynamicData(renderer, bsDynamicTriShape, dynamicTriShape, drawdata, auiSize), L"MapDynamicTriShapeDynamicData");
	}

	void __fastcall hkDrawWorld_LightUpdate(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_DrawWorldLightUpdateOriginal(ptr_drawWorld), L"DrawWorld_LightUpdate");
	}

	void __fastcall hkBSShaderAccumulator_RenderBlendedDecals(BSShaderAccumulator* thisPtr)
	{
		// 在瞄具渲染时，根据优化设置跳过贴花渲染
		// if (ScopeCamera::IsRenderingForScope()) {
		// 	return;
		// }
		D3DEventNode(g_hookMgr->g_BSShaderAccumulator_RenderBlendedDecals(thisPtr), L"BSShaderAccumulator_RenderBlendedDecals");
	}

	void __fastcall hkBSShaderAccumulator_RenderOpaqueDecals(BSShaderAccumulator* thisPtr)
	{
		// 在瞄具渲染时，根据优化设置跳过贴花渲染
		// if (ScopeCamera::IsRenderingForScope()) {
		// 	return;
		// }
		D3DEventNode(g_hookMgr->g_BSShaderAccumulator_RenderOpaqueDecals(thisPtr), L"BSShaderAccumulator_RenderOpaqueDecals");
	}

	void __fastcall hkDoUmbraQuery(uint64_t ptr_drawWorld)
	{
		D3DPERF_BeginEvent(0xffffffff, L"DoUmbraQuery");
		// 在瞄具渲染时跳过Umbra遮挡剔除查询，避免空指针崩溃
		if (ScopeCamera::IsRenderingForScope()) {
			D3DPERF_EndEvent();
			return;
		}
		g_hookMgr->g_DoUmbraQuery(ptr_drawWorld);
		D3DPERF_EndEvent();
	}

	void __fastcall hkBSCullingGroup_Process(BSCullingGroup* thisPtr, bool abFirstStageOnly)
	{
		D3DEventNode(g_hookMgr->g_BSCullingGroupProcessOriginal(thisPtr, abFirstStageOnly), L"BSCullingGroup_Process");
	}

	void __fastcall hkRTManager_CreateRenderTarget(RenderTargetManager rtm, int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent)
	{
		D3DEventNode(g_hookMgr->g_RTManagerCreateRenderTargetOriginal(rtm, aIndex, arProperties, aPersistent), L"RTManager_CreateRenderTarget");
	}

	void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr)
	{
		D3DEventNode(g_hookMgr->g_RefractionOriginal(this_ptr), L"hkDrawWorld_Refraction");
	}

	void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground)
	{
		D3DEventNode(g_hookMgr->g_DrawWorldAndUIOriginal(ptr_drawWorld, abBackground), L"hkMain_DrawWorldAndUI");
	}

	void __fastcall hkBSShaderAccumulator_ResetSunOcclusion(BSShaderAccumulator* thisPtr)
	{
		D3DEventNode(g_hookMgr->g_ResetSunOcclusionOriginal(thisPtr), L"hkBSShaderAccumulator_ResetSunOcclusion");
	}

	void __fastcall hkDecompressDepthStencilTarget(RenderTargetManager* thisPtr, int index)
	{
		D3DEventNode(g_hookMgr->g_DecompressDepthStencilTargetOriginal(thisPtr, index), L"hkDecompressDepthStencilTarget");
	}

	void __fastcall hkMainAccum(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_MainAccumOriginal(ptr_drawWorld), L"hkMainAccum");
	}

	void __fastcall hkBSDistantObjectInstanceRenderer_Render(uint64_t thisPtr)
	{
		// 在瞄具渲染时，根据优化设置跳过远景对象渲染
		if (ScopeCamera::IsRenderingForScope()) {
			return;
		}
		D3DEventNode(g_hookMgr->g_BSDistantObjectInstanceRenderer_RenderOriginal(thisPtr), L"hkBSDistantObjectInstanceRenderer_Render");
	}

	RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties)
	{
		D3DPERF_BeginEvent(0xffffffff, L"Renderer_CreateRenderTarget");
		auto result = g_hookMgr->g_Renderer_CreateRenderTarget(renderer, aId, apName, aProperties);
		D3DPERF_EndEvent();
		return result;
	}

	void __fastcall hkOcclusionMapRender()
	{
		// 在瞄具渲染时，根据优化设置跳过遮挡图渲染
		if (ScopeCamera::IsRenderingForScope()) {
			return;
		}
		D3DEventNode(g_hookMgr->g_OcclusionMapRender(), L"hkOcclusionMapRender");
	}

	void __fastcall hkMainRenderSetup(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_MainRenderSetupOriginal(ptr_drawWorld), L"hkMainRenderSetup");
	}

	void __fastcall hkOpaqueWireframe(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_OpaqueWireframeOriginal(ptr_drawWorld), L"hkOpaqueWireframe");
	}

	void __fastcall hkDeferredPrePass(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_DeferredPrePassOriginal(ptr_drawWorld), L"hkDeferredPrePass");
	}

	void __fastcall hkDeferredComposite(uint64_t ptr_drawWorld)
	{
		D3DEventNode(g_hookMgr->g_DeferredCompositeOriginal(ptr_drawWorld), L"hkDeferredComposite");
	}


}
