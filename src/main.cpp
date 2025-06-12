#include "Constants.h"
#include "D3DHooks.h"
#include "RenderUtilities.h"
#include "ScopeCamera.h"
#include "Utilities.h"
#include <EventHandler.h>
#include <mutex>
#include <NiFLoader.h>
#include <string>
#include <thread>
#include <Windows.h>
#include <winternl.h>

#include "DataPersistence.h"
#include "ImGuiManager.h"



using namespace RE;
using namespace RE::BSGraphics;

#pragma region Func
#pragma region DrawWorld_MainRenderFn
REL::Relocation<uintptr_t> DrawWorld_Render_PreUI_Ori{ REL::ID(984743) };
REL::Relocation<uintptr_t> DrawWorld_MainAccum_Ori{ REL::ID(718911) };
REL::Relocation<uintptr_t> DrawWorld_OcclusionMapRender_Ori{ REL::ID(426737) };
REL::Relocation<uintptr_t> DrawWorld_MainRenderSetup_Ori{ REL::ID(339369) };
REL::Relocation<uintptr_t> DrawWorld_OpaqueWireframe_Ori{ REL::ID(1268987) };
REL::Relocation<uintptr_t> DrawWorld_DeferredPrePass_Ori{ REL::ID(56596) };
REL::Relocation<uintptr_t> DrawWorld_DeferredLightsImpl_Ori{ REL::ID(1108521) };
REL::Relocation<uintptr_t> DrawWorld_DeferredComposite_Ori{ REL::ID(728427) };
REL::Relocation<uintptr_t> DrawWorld_Forward_Ori{ REL::ID(656535) };
REL::Relocation<uintptr_t> DrawWorld_Refraction_Ori{ REL::ID(1572250) };
#pragma region DrawWorld_SubFn

REL::Relocation<uintptr_t> DrawWorld_Add1stPersonGeomToCuller_Ori{ REL::ID(414086) };
REL::Relocation<uintptr_t> BSShaderAccumulator_RenderBatches_Ori{ REL::ID(1048494) };
REL::Relocation<uintptr_t> BSShaderAccumulator_RenderOpaqueDecals_Ori{ REL::ID(163409) };
REL::Relocation<uintptr_t> BSShaderAccumulator_RenderBlendedDecals_Ori{ REL::ID(761249) };

#pragma endregion

#pragma endregion
#pragma region Main
REL::Relocation<uintptr_t> DrawWorld_Begin_Ori{ REL::ID(502840) };
REL::Relocation<uintptr_t> Main_DrawWorldAndUI_Ori{ REL::ID(408683) };
REL::Relocation<uintptr_t> PCUpdateMainThread_Ori{ REL::ID(1134912) };
#pragma endregion

REL::Relocation<uintptr_t> BSCullingGroup_Process_Ori{ REL::ID(1147875) };
REL::Relocation<uintptr_t> Renderer_CreateRenderTarget_Ori{ REL::ID(425575) };
REL::Relocation<uintptr_t> RTM_CreateRenderTarget_Ori{ REL::ID(43433) };

REL::Relocation<uintptr_t> Renderer_DoZPrePass_Ori{ REL::ID(1491502) };
REL::Relocation<uintptr_t> BSGraphics_RenderZPrePass_Ori{ REL::ID(901559) };
REL::Relocation<uintptr_t> BSGraphics_RenderAlphaTestZPrePass_Ori{ REL::ID(767228) };

REL::Relocation<uintptr_t> BSDistantObjectInstanceRenderer_Render_Ori{ REL::ID(148163) };
REL::Relocation<uintptr_t> BSShaderAccumulator_ResetSunOcclusion_Ori{ REL::ID(371166) };
REL::Relocation<uintptr_t> RenderTargetManager_DecompressDepthStencilTarget_Ori{ REL::ID(338650) };

REL::Relocation<uintptr_t> BSP_GetRenderPasses_Ori{ REL::ID(1289086) };
REL::Relocation<uintptr_t> BSBatchRenderer_Draw_Ori{ REL::ID(1152191) };

REL::Relocation<uintptr_t> DrawTriShape_Ori{ REL::ID(763320) };
REL::Relocation<uintptr_t> DrawIndexed_Ori{ REL::ID(763320) , 0x137 };

REL::Relocation<uintptr_t> MapDynamicTriShapeDynamicData_Ori{ REL::ID(732935) };
REL::Relocation<uintptr_t> BSStreamLoad_Ori{ REL::ID(160035) };

REL::Relocation<uintptr_t> MainPreRender_Ori{ REL::ID(378257) };




#pragma endregion

#pragma region Pointer
REL::Relocation<uintptr_t**> ptr_DrawWorldShadowNode{ REL::ID(1327069) };
REL::Relocation<NiAVObject**> ptr_DrawWorld1stPerson{ REL::ID(1491228) };
REL::Relocation<BSShaderManagerState**> ptr_BSShaderManager_State{ REL::ID(1327069) };
REL::Relocation<bool*> ptr_DrawWorld_b1stPersonEnable{ REL::ID(922366) };
REL::Relocation<bool*> ptr_DrawWorld_b1stPersonInWorld{ REL::ID(34473) };
REL::Relocation<BSShaderAccumulator**> ptr_Draw1stPersonAccum{ REL::ID(1430301) };
REL::Relocation<BSShaderAccumulator**> ptr_DrawWorldAccum{ REL::ID(1211381) };
REL::Relocation<BSCullingGroup**> ptr_k1stPersonCullingGroup{ REL::ID(731482) };
REL::Relocation<NiCamera**> ptr_BSShaderManagerSpCamera{ REL::ID(543218) };
REL::Relocation<NiCamera**> ptr_DrawWorldCamera{ REL::ID(1444212) };
REL::Relocation<NiCamera**> ptr_DrawWorldVisCamera{ REL::ID(81406) };
REL::Relocation<NiCamera**> ptr_DrawWorld1stCamera{ REL::ID(380177) };
REL::Relocation<NiCamera**> ptr_DrawWorldSpCamera{ REL::ID(543218) };
static REL::Relocation<Context**> ptr_DefaultContext{ REL::ID(33539) };
REL::Relocation<uint32_t*> ptr_tls_index{ REL::ID(842564) };

REL::Relocation<ZPrePassDrawData**> ptr_pFPZPrePassDrawDataA{ REL::ID(548629) };
REL::Relocation<ZPrePassDrawData**> ptr_pZPrePassDrawDataA{ REL::ID(1503321) };

REL::Relocation<AlphaTestZPrePassDrawData**> ptr_pFPAlphaTestZPrePassDrawDataA{ REL::ID(919131) };
REL::Relocation<AlphaTestZPrePassDrawData**> ptr_pAlphaTestZPrePassDrawDataA{ REL::ID(297801) };

static REL::Relocation<uint32_t*> FPZPrePassDrawDataCount{ REL::ID(163482) };
static REL::Relocation<uint32_t*> ZPrePassDrawDataCount{ REL::ID(844802) };
static REL::Relocation<uint32_t*> MergeInstancedZPrePassDrawDataCount{ REL::ID(1283533) };

static REL::Relocation<uint32_t*> FPAlphaTestZPrePassDrawDataCount{ REL::ID(382658) };
static REL::Relocation<uint32_t*> AlphaTestZPrePassDrawDataCount{ REL::ID(1064092) };
static REL::Relocation<uint32_t*> AlphaTestMergeInstancedZPrePassDrawDataCount{ REL::ID(602241) };

static REL::Relocation<NiCullingProcess**> DrawWorldGeomListCullProc0{ REL::ID(865470) };
static REL::Relocation<NiCullingProcess**> DrawWorldGeomListCullProc1{ REL::ID(1084947) };

static REL::Relocation<BSCullingProcess**> DrawWorldCullingProcess{ REL::ID(520184) };
static REL::Relocation<BSShaderManagerState**> BSM_ST{ REL::ID(1327069) };


#pragma endregion

typedef void (*DoZPrePassOriginalFuncType)(uint64_t, NiCamera*, NiCamera*, float, float, float, float);
typedef void (*RenderZPrePassOriginalFuncType)(RendererShadowState*, ZPrePassDrawData*, unsigned __int64*, unsigned __int16*, unsigned __int16*);
typedef void (*RenderAlphaTestZPrePassOriginalFuncType)(RendererShadowState*, AlphaTestZPrePassDrawData*, unsigned __int64*, unsigned __int16*, unsigned __int16*, ID3D11SamplerState**);
typedef void (*ResetSunOcclusionOriginalFuncType)(BSShaderAccumulator*);
typedef void (*BSDistantObjectInstanceRenderer_Render_OriginalFuncType)(uint64_t);
typedef void (*RenderTargetManager_DecompressDepthStencilTarget_OriginalFuncType)(RenderTargetManager*, int);
typedef void (*RTM_SetCurrentRenderTarget_OriginalFuncType)(RenderTargetManager*, int, int, SetRenderTargetMode);
typedef void (*RTM_SetCurrentDepthStencilTarget_OriginalFuncType)(RenderTargetManager*, int, SetRenderTargetMode, int);
typedef void (*FnSetCurrentCubeMapRenderTarget)(RenderTargetManager*, int, SetRenderTargetMode, int);
typedef void (*FnSetDirtyRenderTargets)(void*);
typedef void (*FnBSShaderRenderTargetsCreate)(void*);
typedef void (*FnBGSetRenderTarget)(RendererShadowState* arShadowState, unsigned int auiIndex, int aiTarget, SetRenderTargetMode aeMode);
typedef void (*BSEffectShaderProperty_GetRenderPasses_Original)(BSEffectShaderProperty* thisPtr, BSGeometry* geom, uint32_t renderMode, BSShaderAccumulator* accumulator);
typedef void (*FnRender_PreUI)(uint64_t ptr_drawWorld);
typedef void (*FnBegin)(uint64_t ptr_drawWorld);
typedef void (*FnMain_DrawWorldAndUI)(uint64_t, bool);
typedef void (*FnMain_Swap)();
typedef void (*FnBSCullingGroup_Process)(BSCullingGroup*, bool);
typedef void (*Fn)(uint64_t);
typedef void (*FnhkAdd1stPersonGeomToCuller)(uint64_t);
typedef void (*hkRTManager_CreateaRenderTarget)(RenderTargetManager rtm, int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent);
typedef void (*BSBatchRenderer_Draw_t)(BSRenderPass* apRenderPass);
typedef void (*MapDynamicTriShapeDynamicData_t)(Renderer*, BSDynamicTriShape*, DynamicTriShape*, DynamicTriShapeDrawData*, unsigned int);
typedef void (*BSStreamLoad)(BSStream* stream, const char* apFileName, NiBinaryStream* apStream);
typedef void (*PCUpdateMainThread)(PlayerCharacter*);
typedef void (*FnDrawTriShape)(BSGraphics::Renderer* thisPtr, BSGraphics::TriShape* apTriShape, unsigned int auiStartIndex, unsigned int auiNumTriangles);
typedef void(__fastcall* FnDrawIndexed)(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__fastcall* FnMainPreRender)(Main* thisptr, int auiDestination);
typedef void (*FnTAA)(ImageSpaceEffectTemporalAA*, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param);


	// 存储原始函数的指针
DoZPrePassOriginalFuncType g_pDoZPrePassOriginal = nullptr;
RenderZPrePassOriginalFuncType g_RenderZPrePassOriginal = nullptr;
RenderAlphaTestZPrePassOriginalFuncType g_RenderAlphaTestZPrePassOriginal = nullptr;
ResetSunOcclusionOriginalFuncType g_ResetSunOcclusionOriginal = nullptr;
BSDistantObjectInstanceRenderer_Render_OriginalFuncType g_BSDistantObjectInstanceRenderer_RenderOriginal = nullptr;
RenderTargetManager_DecompressDepthStencilTarget_OriginalFuncType g_DecompressDepthStencilTargetOriginal = nullptr;
RTM_SetCurrentRenderTarget_OriginalFuncType g_SetCurrentRenderTargetOriginal = nullptr;
RTM_SetCurrentDepthStencilTarget_OriginalFuncType g_SetCurrentDepthStencilTargetOriginal = nullptr;
FnSetCurrentCubeMapRenderTarget g_SetCurrentCubeMapRenderTargetOriginal = nullptr;
FnSetDirtyRenderTargets g_SetDirtyRenderTargetsOriginal = nullptr;
FnBSShaderRenderTargetsCreate g_BSShaderRenderTargetsCreateOriginal = nullptr;
FnBGSetRenderTarget g_BGSetRenderTargetOriginal = nullptr;
BSEffectShaderProperty_GetRenderPasses_Original g_BSEffectShaderGetRenderPassesOriginal = nullptr;
FnRender_PreUI g_RenderPreUIOriginal = nullptr;
FnBegin g_BeginOriginal = nullptr;
FnMain_DrawWorldAndUI g_DrawWorldAndUIOriginal = nullptr;
FnBSCullingGroup_Process g_BSCullingGroupProcessOriginal = nullptr;
Fn g_MainAccumOriginal = nullptr;
Fn g_OcclusionMapRenderOriginal = nullptr;
Fn g_MainRenderSetupOriginal = nullptr;
Fn g_OpaqueWireframeOriginal = nullptr;
Fn g_DeferredPrePassOriginal = nullptr;
Fn g_DeferredLightsImplOriginal = nullptr;
Fn g_DeferredCompositeOriginal = nullptr;
Fn g_ForwardOriginal = nullptr;
Fn g_RefractionOriginal = nullptr;
FnhkAdd1stPersonGeomToCuller g_Add1stPersonGeomToCullerOriginal = nullptr;
hkRTManager_CreateaRenderTarget g_RTManagerCreateRenderTargetOriginal = nullptr;
BSBatchRenderer_Draw_t g_originalBSBatchRendererDraw = nullptr;
MapDynamicTriShapeDynamicData_t g_MapDynamicTriShapeDynamicData = nullptr;
BSStreamLoad g_BSStreamLoad = nullptr;
PCUpdateMainThread g_PCUpdateMainThread = nullptr;
FnDrawTriShape g_DrawTriShape = nullptr;
FnDrawIndexed g_DrawIndexed = nullptr;
FnMainPreRender g_MainPreRender = nullptr;
FnTAA g_TAA = nullptr;

bool isFirstCopy = false;
bool isRenderReady = false;
bool isScopCamReady = false;
bool isImguiManagerInit = false;
bool isFirstSpawnNode = false;
bool isEnableTAA = false;
ThroughScope::D3DHooks* d3dHooks;
NIFLoader* nifloader;
static std::chrono::steady_clock::time_point delayStartTime;

PlayerCharacter* g_pchar = nullptr;

static RendererShadowState* GetRendererShadowState()
{
	_TEB* teb = NtCurrentTeb();
	Context* context;

	auto tls_index = *ptr_tls_index;
	context = *(Context**)(*((uint64_t*)teb->Reserved1[11] + tls_index) + 2848i64);

	if (!context) {
		auto defaultContext = *ptr_DefaultContext;
		context = defaultContext;
	}

	return &context->shadowState;
}

using namespace ThroughScope;
using namespace ThroughScope::Utilities;

//void hkBSBatchRenderer_Draw(BSRenderPass* apRenderPass)
//{
//	auto geometry = apRenderPass->pGeometry;
//	auto trishape = apRenderPass->pGeometry->IsTriShape();
//	auto vertxInfo = (D3D11_BUFFER_DESC*)apRenderPass->pGeometry->vertexDesc.desc;
//	if (trishape)
//	{
//		if (trishape->numTriangles == 32 && trishape->numVertices == 33)
//		{
//			D3DHooks::CacheAllStates();
//			logger::info("FOUND");
//			return;
//		}
//	}
//	g_originalBSBatchRendererDraw(apRenderPass);
//}

NiFrustum originalCamera1stviewFrustum{};
uint64_t savedDrawWorld = 0;

//renderTargets[0] = SwapChainImage RenderTarget(Only rtView and srView)
//renderTargets[4] = Main Render_PreUI RenderTarget
//renderTargets[26] = TAA 历史缓冲 = TAA PS t1
//renderTargets[29] = TAA Motion Vectors = TAA PS t2
//renderTargets[24] = TAA Jitter Mask = TAA PS t4 就是那个红不拉几的
//renderTargets[15] = 用于调整颜色的 1920 -> 480 的模糊的图像
//renderTargets[69] = 1x1 的小像素




void __fastcall hkHookTAA(ImageSpaceEffectTemporalAA* thisPtr, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param)
{
	// 在执行TAA之前捕获LUT纹理
	ID3D11DeviceContext* context = d3dHooks->GetContext();
	if (context && D3DHooks::IsEnableRender()) {
		// 捕获当前绑定的LUT纹理 (t3, t4, t5, t6)
		D3DHooks::CaptureLUTTextures(context);
	}

	// 1. 先执行原本的TAA
	g_TAA(thisPtr, a_geometry, a_param);
	

	if (!isScopCamReady || !isRenderReady || !D3DHooks::IsEnableRender())
		return;

	auto playerCamera = *ptr_DrawWorldCamera;
	auto scopeCamera = ScopeCamera::GetScopeCamera();

	if (!scopeCamera || !scopeCamera->parent)
		return;

	auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	if (!rendererData || !rendererData->context) {
		logger::error("Renderer data or context is null");
		return;
	}

	auto RTVs = rendererData->renderTargets;
	ID3D11RenderTargetView* mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
	ID3D11DepthStencilView* mainDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
	ID3D11Texture2D* mainRTTexture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
	ID3D11Texture2D* mainDSTexture = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;

	//ID3D11DeviceContext* context = (ID3D11DeviceContext*)rendererData->context;
	ID3D11Device* device = d3dHooks->GetDevice();
	ID3D11Texture2D* rtTexture2D = nullptr;
	ID3D11RenderTargetView* savedRTVs[2] = { nullptr };
	context->OMGetRenderTargets(2, savedRTVs, nullptr);
	
	ID3D11Resource* rtResource = nullptr;
	savedRTVs[1]->GetResource(&rtResource);
	if (rtResource != nullptr) {
		rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&rtTexture2D);
		rtResource->Release();  // 释放原始资源引用
	}

	D3D11_TEXTURE2D_DESC originalDesc;
	D3D11_TEXTURE2D_DESC rtTextureDesc;
	rtTexture2D->GetDesc(&originalDesc);  // 获取原纹理参数
	rtTextureDesc = originalDesc;
	rtTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	// 1. 创建临时的BackBuffer纹理和SRV用于此次渲染
	ID3D11Texture2D* tempBackBufferTex = nullptr;
	ID3D11ShaderResourceView* tempBackBufferSRV = nullptr;

	HRESULT hr = device->CreateTexture2D(&rtTextureDesc, nullptr, &tempBackBufferTex);
	if (SUCCEEDED(hr)) {
		hr = device->CreateShaderResourceView(tempBackBufferTex, NULL, &tempBackBufferSRV);
		if (FAILED(hr)) {
			logger::error("Failed to create temporary BackBuffer SRV: 0x{:X}", hr);
			SAFE_RELEASE(tempBackBufferTex);
			return;
		}
	} else {
		logger::error("Failed to create temporary BackBuffer texture: 0x{:X}", hr);
		return;
	}
	
	// 复制当前渲染目标内容到临时BackBuffer
	context->CopyResource(tempBackBufferTex, rtTexture2D);

	if (mainRTTexture) {
		// Copy the render target to our texture
		context->CopyResource(RenderUtilities::GetFirstPassColorTexture(), mainRTTexture);
		context->CopyResource(RenderUtilities::GetFirstPassDepthTexture(), mainDSTexture);
		RenderUtilities::SetFirstPassComplete(true);
	} else {
		logger::error("Failed to find a valid render target texture");
	}

	//更新瞄具镜头
	NiCloningProcess tempP{};
	NiCamera* originalCamera = (NiCamera*)((*ptr_DrawWorldCamera)->CreateClone(tempP));

	auto originalCamera1st = *ptr_DrawWorldCamera;
	//originalCamera1stviewFrustum = originalCamera1st->viewFrustum;
	originalCamera1stviewFrustum.bottom = originalCamera1st->viewFrustum.bottom;
	originalCamera1stviewFrustum.top = originalCamera1st->viewFrustum.top;
	originalCamera1stviewFrustum.left = originalCamera1st->viewFrustum.left;
	originalCamera1stviewFrustum.right = originalCamera1st->viewFrustum.right;

	auto originalCamera1stport = (*ptr_DrawWorld1stCamera)->port;
	scopeCamera->local.translate = originalCamera->local.translate;
	scopeCamera->local.rotate = originalCamera->local.rotate;
	scopeCamera->local.translate.y += 15;

	auto drawWorldCullingProcess = *DrawWorldCullingProcess;

	

	
	//清理主输出，准备第二次渲染
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	for (size_t i = 0; i < sizeof(RTVs); i++)
	{
		context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
	}

	//for (size_t i = 0; i < 100; i++) {
	//	context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
	//}

	context->ClearDepthStencilView(mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);


	D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");
	DrawWorld::SetCamera(scopeCamera);
	DrawWorld::SetUpdateCameraFOV(true);
	DrawWorld::SetAdjusted1stPersonFOV(ScopeCamera::GetTargetFOV());
	DrawWorld::SetCameraFov(ScopeCamera::GetTargetFOV());


	float scopeViewSize = 0.275f;
	NiFrustum scopeFrustum{};
	scopeFrustum.left = -scopeViewSize;
	scopeFrustum.right = scopeViewSize;
	scopeFrustum.top = scopeViewSize;
	scopeFrustum.bottom = -scopeViewSize;
	scopeFrustum.farPlane = 20480;
	scopeFrustum.nearPlane = 10;


	NiUpdateData nData;
	//originalCamera1st->viewFrustum = scopeFrustum;
	nData.camera = scopeCamera;
	scopeCamera->Update(nData);

	nData.camera = originalCamera1st;
	originalCamera1st->Update(nData);

	ScopeCamera::SetRenderingForScope(true);
	g_RenderPreUIOriginal(savedDrawWorld);  //第二次渲染输出到临时RenderTarget
	ScopeCamera::SetRenderingForScope(false);
	D3DPERF_EndEvent();



	context->CopyResource(RenderUtilities::GetSecondPassColorTexture(), mainRTTexture);
	context->CopyResource(rtTexture2D, tempBackBufferTex);
	RenderUtilities::SetSecondPassComplete(true);

	// 现在更新scope模型的纹理
	if (RenderUtilities::IsSecondPassComplete()) {
		// Restore the original render target content for normal display
		context->CopyResource(mainRTTexture, RenderUtilities::GetFirstPassColorTexture());
		context->CopyResource(mainDSTexture, RenderUtilities::GetFirstPassDepthTexture());
		}


	/*visCamera->viewFrustum = originalCamera1stviewFrustum;
	nData.camera = visCamera;
	visCamera->Update(nData);*/

	context->OMSetRenderTargets(1, &savedRTVs[1], nullptr);

	int scopeNodeIndexCount = ScopeCamera::GetScopeNodeIndexCount();
	if (scopeNodeIndexCount != -1) {
		try {
			RenderUtilities::SetRender_PreUIComplete(true);
			d3dHooks->SetScopeTexture(context);
			D3DHooks::isSelfDrawCall = true;
			context->DrawIndexed(scopeNodeIndexCount, 0, 0);
			D3DHooks::isSelfDrawCall = false;
			RenderUtilities::SetRender_PreUIComplete(false);
		} catch (...) {
			logger::error("Exception during scope content rendering");
			RenderUtilities::SetRender_PreUIComplete(false);
		}
	}

	DrawWorld::SetCamera(originalCamera1st);
	DrawWorld::SetUpdateCameraFOV(true);

	//originalCamera1st->viewFrustum = originalCamera1stviewFrustum;
	nData.camera = originalCamera1st;
	originalCamera1st->Update(nData);

	// === 资源清理部分 ===
	// 清理临时创建的D3D资源
	SAFE_RELEASE(tempBackBufferTex);
	SAFE_RELEASE(tempBackBufferSRV);
	SAFE_RELEASE(rtTexture2D);
	SAFE_RELEASE(savedRTVs[0]);
	SAFE_RELEASE(savedRTVs[1]);
	
	// 清理Camera克隆
	if (originalCamera) {
		if (originalCamera->DecRefCount() == 0) {
			originalCamera->DeleteThis();
		}
	}
}

void __fastcall hkMainPreRender(Main* thisPtr, int auiDestination)
{
	g_MainPreRender(thisPtr, auiDestination);
}

void __fastcall hkBegin(uint64_t ptr_drawWorld)
{
	g_BeginOriginal(ptr_drawWorld);

	if (D3DHooks::IsEnableRender())
	{
		auto drawWorldCullingProcess = *DrawWorldCullingProcess;
		float scopeViewSize = 0.275f;
		NiFrustum scopeFrustum{};
		scopeFrustum.left = -scopeViewSize;
		scopeFrustum.right = scopeViewSize;
		scopeFrustum.top = scopeViewSize;
		scopeFrustum.bottom = -scopeViewSize;
		drawWorldCullingProcess->m_kFrustum = scopeFrustum;
	}
	
}

void hkDrawTriShape(BSGraphics::Renderer* thisPtr, BSGraphics::TriShape* apTriShape, unsigned int auiStartIndex, unsigned int auiNumTriangles)
{
	auto trishape = reinterpret_cast<BSTriShape*>(apTriShape);

	if (trishape->numTriangles == 32 && trishape->numVertices == 33)
		return;

	g_DrawTriShape(thisPtr, apTriShape, auiStartIndex, auiNumTriangles);
}

void hkMapDynamicTriShapeDynamicData(Renderer* renderer, BSDynamicTriShape* bsDynamicTriShape, DynamicTriShape* dynamicTriShape, DynamicTriShapeDrawData* drawdata, unsigned int auiSize)
{
	g_MapDynamicTriShapeDynamicData(renderer, bsDynamicTriShape, dynamicTriShape, drawdata, auiSize);
}

void hkBSStreamLoad(BSStream* stream, const char* apFileName, NiBinaryStream* apStream)
{
	logger::info("apFileName: {}", apFileName);
	g_BSStreamLoad(stream, apFileName, apStream);
}



// ------ Main Render Hooks ------
void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld)
{
	//先正常渲染主场景
	savedDrawWorld = ptr_drawWorld;
	D3DEventNode(g_RenderPreUIOriginal(ptr_drawWorld), L"First Render_PreUI");
	

	//if (!isScopCamReady || !isRenderReady || !D3DHooks::IsEnableRender())
	//	return;

	//auto playerCamera = *ptr_DrawWorldCamera;
	//auto scopeCamera = ScopeCamera::GetScopeCamera();

	//if (!scopeCamera || !scopeCamera->parent)
	//	return;

	//auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
	//if (!rendererData || !rendererData->context) {
	//	logger::error("Renderer data or context is null");
	//	return;
	//}

	////ID3D11DeviceContext* context = (ID3D11DeviceContext*)rendererData->context;
	//ID3D11DeviceContext* context = d3dHooks->GetContext();

	//auto RTVs = rendererData->renderTargets;
	//ID3D11RenderTargetView* mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
	//ID3D11DepthStencilView* mainDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
	//ID3D11Texture2D* mainRTTexture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
	//ID3D11Texture2D* mainDSTexture = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;

	//if (mainRTTexture) {
	//	// Copy the render target to our texture
	//	context->CopyResource(RenderUtilities::GetFirstPassColorTexture(), mainRTTexture);
	//	context->CopyResource(RenderUtilities::GetFirstPassDepthTexture(), mainDSTexture);
	//	RenderUtilities::SetFirstPassComplete(true);
	//} else {
	//	logger::error("Failed to find a valid render target texture");
	//}

	////更新瞄具镜头
	//NiCloningProcess tempP{};
	//NiCamera* originalCamera = (NiCamera*)((*ptr_DrawWorldCamera)->CreateClone(tempP));

	//auto originalCamera1st = *ptr_DrawWorldCamera;
	//originalCamera1stviewFrustum = originalCamera1st->viewFrustum;
	//auto originalCamera1stport = (*ptr_DrawWorld1stCamera)->port;
	//scopeCamera->local.translate = originalCamera->local.translate;
	//scopeCamera->local.rotate = originalCamera->local.rotate;
	//scopeCamera->local.translate.y += 15;


	//auto visCamera = *ptr_DrawWorldVisCamera;
	//float scopeViewSize = 0.275f;
	//NiFrustum scopeFrustum{};
	//scopeFrustum.left = -scopeViewSize;
	//scopeFrustum.right = scopeViewSize;
	//scopeFrustum.top = scopeViewSize;
	//scopeFrustum.bottom = -scopeViewSize;

	//auto drawWorldCullingProcess = *DrawWorldCullingProcess;
	//drawWorldCullingProcess->m_kFrustum = scopeFrustum;

	//NiUpdateData nData;

	//visCamera->viewFrustum = scopeFrustum;

	//nData.camera = visCamera;
	//visCamera->Update(nData);

	//nData.camera = scopeCamera;
	//scopeCamera->Update(nData);

	//nData.camera = originalCamera1st;
	//originalCamera1st->Update(nData);

	//
	////清理主输出，准备第二次渲染
	//float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	//for (size_t i = 0; i < sizeof(RTVs); i++)
	//{
	//	context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
	//}

	//context->ClearDepthStencilView(mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	//D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");
	//DrawWorld::SetCamera(scopeCamera);
	//DrawWorld::SetUpdateCameraFOV(true);
	//DrawWorld::SetAdjusted1stPersonFOV(ScopeCamera::GetTargetFOV());
	//DrawWorld::SetCameraFov(ScopeCamera::GetTargetFOV());

	//// 更新裁剪处理器的视锥体（使用已优化的视锥体）
	////(*DrawWorldGeomListCullProc0)->SetFrustum(&visCamera->viewFrustum);
	////(*DrawWorldGeomListCullProc1)->SetFrustum(&visCamera->viewFrustum);
	////(*DrawWorldGeomListCullProc0)->bCustomCullPlanes = true;
	////(*DrawWorldGeomListCullProc1)->bCustomCullPlanes = true;

	//nData.camera = scopeCamera;
	//scopeCamera->Update(nData);

	////Main::GetSingleton()->DrawWorld_PreRender(0);
	////DrawWorld::DoUmbraQuery(ptr_drawWorld);

	//ScopeCamera::SetRenderingForScope(true);
	//(*BSM_ST)->ForceDisableSSR = true;
	//g_RenderPreUIOriginal(ptr_drawWorld);  //第二次渲染
	//(*BSM_ST)->ForceDisableSSR = false;
	//ScopeCamera::SetRenderingForScope(false);
	//D3DPERF_EndEvent();

	//context->CopyResource(RenderUtilities::GetSecondPassColorTexture(), mainRTTexture);
	//RenderUtilities::SetSecondPassComplete(true);
	//ID3D11ShaderResourceView* scopeSRV = nullptr;


	//visCamera->viewFrustum = originalCamera1stviewFrustum;
	//nData.camera = visCamera;
	//visCamera->Update(nData);

	///*originalCamera1st->viewFrustum = originalCamera1stviewFrustum;
	//nData.camera = originalCamera1st;
	//originalCamera1st->Update(nData);*/

	//DrawWorld::SetCamera(originalCamera);
	//DrawWorld::SetUpdateCameraFOV(true);

	//int scopeNodeIndexCount = ScopeCamera::GetScopeNodeIndexCount();
	//if (scopeNodeIndexCount != -1) {
	//	try {
	//		RenderUtilities::SetRender_PreUIComplete(true);
	//		d3dHooks->RestoreAllCachedStates();
	//		d3dHooks->SetScopeTexture(context);
	//		D3DHooks::isSelfDrawCall = true;
	//		context->DrawIndexed(scopeNodeIndexCount, 0, 0);
	//		D3DHooks::isSelfDrawCall = false;
	//		RenderUtilities::SetRender_PreUIComplete(false);
	//	} catch (...) {
	//		logger::error("Exception during scope quad rendering");
	//		RenderUtilities::SetRender_PreUIComplete(false);
	//	}
	//}

	//DrawWorld::Render_UI(ptr_drawWorld);

	//if (originalCamera) {
	//	if (originalCamera->DecRefCount() == 0) {
	//		originalCamera->DeleteThis();
	//	}
	//}
}


void __fastcall hkRenderZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::ZPrePassDrawData* aZPreData,
	unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode)
{
	g_RenderZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode);
}

void __fastcall hkRenderAlphaTestZPrePass(BSGraphics::RendererShadowState* rshadowState,
	BSGraphics::AlphaTestZPrePassDrawData* aZPreData,
	unsigned __int64* aVertexDesc,
	unsigned __int16* aCullmode,
	unsigned __int16* aDepthBiasMode,
	ID3D11SamplerState** aCurSamplerState)
{
	g_RenderAlphaTestZPrePassOriginal(rshadowState, aZPreData, aVertexDesc, aCullmode, aDepthBiasMode, aCurSamplerState);
}

void __fastcall hkRenderer_DoZPrePass(uint64_t thisPtr, NiCamera* apFirstPersonCamera, NiCamera* apWorldCamera, float afFPNear, float afFPFar, float afNear, float afFar)
{
	if (ScopeCamera::IsRenderingForScope()) {
		*FPZPrePassDrawDataCount = 0;
		*FPAlphaTestZPrePassDrawDataCount = 0;
	}
	D3DEventNode(g_pDoZPrePassOriginal(thisPtr, apFirstPersonCamera, apWorldCamera, afFPNear, afFPFar, afNear, afFar), L"hkRenderer_DoZPrePass");
}
void __fastcall hkBSDistantObjectInstanceRenderer_Render(uint64_t thisPtr)
{
	D3DEventNode(g_BSDistantObjectInstanceRenderer_RenderOriginal(thisPtr), L"hkBSDistantObjectInstanceRenderer_Render");
}
void __fastcall hkBSShaderAccumulator_ResetSunOcclusion(BSShaderAccumulator* thisPtr)
{
	D3DEventNode(g_ResetSunOcclusionOriginal(thisPtr), L"hkBSShaderAccumulator_ResetSunOcclusion");
}
void __fastcall hkDecompressDepthStencilTarget(RenderTargetManager* thisPtr, int index)
{
	D3DEventNode(g_DecompressDepthStencilTargetOriginal(thisPtr, index), L"hkBSShaderAccumulator_ResetSunOcclusion");
}
void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr)
{
	if (ScopeCamera::IsRenderingForScope())
		return;
	g_Add1stPersonGeomToCullerOriginal(thisPtr);
}
void __fastcall hkBSShaderAccumulator_RenderBatches(
	BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup)
{
	typedef void (*FnRenderBatches)(BSShaderAccumulator*, int, bool, int);
	FnRenderBatches fn = (FnRenderBatches)BSShaderAccumulator_RenderBatches_Ori.address();
	(*fn)(thisPtr, aiShader, abAlphaPass, aeGroup);
}
void __fastcall hkBSShaderAccumulator_RenderBlendedDecals(BSShaderAccumulator* thisPtr)
{
	typedef void (*Fn)(BSShaderAccumulator*);
	Fn fn = (Fn)BSShaderAccumulator_RenderBlendedDecals_Ori.address();
	(*fn)(thisPtr);
}
void __fastcall hkBSShaderAccumulator_RenderOpaqueDecals(BSShaderAccumulator* thisPtr)
{
	typedef void (*Fn)(BSShaderAccumulator*);
	Fn fn = (Fn)BSShaderAccumulator_RenderOpaqueDecals_Ori.address();
	(*fn)(thisPtr);
}
void __fastcall hkBSCullingGroup_Process(BSCullingGroup* thisPtr, bool abFirstStageOnly)
{
	g_BSCullingGroupProcessOriginal(thisPtr, abFirstStageOnly);
}
RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties)
{
	typedef RenderTarget* (*hkRenderer_CreateRenderTarget)(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties);
	hkRenderer_CreateRenderTarget fn = (hkRenderer_CreateRenderTarget)Renderer_CreateRenderTarget_Ori.address();
	return (*fn)(renderer, aId, apName, aProperties);
}
void __fastcall hkRTManager_CreateRenderTarget(RenderTargetManager rtm, int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent)
{
	g_RTManagerCreateRenderTargetOriginal(rtm, aIndex, arProperties, aPersistent);
}

void __fastcall hkMainAccum(uint64_t ptr_drawWorld)
{
	D3DEventNode(g_MainAccumOriginal(ptr_drawWorld), L"hkMainAccum");
}

void __fastcall hkOcclusionMapRender()
{
	typedef void (*Fn)();
	Fn fn = (Fn)DrawWorld_OcclusionMapRender_Ori.address();
	D3DEventNode((*fn)(), L"hkOcclusionMapRender");
}

void __fastcall hkMainRenderSetup(uint64_t ptr_drawWorld)
{
	D3DEventNode(g_MainRenderSetupOriginal(ptr_drawWorld), L"hkMainRenderSetup");
}

void __fastcall hkOpaqueWireframe(uint64_t ptr_drawWorld)
{
	D3DEventNode(g_OpaqueWireframeOriginal(ptr_drawWorld), L"hkOpaqueWireframe");
}

void __fastcall hkDeferredPrePass(uint64_t ptr_drawWorld)
{
	D3DEventNode(g_DeferredPrePassOriginal(ptr_drawWorld), L"hkDeferredPrePass");
}

void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld)
{
	D3DEventNode(g_DeferredLightsImplOriginal(ptr_drawWorld), L"hkDeferredLightsImpl");
}

void __fastcall hkDeferredComposite(uint64_t ptr_drawWorld)
{
	D3DEventNode(g_DeferredCompositeOriginal(ptr_drawWorld), L"hkDeferredComposite");
}

void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld)
{
	D3DHooks::SetForwardStage(true);
	D3DEventNode(g_ForwardOriginal(ptr_drawWorld), L"hkDrawWorld_Forward");
	D3DHooks::SetForwardStage(false);
}

void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr)
{
	D3DEventNode(g_RefractionOriginal(this_ptr), L"hkDrawWorld_Refraction");
}



void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground)
{
	D3DEventNode(g_DrawWorldAndUIOriginal(ptr_drawWorld, abBackground), L"hkMain_DrawWorldAndUI");
}

namespace FirstSpawnDelay
{
	static bool delayStarted = false;
	static std::chrono::steady_clock::time_point delayStartTime;

	void Reset()
	{
		delayStarted = false;
		// delayStartTime 不需要重置，下次会重新赋值
	}
}

void ResetFirstSpawnState()
{
	ScopeCamera::isFirstSpawnNode = false;
	ScopeCamera::isDelayStarted = false;
	ScopeCamera::isFirstScopeRender = true;
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
		for (size_t i = 0; i < aRetArray.size();i++)
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

		//auto pAccum = (*ptr_DrawWorldAccum);
		//auto nameofSomething = pAccum->BatchRenderer.geometryGroups[0]->passList.head->pGeometry->name;
		//imadThermal->Apply(100, 100, g_pchar->Get3D());
		for (auto actor : actorList)
		{
			auto actorNode = actor->Get3D()->IsNode();
			if (!actorNode) continue;

			auto actorBase = actor->GetActorBase();
			if (!actorBase) continue;

			LogNodeHierarchy(actorNode);
			auto actorNodeChildren = actorNode->children.data();
			for (size_t j = 0; j < actorNode->children.size();j++) 
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
		return g_PCUpdateMainThread(pChar);
	}


	if (!ScopeCamera::isFirstSpawnNode) {
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
				ScopeCamera::isFirstSpawnNode = true;
				logger::warn("FirstSpawn Finish");
			}
		}
		return g_PCUpdateMainThread(pChar);
	}

	if (g_pchar->IsInThirdPerson())
		return g_PCUpdateMainThread(pChar);

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

	g_PCUpdateMainThread(pChar);
}

void RegisterHooks()
{
	logger::info("Registering hooks...");
	using namespace Utilities;

	CreateAndEnableHook((LPVOID)Renderer_DoZPrePass_Ori.address(), &hkRenderer_DoZPrePass, reinterpret_cast<LPVOID*>(&g_pDoZPrePassOriginal), "DoZPrePass");
	CreateAndEnableHook((LPVOID)BSGraphics_RenderZPrePass_Ori.address(), &hkRenderZPrePass, reinterpret_cast<LPVOID*>(&g_RenderZPrePassOriginal), "RenderZPrePass");
	CreateAndEnableHook((LPVOID)BSGraphics_RenderAlphaTestZPrePass_Ori.address(), &hkRenderAlphaTestZPrePass, reinterpret_cast<LPVOID*>(&g_RenderAlphaTestZPrePassOriginal), "RenderAlphaTestZPrePass");

	CreateAndEnableHook((LPVOID)BSShaderAccumulator_ResetSunOcclusion_Ori.address(), &hkBSShaderAccumulator_ResetSunOcclusion,
		reinterpret_cast<LPVOID*>(&g_ResetSunOcclusionOriginal), "ResetSunOcclusion");

	CreateAndEnableHook((LPVOID)BSDistantObjectInstanceRenderer_Render_Ori.address(), &hkBSDistantObjectInstanceRenderer_Render,
		reinterpret_cast<LPVOID*>(&g_BSDistantObjectInstanceRenderer_RenderOriginal), "BSDistantObjectInstanceRenderer_Render");

	CreateAndEnableHook((LPVOID)RenderTargetManager_DecompressDepthStencilTarget_Ori.address(), &hkDecompressDepthStencilTarget,
		reinterpret_cast<LPVOID*>(&g_DecompressDepthStencilTargetOriginal), "DecompressDepthStencilTarget");

	CreateAndEnableHook((LPVOID)DrawWorld_Render_PreUI_Ori.address(), &hkRender_PreUI,
		reinterpret_cast<LPVOID*>(&g_RenderPreUIOriginal), "Render_PreUI");

	CreateAndEnableHook((LPVOID)DrawWorld_Begin_Ori.address(), &hkBegin,
		reinterpret_cast<LPVOID*>(&g_BeginOriginal), "Begin");

	CreateAndEnableHook((LPVOID)Main_DrawWorldAndUI_Ori.address(), &hkMain_DrawWorldAndUI,
		reinterpret_cast<LPVOID*>(&g_DrawWorldAndUIOriginal), "Main_DrawWorldAndUI");

	CreateAndEnableHook((LPVOID)BSCullingGroup_Process_Ori.address(), &hkBSCullingGroup_Process,
		reinterpret_cast<LPVOID*>(&g_BSCullingGroupProcessOriginal), "BSCullingGroup_Process");

	CreateAndEnableHook((LPVOID)DrawWorld_MainAccum_Ori.address(), &hkMainAccum,
		reinterpret_cast<LPVOID*>(&g_MainAccumOriginal), "MainAccum");

	CreateAndEnableHook((LPVOID)DrawWorld_MainRenderSetup_Ori.address(), &hkMainRenderSetup,
		reinterpret_cast<LPVOID*>(&g_MainRenderSetupOriginal), "MainRenderSetup");

	CreateAndEnableHook((LPVOID)DrawWorld_OpaqueWireframe_Ori.address(), &hkOpaqueWireframe,
		reinterpret_cast<LPVOID*>(&g_OpaqueWireframeOriginal), "OpaqueWireframe");

	CreateAndEnableHook((LPVOID)DrawWorld_DeferredPrePass_Ori.address(), &hkDeferredPrePass,
		reinterpret_cast<LPVOID*>(&g_DeferredPrePassOriginal), "DeferredPrePass");

	CreateAndEnableHook((LPVOID)DrawWorld_DeferredLightsImpl_Ori.address(), &hkDeferredLightsImpl,
		reinterpret_cast<LPVOID*>(&g_DeferredLightsImplOriginal), "DeferredLightsImpl");

	CreateAndEnableHook((LPVOID)DrawWorld_DeferredComposite_Ori.address(), &hkDeferredComposite,
		reinterpret_cast<LPVOID*>(&g_DeferredCompositeOriginal), "DeferredComposite");

	CreateAndEnableHook((LPVOID)DrawWorld_Forward_Ori.address(), &hkDrawWorld_Forward,
		reinterpret_cast<LPVOID*>(&g_ForwardOriginal), "DrawWorld_Forward");

	CreateAndEnableHook((LPVOID)DrawWorld_Refraction_Ori.address(), &hkDrawWorld_Refraction,
		reinterpret_cast<LPVOID*>(&g_RefractionOriginal), "DrawWorld_Refraction");

	CreateAndEnableHook((LPVOID)DrawWorld_Add1stPersonGeomToCuller_Ori.address(), &hkAdd1stPersonGeomToCuller,
		reinterpret_cast<LPVOID*>(&g_Add1stPersonGeomToCullerOriginal), "Add1stPersonGeomToCuller");

	CreateAndEnableHook((LPVOID)RTM_CreateRenderTarget_Ori.address(), &hkRTManager_CreateRenderTarget,
		reinterpret_cast<LPVOID*>(&g_RTManagerCreateRenderTargetOriginal), "RTManager_CreateRenderTarget");

	/*CreateAndEnableHook((LPVOID)BSBatchRenderer_Draw_Ori.address(), &hkBSBatchRenderer_Draw,
		reinterpret_cast<LPVOID*>(&g_originalBSBatchRendererDraw), "BSBatchRenderer_Draw");*/

	//CreateAndEnableHook((LPVOID)MapDynamicTriShapeDynamicData_Ori.address(), &hkMapDynamicTriShapeDynamicData,
	//	reinterpret_cast<LPVOID*>(&g_MapDynamicTriShapeDynamicData), "MapDynamicTriShapeDynamicData");


	/* CreateAndEnableHook((LPVOID)BSStreamLoad_Ori.address(), &hkBSStreamLoad,
		reinterpret_cast<LPVOID*>(&g_BSStreamLoad), "BSStreamLoad");*/
	 
	 
	 CreateAndEnableHook((LPVOID)PCUpdateMainThread_Ori.address(), &hkPCUpdateMainThread,
		reinterpret_cast<LPVOID*>(&g_PCUpdateMainThread), "PCUpdateMainThread");

	 CreateAndEnableHook((LPVOID)MainPreRender_Ori.address(), &hkMainPreRender,
		 reinterpret_cast<LPVOID*>(&g_MainPreRender), "MainPreRender");

	 /* CreateAndEnableHook((LPVOID)DrawTriShape_Ori.address(), &hkDrawTriShape,
		  reinterpret_cast<LPVOID*>(&g_DrawTriShape), "DrawTriShape");*/


	  //F4SE::Trampoline& trampoline = F4SE::GetTrampoline();
	  //g_DrawIndexed = (FnDrawIndexed)trampoline.write_branch<5>(DrawIndexed_Ori.address(), reinterpret_cast<uintptr_t>(hkDrawIndexed));
	  //

	  /*CreateAndEnableHook((LPVOID)DrawIndexed_Ori.address(), &hkDrawIndexed,
		  reinterpret_cast<LPVOID*>(&g_DrawIndexed), "DrawIndexed");*/

	 REL::Relocation<std::uintptr_t> vtable_TAA(RE::ImageSpaceEffectTemporalAA::VTABLE[0]);
	 const auto oldFuncTAA = vtable_TAA.write_vfunc(1, reinterpret_cast<std::uintptr_t>(&hkHookTAA));
	 g_TAA = decltype(&hkHookTAA)(oldFuncTAA);

	logger::info("Hooks registered successfully");
}




// Initialization thread function
DWORD WINAPI InitThread(HMODULE hModule) 
{
	while (!BSGraphics::RendererData::GetSingleton() 
		|| !BSGraphics::RendererData::GetSingleton()->renderWindow 
		|| !BSGraphics::RendererData::GetSingleton()->renderWindow->hwnd 
		|| !BSGraphics::RendererData::GetSingleton()->renderWindow->swapChain 
		|| !RE::BSGraphics::RendererData::GetSingleton()->device
		|| !RE::BSGraphics::RendererData::GetSingleton()->context) {
		Sleep(10);
	}

	
    // Wait for the game world to be fully loaded
	while (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Get3D() || !RE::PlayerControls::GetSingleton() || !RE::PlayerCamera::GetSingleton() || !RE::Main::WorldRootCamera()) 
    {
		Sleep(500);
	}
    logger::info("Game world loaded, initializing ThroughScope...");
    
    // Initialize systems
	isImguiManagerInit = ThroughScope::ImGuiManager::GetSingleton()->Initialize();

    isScopCamReady = ThroughScope::ScopeCamera::Initialize();
	isRenderReady = ThroughScope::RenderUtilities::Initialize();


    logger::info("ThroughScope initialization completed");
    return 0;
}

// Initialize the plugin
void InitializePlugin()
{
	g_pchar = RE::PlayerCharacter::GetSingleton();
	RegisterHooks();
	ThroughScope::EquipWatcher::GetSingleton()->Initialize();
	ThroughScope::AnimationGraphEventWatcher::GetSingleton()->Initialize();
	
    // Start initialization thread for components that need the game world
    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitThread, (HMODULE)REX::W32::GetCurrentModule(), 0, NULL);
}

// F4SE Query plugin
F4SE_EXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name = Version::PROJECT.data();
    a_info->version = Version::MAJOR;

    // Setup logging
#ifndef NDEBUG
    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
    auto path = logger::log_directory();
    if (!path) {
        return false;
    }

    *path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
    log->set_level(spdlog::level::trace);
#else
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::warn);
#endif

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

    logger::info(FMT_STRING("{} v{}.{}.{}"), Version::PROJECT, Version::MAJOR, Version::MINOR, Version::PATCH);

    if (a_f4se->IsEditor()) {
        logger::critical("loaded in editor");
        return false;
    }

    const auto ver = a_f4se->RuntimeVersion();
    if ((REL::Module::IsF4() && ver < F4SE::RUNTIME_1_10_163) ||
        (REL::Module::IsVR() && ver < F4SE::RUNTIME_LATEST_VR)) {
        logger::critical("unsupported runtime v{}", ver.string());
        return false;
    }

    logger::info("TrueThroughScope Query successful!");
    
    F4SE::AllocTrampoline(32 * 8);
    return true;
}

// F4SE Load plugin
F4SE_EXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

#ifdef _DEBUG
    while (!IsDebuggerPresent()) {
        Sleep(1000);
    }
    Sleep(1000);
	

#endif

	auto mhInit = MH_Initialize();
	// 初始化MinHook
	if (mhInit != MH_OK) {
		logger::info("MH_Initialize Not Ok, Reason: {}", (int)mhInit);
	}

	d3dHooks = D3DHooks::GetSington();
	nifloader = NIFLoader::GetSington();
	
	logger::info("Ninja!");
	d3dHooks->PreInit();

    F4SE::Init(a_f4se);
    
    // Register plugin for F4SE messages
    const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
    if (!message) {
        logger::critical("Failed to get messaging interface");
        return false;
    }
    
    // Register for F4SE messages
    message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
            // Game data is ready - this is when we should initialize
            logger::info("Game data ready, initializing plugin");
			d3dHooks->Initialize();
            InitializePlugin();
		} else if (msg->type == F4SE::MessagingInterface::kPostLoadGame) {

			logger::info("Load a save, reset scope status");
			ResetFirstSpawnState();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame)
		{
			ResetFirstSpawnState();
		}
		else if (msg->type == F4SE::MessagingInterface::kPreLoadGame ||
				 msg->type == F4SE::MessagingInterface::kDeleteGame)
		{
			// 游戏关闭前或删除存档时清理资源
			logger::info("Cleaning up resources before game shutdown");
			ThroughScope::D3DHooks::CleanupStaticResources();
			ThroughScope::RenderUtilities::CleanupBackBufferResources();
		}
    });

    return true;
}

// Version data for F4SE
F4SE_EXPORT constinit auto F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData data{};

    data.AuthorName(Version::AUTHOR);
    data.PluginName(Version::PROJECT);
    data.PluginVersion(Version::VERSION);

    data.UsesAddressLibrary(true);
    data.IsLayoutDependent(true);
    data.UsesSigScanning(false);
    data.HasNoStructUse(false);

    data.CompatibleVersions({ F4SE::RUNTIME_1_10_163 });
    return data;
}();
