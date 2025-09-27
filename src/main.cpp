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
#include <chrono>
#include <Windows.h>
#include <winternl.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>

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

REL::Relocation<uintptr_t> DrawWorld_LightUpdate_Ori{ REL::ID(918638) };  // DrawWorld::LightUpdate
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
REL::Relocation<uintptr_t> BSCullingGroupAdd_Ori{ REL::ID(1175493) };
REL::Relocation<uintptr_t> BSCullingGroup_SetCompoundFrustum_Ori{ REL::ID(158202) };

REL::Relocation<uintptr_t> DrawWorld_Move1stPersonToOrigin_Ori{ REL::ID(76526) };



#pragma endregion

#pragma region Pointer
REL::Relocation<ShadowSceneNode**> ptr_DrawWorldShadowNode{ REL::ID(1327069) };  // DrawWorld::pShadowSceneNode
REL::Relocation<NiAVObject**> ptr_DrawWorld1stPerson{ REL::ID(1491228) };
REL::Relocation<bool*> ptr_DrawWorld_b1stPersonEnable{ REL::ID(922366) };
REL::Relocation<bool*> ptr_DrawWorld_b1stPersonInWorld{ REL::ID(34473) };
REL::Relocation<BSShaderAccumulator**> ptr_Draw1stPersonAccum{ REL::ID(1430301) };
REL::Relocation<BSShaderAccumulator**> ptr_DrawWorldAccum{ REL::ID(1211381) };
REL::Relocation<BSCullingGroup**> ptr_k1stPersonCullingGroup{ REL::ID(731482) };
REL::Relocation<NiCamera**> ptr_BSShaderManagerSpCamera{ REL::ID(543218) };
REL::Relocation<NiCamera**> ptr_DrawWorldCamera{ REL::ID(1444212) }; //pCam
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

static REL::Relocation<BSGeometryListCullingProcess**> DrawWorldGeomListCullProc0{ REL::ID(865470) };
static REL::Relocation<BSGeometryListCullingProcess**> DrawWorldGeomListCullProc1{ REL::ID(1084947) };

static REL::Relocation<BSCullingProcess**> DrawWorldCullingProcess{ REL::ID(520184) };
static REL::Relocation<BSShaderManagerState**> BSM_ST{ REL::ID(1327069) };

struct LightStateBackup
{
	NiPointer<BSLight> light;
	uint32_t frustumCull;
	bool occluded;
	bool temporary;
	bool dynamic;
	float lodDimmer;
	NiPointer<NiCamera> camera;
	BSCullingProcess* cullingProcess;
};
static std::vector<LightStateBackup> g_LightStateBackups;

#pragma endregion

typedef void (*FnDrawWorldLightUpdate)(uint64_t);  // DrawWorld::LightUpdate
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
typedef void (__fastcall *FnTAA)(ImageSpaceEffectTemporalAA*, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param);
typedef void (*FnBSCullingGroupAdd)(BSCullingGroup* ,NiAVObject* apObj,const NiBound* aBound,const unsigned int aFlags);
typedef void (*FnBSShaderAccumulator_RenderBatches)(BSShaderAccumulator*, int, bool, int);
typedef void (*FnBSCullingGroup_SetCompoundFrustum)(BSCullingGroup*, BSCompoundFrustum*);
typedef void (*FnDrawWorld_Move1stPersonToOrigin)(uint64_t);


	// 存储原始函数的指针
FnDrawWorldLightUpdate g_DrawWorldLightUpdateOriginal = nullptr;
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
FnMainPreRender g_MainPreRender = nullptr;
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
FnTAA g_TAA = nullptr;
FnBSCullingGroupAdd g_BSCullingGroupAdd = nullptr;
FnBSShaderAccumulator_RenderBatches g_BSShaderAccumulatorRenderBatches = nullptr;
FnBSCullingGroup_SetCompoundFrustum g_BSCullingGroup_SetCompoundFrustum = nullptr;
FnDrawWorld_Move1stPersonToOrigin g_DrawWorld_Move1stPersonToOrigin = nullptr;


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

NiFrustum originalCamera1stviewFrustum{};
NiFrustum originalFrustum{};
NiFrustum scopeFrustum{};
NiRect<float> scopeViewPort{};

extern NiCamera* ggg_ScopeCamera = nullptr;

// 用于frustum剔除的全局变量
static NiFrustum g_BackupFrustum{};
static bool g_FrustumBackedUp = false;

uint64_t savedDrawWorld = 0;
HMODULE upscalerModular;

NiCamera* g_worldFirstCam = *ptr_DrawWorld1stCamera;

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



void hkDrawWorld_Move1stPersonToOrigin(uint64_t thisPtr)
{
	g_DrawWorld_Move1stPersonToOrigin(thisPtr);
}

void hkBSBatchRenderer_Draw(BSRenderPass* apRenderPass)
{
	g_originalBSBatchRendererDraw(apRenderPass);
}

void hkBSCullingGroup_SetCompoundFrustum(BSCullingGroup* thisPtr, BSCompoundFrustum* apCompoundFrustum)
{
	g_BSCullingGroup_SetCompoundFrustum(thisPtr, apCompoundFrustum);
}

bool IsValidObject(NiAVObject* apObj)
{
	if (!apObj || apObj->refCount == 0)
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
	if (!IsValidObject(apObj)) {
		logger::error("Invalid object: 0x{:X}", (uintptr_t)apObj);
		return;
	}

	g_BSCullingGroupAdd(thisPtr, apObj, aBound, aFlags);
}

//renderTargets[0] = SwapChainImage RenderTarget(Only rtView and srView)
//renderTargets[4] = Main Render_PreUI RenderTarget
//renderTargets[26] = TAA 历史缓冲 = TAA PS t1
//renderTargets[29] = TAA Motion Vectors = TAA PS t2
//renderTargets[24] = TAA Jitter Mask = TAA PS t4 就是那个红不拉几的
//renderTargets[15] = 用于调整颜色的 1920 -> 480 的模糊的图像
//renderTargets[69] = 1x1 的小像素


// 添加一个标志来区分瞄具专用渲染
static bool g_IsScopeOnlyRender = false;

void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld);

void __fastcall hkTAA(ImageSpaceEffectTemporalAA* thisPtr, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param)
{

	// 检查原始函数指针是否有效
	if (!g_TAA) {
		logger::error("g_TAA is null! Cannot call original function");
		return;
	}

	// 在执行TAA之前捕获LUT纹理
	ID3D11DeviceContext* context = d3dHooks->GetContext();
	if (context && D3DHooks::IsEnableRender()) {
		// 捕获当前绑定的LUT纹理 (t3, t4, t5, t6)
		D3DHooks::CaptureLUTTextures(context);
	}

	// 1. 先执行原本的TAA
	g_TAA(thisPtr, a_geometry, a_param);

	//if (ScopeCamera::IsRenderingForScope())
	//	return;

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
	auto RTVMain = ((ID3D11RenderTargetView*)(RTVs[4].rtView));
	auto RTVSwap= ((ID3D11RenderTargetView*)(RTVs[0].rtView));
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

	auto originalCamera1stport = (*ptr_DrawWorld1stCamera)->port;
	scopeCamera->local.translate = originalCamera->local.translate;
	scopeCamera->local.rotate = originalCamera->local.rotate;
	scopeCamera->local.translate.y += 15;

	if (scopeCamera->parent) {
		NiUpdateData parentUpdate{};
		parentUpdate.camera = scopeCamera;
		scopeCamera->parent->Update(parentUpdate);
	}


	scopeCamera->world.translate = originalCamera->world.translate;
	scopeCamera->world.rotate = originalCamera->world.rotate;
	scopeCamera->world.scale = originalCamera->world.scale;
	scopeCamera->world.translate.y += 15;

	scopeCamera->UpdateWorldBound();

	//清理主输出，准备第二次渲染
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	for (size_t i = 0; i < 4; i++)
	{
		context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
	}

	context->ClearDepthStencilView(mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 清理延迟渲染的G-Buffer以确保光照信息不会残留
	// G-Buffer通常包括：法线、反照率、深度等
	// 扩展清理范围，包括所有可能的光照相关缓冲区
	static const int lightingBuffers[] = { 8, 9, 10, 11, 12, 13, 14, 15 }; // G-Buffer和光照累积缓冲区
	for (int bufIdx : lightingBuffers) {
		if (bufIdx < 100 && rendererData->renderTargets[bufIdx].rtView) {
			float clearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			// 对于法线缓冲区，使用默认法线值
			if (bufIdx == 8) {
				clearValue[0] = 0.5f;
				clearValue[1] = 0.5f;
				clearValue[2] = 1.0f;
				clearValue[3] = 1.0f;
			}
			context->ClearRenderTargetView((ID3D11RenderTargetView*)rendererData->renderTargets[bufIdx].rtView, clearValue);
		}
	}

	D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");
	DrawWorld::SetCamera(scopeCamera);
	DrawWorld::SetUpdateCameraFOV(true);
	DrawWorld::SetAdjusted1stPersonFOV(ScopeCamera::GetTargetFOV());
	DrawWorld::SetCameraFov(ScopeCamera::GetTargetFOV());

	// 同步BSShaderManager的相机指针，确保着色器使用正确的视图矩阵
	*ptr_BSShaderManagerSpCamera = scopeCamera;

	NiUpdateData nData;
	ScopeCamera::SetRenderingForScope(true);

	auto SpCam = *ptr_DrawWorldSpCamera; 
	auto fpsCam = *ptr_DrawWorld1stCamera;
	auto DrawWorldCamera = *ptr_DrawWorldCamera;	
	auto DrawWorldVisCamera = *ptr_DrawWorldVisCamera;
	// 不再使用固定的scopeFrustum，而是使用动态更新的视锥体
	// NiFrustum modifiedFrustum = scopeFrustum; // 移除这行

	*ptr_DrawWorldCamera = scopeCamera;
	*ptr_DrawWorldVisCamera = scopeCamera;

	if (scopeCamera) {
		// 使用原始相机的视锥体作为基础，然后调整FOV
		scopeCamera->viewFrustum = originalCamera->viewFrustum;

		// 调整视锥体参数以适应瞄具的FOV
		float aspectRatio = scopeCamera->viewFrustum.right / scopeCamera->viewFrustum.top;
		float targetFOV = ScopeCamera::GetTargetFOV();

		// 限制FOV范围以避免极端俯仰角下的数值不稳定
		// 当俯仰角接近±90度时，减小FOV以保持投影矩阵稳定
		float pitch = asin(-scopeCamera->world.rotate.entry[2][1]);
		float pitchDeg = pitch * 57.295779513f;
		if (abs(pitchDeg) > 70.0f) {
			float factor = (90.0f - abs(pitchDeg)) / 20.0f;
			factor = std::max(0.3f, std::min(1.0f, factor));
			targetFOV = targetFOV * factor;
		}

		float fovRad = targetFOV * 0.01745329251f;
		float halfFovTan = tan(fovRad * 0.5f);

		// 根据FOV重新计算视锥体边界
		scopeCamera->viewFrustum.top = scopeCamera->viewFrustum.nearPlane * halfFovTan;
		scopeCamera->viewFrustum.bottom = -scopeCamera->viewFrustum.top;
		scopeCamera->viewFrustum.right = scopeCamera->viewFrustum.top * aspectRatio;
		scopeCamera->viewFrustum.left = -scopeCamera->viewFrustum.right;

		// 保持远裁剪面距离，确保远处的光源不会被裁剪
		scopeCamera->viewFrustum.farPlane = originalCamera->viewFrustum.farPlane;
	}

	// 同步光照累积器的眼睛位置，确保全方向光源的距离判断正确
	auto pDrawWorldAccum = *ptr_DrawWorldAccum;
	auto p1stPersonAccum = *ptr_Draw1stPersonAccum;
	auto pShadowSceneNode = *ptr_DrawWorldShadowNode;


	// 保存原始的眼睛位置以便后续恢复
	NiPoint3 originalAccumEyePos;
	NiPoint3 originalShadowNodeEyePos;
	NiPoint3 original1stAccumEyePos;
	float originalFarClip = 0.0f;

	// 备份光源和阴影状态，防止第二次渲染破坏渲染信息
	BSTArray<NiPointer<BSLight>> backupLightList;
	BSTArray<NiPointer<BSLight>> backupShadowLightList;
	BSTArray<NiPointer<BSLight>> backupAmbientLightList;
	bool lightsBackedUp = false;

	// 关键：保存光源的原始世界变换，因为渲染过程可能会修改它们
	struct LightTransformBackup {
		BSLight* light;
		NiTransform originalTransform;
	};
	std::vector<LightTransformBackup> lightTransformBackups;

	// 阴影系统状态管理标志
	bool shadowStateBackedUp = false;

	if (pShadowSceneNode) {
		pShadowSceneNode->bDisableLightUpdate = true;
	}

	if (pDrawWorldAccum) {
		originalAccumEyePos = pDrawWorldAccum->kEyePosition;
		pDrawWorldAccum->kEyePosition.x = scopeCamera->world.translate.x;
		pDrawWorldAccum->kEyePosition.y = scopeCamera->world.translate.y;
		pDrawWorldAccum->kEyePosition.z = scopeCamera->world.translate.z;
		pDrawWorldAccum->ClearActivePasses(true);
		pDrawWorldAccum->ClearRenderPasses();

		// 重置太阳遮挡查询
		pDrawWorldAccum->ResetSunOcclusion();
	}
	if (p1stPersonAccum) {
		original1stAccumEyePos = p1stPersonAccum->kEyePosition;
		p1stPersonAccum->kEyePosition.x = scopeCamera->world.translate.x;
		p1stPersonAccum->kEyePosition.y = scopeCamera->world.translate.y;
		p1stPersonAccum->kEyePosition.z = scopeCamera->world.translate.z;
	}

	// 同步ShadowSceneNode的眼睛位置
	if (pShadowSceneNode) {
		originalShadowNodeEyePos = pShadowSceneNode->kEyePosition;
		pShadowSceneNode->kEyePosition.x = scopeCamera->world.translate.x;
		pShadowSceneNode->kEyePosition.y = scopeCamera->world.translate.y;
		pShadowSceneNode->kEyePosition.z = scopeCamera->world.translate.z;

		// 确保光源更新不被禁用
		pShadowSceneNode->bDisableLightUpdate = false;

		originalFarClip = pShadowSceneNode->fStoredFarClip;
		pShadowSceneNode->fStoredFarClip = scopeCamera->viewFrustum.farPlane;
		pShadowSceneNode->bAlwaysUpdateLights = true;
	}
	auto geomListCullProc0 = *DrawWorldGeomListCullProc0;
	auto geomListCullProc1 = *DrawWorldGeomListCullProc1;

	if (geomListCullProc0 && scopeCamera) {
		geomListCullProc0->m_pkCamera = scopeCamera;
		geomListCullProc0->SetFrustum(&scopeCamera->viewFrustum);
	}

	if (geomListCullProc1 && scopeCamera) {
		geomListCullProc1->m_pkCamera = scopeCamera;
		geomListCullProc1->SetFrustum(&scopeCamera->viewFrustum);
	}

	//scopeCamera->port = scopeViewPort;
	NiUpdateData updateData{};
	updateData.camera = scopeCamera;
	scopeCamera->Update(updateData);
	scopeCamera->UpdateWorldData(&updateData);
	scopeCamera->UpdateWorldBound();

	// 计算视图矩阵的逆矩阵（世界矩阵）
	NiMatrix3 viewToWorld = scopeCamera->world.rotate;
	viewToWorld.Transpose(); // 旋转矩阵的逆等于其转置

	if (pDrawWorldAccum) {
		// 设置累积器使用瞄具相机进行光照计算
		pDrawWorldAccum->StartAccumulating(scopeCamera);

		pDrawWorldAccum->m_pkCamera = scopeCamera;
	}

	etRenderMode originalRenderMode;
	bool originalZPrePass = false;
	bool originalRenderDecals = false;
	bool original1stPerson = false;

	if (pDrawWorldAccum) {
		originalRenderMode = pDrawWorldAccum->GetRenderMode();
		originalZPrePass = pDrawWorldAccum->QZPrePass();
		originalRenderDecals = pDrawWorldAccum->QRenderDecals();
		original1stPerson = pDrawWorldAccum->Q1stPerson();
		shadowStateBackedUp = true;
	}

	if (pShadowSceneNode) {
		// 备份当前的光源状态（这是第一次渲染后的正确状态）
		backupLightList = pShadowSceneNode->lLightList;
		backupShadowLightList = pShadowSceneNode->lShadowLightList;
		backupAmbientLightList = pShadowSceneNode->lAmbientLightList;
		lightsBackedUp = true;
	}

	{
		// 保存当前光源列表的状态
		auto lightCount = pShadowSceneNode->lLightList.size();
		auto shadowLightCount = pShadowSceneNode->lShadowLightList.size();
		auto ambientLightCount = pShadowSceneNode->lAmbientLightList.size();

	}

	// === 第一次渲染后保存光源状态 ===
	// 在这个时间点，第一次渲染已经完成，光源状态应该是有效的
	g_LightStateBackups.clear();

	// 保存所有光源的当前状态（第一次渲染后的状态）
	if (pShadowSceneNode) {
		g_LightStateBackups.reserve(pShadowSceneNode->lLightList.size());
		for (size_t i = 0; i < pShadowSceneNode->lLightList.size(); i++) {
			auto bsLight = pShadowSceneNode->lLightList[i];
			if (bsLight && bsLight.get()) {
				LightStateBackup backup{};
				backup.light = bsLight;
				backup.frustumCull = bsLight->usFrustumCull;
				backup.occluded = bsLight->bOccluded;
				backup.temporary = bsLight->bTemporary;
				backup.dynamic = bsLight->bDynamicLight;
				backup.lodDimmer = bsLight->fLODDimmer;
				backup.camera = bsLight->spCamera;
				backup.cullingProcess = bsLight->pCullingProcess;
				g_LightStateBackups.push_back(backup);
			}
		}
	}


	// 在第二次渲染之前应用优化的光源状态
	if (!g_LightStateBackups.empty()) {
		for (const auto& firstRenderState : g_LightStateBackups) {
			auto bsLight = firstRenderState.light.get();
			if (!bsLight) {
				continue;
			}

			if (firstRenderState.frustumCull == 0xFF || firstRenderState.frustumCull == 0xFE) {
				bsLight->usFrustumCull = firstRenderState.frustumCull;
			} else {
				bsLight->usFrustumCull = 0xFF;  // BSL_ALL
			}

			bsLight->SetOccluded(firstRenderState.occluded);
			bsLight->SetTemporary(firstRenderState.temporary);
			bsLight->SetLODFade(false);  // 暂不持久保存LODFade，保持关闭以提升可见性
			bsLight->fLODDimmer = firstRenderState.lodDimmer;
			if (bsLight->bDynamicLight != firstRenderState.dynamic) {
				bsLight->SetDynamic(firstRenderState.dynamic);
			}

			bsLight->SetAffectLand(true);
			bsLight->SetAffectWater(true);
			bsLight->SetIgnoreRoughness(false);
			bsLight->SetIgnoreRim(false);
			bsLight->SetAttenuationOnly(false);
			bsLight->spCamera = firstRenderState.camera;

			if (firstRenderState.cullingProcess) {
				bsLight->SetCullingProcess(firstRenderState.cullingProcess);
			} else if (*DrawWorldCullingProcess) {
				bsLight->SetCullingProcess(*DrawWorldCullingProcess);
			}
		}
	}

	if (pDrawWorldAccum) {
		static int debugFrameCount = 0;
		debugFrameCount++;

		// 确保眼睛位置是瞄准镜相机的位置
		if (scopeCamera) {
			NiPoint3A scopeEyePos =  NiPoint3A(scopeCamera->world.translate.x, scopeCamera->world.translate.y, scopeCamera->world.translate.z);
			pDrawWorldAccum->kEyePosition = scopeEyePos;

			
			pDrawWorldAccum->ClearActivePasses(false);  // 不清除全部，只清除活动的pass

		}
	}

	g_RenderPreUIOriginal(savedDrawWorld); 

	// 立即重新启用光源更新
	if (pShadowSceneNode) {
		pShadowSceneNode->bDisableLightUpdate = false;
	}

	// === 第二次渲染后恢复原始光源状态 ===
	if (!g_LightStateBackups.empty()) {
		for (const auto& backup : g_LightStateBackups) {
			auto bsLight = backup.light.get();
			if (!bsLight) {
				continue;
			}

			bsLight->usFrustumCull = backup.frustumCull;
			bsLight->SetOccluded(backup.occluded);
			bsLight->SetTemporary(backup.temporary);
			bsLight->fLODDimmer = backup.lodDimmer;
			bsLight->spCamera = backup.camera;
			bsLight->SetCullingProcess(backup.cullingProcess);
			if (bsLight->bDynamicLight != backup.dynamic) {
				bsLight->SetDynamic(backup.dynamic);
			}
		}
	}

	ScopeCamera::SetRenderingForScope(false);

	// 立即恢复原始的眼睛位置和状态
	// 这是关键：确保下一帧的光照计算使用正确的参考点
	if (pDrawWorldAccum) {
		pDrawWorldAccum->kEyePosition = originalAccumEyePos;
		// 注意：我们在最后的状态恢复中再次清理累积器
	}
	if (p1stPersonAccum) {
		p1stPersonAccum->kEyePosition = original1stAccumEyePos;
		p1stPersonAccum->ClearActivePasses(false);
	}
	if (pShadowSceneNode) {
		pShadowSceneNode->kEyePosition = originalShadowNodeEyePos;
		pShadowSceneNode->bDisableLightUpdate = false;

		// 恢复原始的远裁剪面和AlwaysUpdateLights设置
		pShadowSceneNode->fStoredFarClip = originalFarClip;
		pShadowSceneNode->bAlwaysUpdateLights = false;

		// 不再恢复光源列表，让引擎自己管理
		// 恢复光源列表可能导致引用计数问题
		if (false && lightsBackedUp) {

			static int frameCount1 = 0;
			frameCount1++;
			pShadowSceneNode->lLightList = backupLightList;
			pShadowSceneNode->lShadowLightList = backupShadowLightList;
			pShadowSceneNode->lAmbientLightList = backupAmbientLightList;

			// 调试：检查恢复后的光源数量
			if (frameCount1 % 30 == 0) {
				logger::debug("Restored lights: {}, shadows: {}, ambient: {}",
					pShadowSceneNode->lLightList.size(),
					pShadowSceneNode->lShadowLightList.size(),
					pShadowSceneNode->lAmbientLightList.size());
			}
		}

		// 清理光照队列，防止队列状态影响下一帧
		// 注意：我们不清空主光源列表，只清理队列
		if (pShadowSceneNode->lLightQueueAdd.size() > 0) {
			pShadowSceneNode->lLightQueueAdd.resize(0);
		}
		if (pShadowSceneNode->lLightQueueRemove.size() > 0) {
			pShadowSceneNode->lLightQueueRemove.resize(0);
		}
	}

	// 完全恢复BSShaderAccumulator的状态
	if (pDrawWorldAccum && shadowStateBackedUp) {
		// 恢复原始的渲染模式和状态
		pDrawWorldAccum->SetRenderMode(originalRenderMode);
		pDrawWorldAccum->SetZPrePass(originalZPrePass);
		pDrawWorldAccum->SetRenderDecals(originalRenderDecals);
		pDrawWorldAccum->Set1stPerson(original1stPerson);

		// 重置所有可能被第二次渲染影响的状态
		pDrawWorldAccum->ResetSunOcclusion();
		pDrawWorldAccum->ClearSunQueries();
		pDrawWorldAccum->ClearActivePasses(false);
		pDrawWorldAccum->ClearRenderPasses();
	}

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

	*ptr_DrawWorldCamera = originalCamera;
	DrawWorld::SetCamera(originalCamera);
	DrawWorld::SetUpdateCameraFOV(true);

	nData.camera = originalCamera;
	originalCamera->Update(nData);

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
	savedDrawWorld = ptr_drawWorld;
	D3DEventNode(g_RenderPreUIOriginal(ptr_drawWorld), L"Render_PreUI");
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

void __fastcall hkDrawWorld_LightUpdate(uint64_t ptr_drawWorld)
{
	// 直接调用原函数，不要修改任何状态
	// 光源位置的同步已经在hkTAA中处理了
	g_DrawWorldLightUpdateOriginal(ptr_drawWorld);
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
	// 瞄具渲染时的处理已经在hkRender_PreUI中完成了眼睛位置同步
	// 这里直接执行原始函数即可
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
	if (ScopeCamera::IsRenderingForScope()) {
		auto shadowNode = *ptr_DrawWorldShadowNode;
		if (shadowNode && !g_LightStateBackups.empty()) {
			for (const auto& backup : g_LightStateBackups) {
				auto light = backup.light.get();
				if (!light) {
					continue;
				}

				light->usFrustumCull = backup.frustumCull;
				light->SetOccluded(backup.occluded);
				light->SetTemporary(backup.temporary);
				light->fLODDimmer = backup.lodDimmer;
				light->spCamera = backup.camera;
				light->SetCullingProcess(backup.cullingProcess);
				if (light->bDynamicLight != backup.dynamic) {
					light->SetDynamic(backup.dynamic);
				}
			}
		}
	}

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
	ScopeCamera::hasFirstSpawnNode = false;
	ScopeCamera::isDelayStarted = false;
	ScopeCamera::isFirstScopeRender = true;

	// 读档后立即恢复ZoomData
	std::thread([]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 等待武器完全加载
		ScopeCamera::RestoreZoomDataForCurrentWeapon();
		logger::info("Restored ZoomData after load game");
	}).detach();
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

// 延迟 hook TAA 的函数
void DelayedTAAHook()
{
	static int retryCount = 0;
	const int maxRetries = 5;

	REL::Relocation<std::uintptr_t> TAAFunc(REL::ID(528052));
	void* targetAddr = reinterpret_cast<void*>(TAAFunc.address());

	// 检查 hook 是否已经生效
	if (g_TAA != nullptr) {
		logger::info("TAA hook already active");
		return;
	}

	retryCount++;
	logger::info("Attempting TAA hook (attempt {}/{})", retryCount, maxRetries);

	// 尝试创建 hook
	MH_STATUS status = MH_CreateHook(targetAddr,
	                                 reinterpret_cast<void*>(&hkTAA),
	                                 reinterpret_cast<void**>(&g_TAA));

	if (status == MH_OK && MH_EnableHook(targetAddr) == MH_OK) {
		logger::info("TAA hook successfully created on attempt {}", retryCount);
	} else if (retryCount < maxRetries) {
		// 如果失败且未达到最大重试次数，设置定时器重试
		std::thread([targetAddr]() {
			std::this_thread::sleep_for(std::chrono::seconds(2));
			DelayedTAAHook();
		}).detach();
	} else {
		logger::error("Failed to create TAA hook after {} attempts", maxRetries);
	}
}

void RegisterHooks()
{
	logger::info("Registering hooks...");
	using namespace Utilities;

	CreateAndEnableHook((LPVOID)DrawWorld_LightUpdate_Ori.address(), &hkDrawWorld_LightUpdate, reinterpret_cast<LPVOID*>(&g_DrawWorldLightUpdateOriginal), "DrawWorld_LightUpdate");

	CreateAndEnableHook((LPVOID)Renderer_DoZPrePass_Ori.address(), &hkRenderer_DoZPrePass, reinterpret_cast<LPVOID*>(&g_pDoZPrePassOriginal), "DoZPrePass");
	CreateAndEnableHook((LPVOID)BSGraphics_RenderZPrePass_Ori.address(), &hkRenderZPrePass, reinterpret_cast<LPVOID*>(&g_RenderZPrePassOriginal), "RenderZPrePass");
	/*CreateAndEnableHook((LPVOID)BSGraphics_RenderAlphaTestZPrePass_Ori.address(), &hkRenderAlphaTestZPrePass, reinterpret_cast<LPVOID*>(&g_RenderAlphaTestZPrePassOriginal), "RenderAlphaTestZPrePass");*/

	CreateAndEnableHook((LPVOID)BSShaderAccumulator_ResetSunOcclusion_Ori.address(), &hkBSShaderAccumulator_ResetSunOcclusion,
		reinterpret_cast<LPVOID*>(&g_ResetSunOcclusionOriginal), "ResetSunOcclusion");

	/*CreateAndEnableHook((LPVOID)BSDistantObjectInstanceRenderer_Render_Ori.address(), &hkBSDistantObjectInstanceRenderer_Render,
		reinterpret_cast<LPVOID*>(&g_BSDistantObjectInstanceRenderer_RenderOriginal), "BSDistantObjectInstanceRenderer_Render");*/

	/*CreateAndEnableHook((LPVOID)RenderTargetManager_DecompressDepthStencilTarget_Ori.address(), &hkDecompressDepthStencilTarget,
		reinterpret_cast<LPVOID*>(&g_DecompressDepthStencilTargetOriginal), "DecompressDepthStencilTarget");*/

	CreateAndEnableHook((LPVOID)DrawWorld_Render_PreUI_Ori.address(), &hkRender_PreUI,
		reinterpret_cast<LPVOID*>(&g_RenderPreUIOriginal), "Render_PreUI");

	CreateAndEnableHook((LPVOID)DrawWorld_Begin_Ori.address(), &hkBegin,
		reinterpret_cast<LPVOID*>(&g_BeginOriginal), "Begin");

	CreateAndEnableHook((LPVOID)Main_DrawWorldAndUI_Ori.address(), &hkMain_DrawWorldAndUI,
		reinterpret_cast<LPVOID*>(&g_DrawWorldAndUIOriginal), "Main_DrawWorldAndUI");

	//CreateAndEnableHook((LPVOID)BSCullingGroup_Process_Ori.address(), &hkBSCullingGroup_Process,
	//	reinterpret_cast<LPVOID*>(&g_BSCullingGroupProcessOriginal), "BSCullingGroup_Process");

	CreateAndEnableHook((LPVOID)DrawWorld_MainAccum_Ori.address(), &hkMainAccum,
		reinterpret_cast<LPVOID*>(&g_MainAccumOriginal), "MainAccum");

	CreateAndEnableHook((LPVOID)DrawWorld_MainRenderSetup_Ori.address(), &hkMainRenderSetup,
		reinterpret_cast<LPVOID*>(&g_MainRenderSetupOriginal), "MainRenderSetup");

	/*CreateAndEnableHook((LPVOID)DrawWorld_OpaqueWireframe_Ori.address(), &hkOpaqueWireframe,
		reinterpret_cast<LPVOID*>(&g_OpaqueWireframeOriginal), "OpaqueWireframe");*/

	/*CreateAndEnableHook((LPVOID)DrawWorld_DeferredPrePass_Ori.address(), &hkDeferredPrePass,
		reinterpret_cast<LPVOID*>(&g_DeferredPrePassOriginal), "DeferredPrePass");*/

	CreateAndEnableHook((LPVOID)DrawWorld_DeferredLightsImpl_Ori.address(), &hkDeferredLightsImpl,
		reinterpret_cast<LPVOID*>(&g_DeferredLightsImplOriginal), "DeferredLightsImpl");

	/*CreateAndEnableHook((LPVOID)DrawWorld_DeferredComposite_Ori.address(), &hkDeferredComposite,
		reinterpret_cast<LPVOID*>(&g_DeferredCompositeOriginal), "DeferredComposite");*/

	CreateAndEnableHook((LPVOID)DrawWorld_Forward_Ori.address(), &hkDrawWorld_Forward,
		reinterpret_cast<LPVOID*>(&g_ForwardOriginal), "DrawWorld_Forward");

	CreateAndEnableHook((LPVOID)DrawWorld_Refraction_Ori.address(), &hkDrawWorld_Refraction,
		reinterpret_cast<LPVOID*>(&g_RefractionOriginal), "DrawWorld_Refraction");

	CreateAndEnableHook((LPVOID)DrawWorld_Add1stPersonGeomToCuller_Ori.address(), &hkAdd1stPersonGeomToCuller,
		reinterpret_cast<LPVOID*>(&g_Add1stPersonGeomToCullerOriginal), "Add1stPersonGeomToCuller");

	/*CreateAndEnableHook((LPVOID)RTM_CreateRenderTarget_Ori.address(), &hkRTManager_CreateRenderTarget,
		reinterpret_cast<LPVOID*>(&g_RTManagerCreateRenderTargetOriginal), "RTManager_CreateRenderTarget");*/

	// 重新启用BSBatchRenderer_Draw hook，在其中添加剔除逻辑
	CreateAndEnableHook((LPVOID)BSBatchRenderer_Draw_Ori.address(), &hkBSBatchRenderer_Draw,
		reinterpret_cast<LPVOID*>(&g_originalBSBatchRendererDraw), "BSBatchRenderer_Draw");
	 
	 CreateAndEnableHook((LPVOID)PCUpdateMainThread_Ori.address(), &hkPCUpdateMainThread,
		reinterpret_cast<LPVOID*>(&g_PCUpdateMainThread), "PCUpdateMainThread");

	 CreateAndEnableHook((LPVOID)MainPreRender_Ori.address(), &hkMainPreRender,
		 reinterpret_cast<LPVOID*>(&g_MainPreRender), "MainPreRender");

	 CreateAndEnableHook((LPVOID)BSCullingGroupAdd_Ori.address(), &hkBSCullingGroupAdd,
		 reinterpret_cast<LPVOID*>(&g_BSCullingGroupAdd), "BSCullingGroupAdd");

	 CreateAndEnableHook((LPVOID)BSCullingGroup_SetCompoundFrustum_Ori.address(), &hkBSCullingGroup_SetCompoundFrustum,
		 reinterpret_cast<LPVOID*>(&g_BSCullingGroup_SetCompoundFrustum), "BSCullingGroup_SetCompoundFrustum");
	 
	 
	 CreateAndEnableHook((LPVOID)DrawWorld_Move1stPersonToOrigin_Ori.address(), &hkDrawWorld_Move1stPersonToOrigin,
		 reinterpret_cast<LPVOID*>(&g_DrawWorld_Move1stPersonToOrigin), "DrawWorld_Move1stPersonToOrigin");

	 /* CreateAndEnableHook((LPVOID)DrawTriShape_Ori.address(), &hkDrawTriShape,
		  reinterpret_cast<LPVOID*>(&g_DrawTriShape), "DrawTriShape");*/

	  //F4SE::Trampoline& trampoline = F4SE::GetTrampoline();
	  //g_DrawIndexed = (FnDrawIndexed)trampoline.write_branch<5>(DrawIndexed_Ori.address(), reinterpret_cast<uintptr_t>(hkDrawIndexed));
	  //

	// Hook TAA render function with conflict detection
	REL::Relocation<std::uintptr_t> TAAFunc(REL::ID(528052));  // 直接使用函数ID
	void* targetAddr = reinterpret_cast<void*>(TAAFunc.address());

	// 1. 检测是否已经被其他 mod hook 了
	uint8_t* funcBytes = reinterpret_cast<uint8_t*>(targetAddr);
	bool alreadyHooked = false;

	// 检查常见的 hook 模式 (JMP, CALL 等)
	if (funcBytes[0] == 0xE9 || funcBytes[0] == 0xFF || funcBytes[0] == 0x48) {
		logger::warn("TAA function may already be hooked by another mod. First bytes: {:02X} {:02X} {:02X} {:02X} {:02X}",
		            funcBytes[0], funcBytes[1], funcBytes[2], funcBytes[3], funcBytes[4]);
		alreadyHooked = true;
	}

	// 2. 尝试移除已存在的 hook (如果有)
	if (alreadyHooked) {
		logger::info("Attempting to remove existing hook...");
		MH_DisableHook(targetAddr);
		MH_RemoveHook(targetAddr);
	}

	// 输出调试信息
	logger::info("Target TAA function address: {:X}", reinterpret_cast<uintptr_t>(targetAddr));
	logger::info("Hook function (hkTAA) address: {:X}", reinterpret_cast<uintptr_t>(&hkTAA));

	// 3. 创建我们的 hook
	MH_STATUS status = MH_CreateHook(targetAddr,
	                                 reinterpret_cast<void*>(&hkTAA),
	                                 reinterpret_cast<void**>(&g_TAA));

	if (status != MH_OK) {
		logger::error("Failed to create TAA hook. Status: {}", static_cast<int>(status));

		// 4. 如果失败，尝试使用虚函数表 hook 作为后备方案
		logger::info("Attempting vtable hook as fallback...");
		REL::Relocation<std::uintptr_t> vtable_TAA(RE::ImageSpaceEffectTemporalAA::VTABLE[0]);
		void** vtablePtr = reinterpret_cast<void**>(vtable_TAA.address());
		void* originalFunc = vtablePtr[1];

		// 检查虚函数表是否指向我们期望的函数
		if (reinterpret_cast<uintptr_t>(originalFunc) == TAAFunc.address()) {
			logger::info("Vtable points to expected function, modifying vtable directly");
		} else {
			logger::warn("Vtable function differs from expected. Vtable: {:X}, Expected: {:X}",
			            reinterpret_cast<uintptr_t>(originalFunc), TAAFunc.address());
		}

		// 保存原始函数
		g_TAA = reinterpret_cast<FnTAA>(originalFunc);

		// 修改虚函数表
		DWORD oldProtect;
		if (VirtualProtect(&vtablePtr[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
			vtablePtr[1] = reinterpret_cast<void*>(&hkTAA);
			VirtualProtect(&vtablePtr[1], sizeof(void*), oldProtect, &oldProtect);
			logger::info("TAA vtable hook applied successfully");
		}
	} else if (MH_EnableHook(targetAddr) != MH_OK) {
		logger::error("Failed to enable TAA hook");
	} else {
		logger::info("TAA hook created successfully at address: {:X} (ID: 528052)", TAAFunc.address());
		logger::info("Original function (g_TAA) points to: {:X}", reinterpret_cast<uintptr_t>(g_TAA));

		// 5. 验证 hook 是否真正生效
		funcBytes = reinterpret_cast<uint8_t*>(targetAddr);
		logger::info("After hook - First bytes: {:02X} {:02X} {:02X} {:02X} {:02X}",
		            funcBytes[0], funcBytes[1], funcBytes[2], funcBytes[3], funcBytes[4]);

		// 验证 g_TAA 是否可调用
		if (g_TAA) {
			logger::info("g_TAA is valid and points to trampoline");
		} else {
			logger::error("g_TAA is null after successful hook!");
		}
	}

	// 6. 如果初始 hook 失败，启动延迟 hook
	if (g_TAA == nullptr) {
		logger::warn("Initial TAA hook failed, starting delayed hook attempts...");
		std::thread([]() {
			std::this_thread::sleep_for(std::chrono::seconds(3));
			DelayedTAAHook();
		}).detach();
	}

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
	ggg_ScopeCamera = ThroughScope::ScopeCamera::GetScopeCamera();

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

	originalCamera1stviewFrustum.bottom = -0.84f;
	originalCamera1stviewFrustum.top = 0.84f;
	originalCamera1stviewFrustum.left = 0.472f;
	originalCamera1stviewFrustum.right = -0.472f;
	originalCamera1stviewFrustum.nearPlane = 10;
	originalCamera1stviewFrustum.farPlane = 10240.0f;

	float scopeViewSize = 0.125f;
	scopeFrustum.left = -scopeViewSize * 2;
	scopeFrustum.right = scopeViewSize * 2;
	scopeFrustum.top = scopeViewSize;
	scopeFrustum.bottom = -scopeViewSize;
	scopeFrustum.nearPlane = 15;
	scopeFrustum.farPlane = 353840.0f;
	scopeFrustum.ortho = false;

	scopeViewPort.left = 0.4f;
	scopeViewPort.right = 0.6f;
	scopeViewPort.top = 0.6f;
	scopeViewPort.bottom = 0.4f;

	originalFrustum = originalCamera1stviewFrustum;

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
	//SetConsoleOutputCP(CP_UTF8);
	//SetConsoleCP(CP_UTF8);

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
			upscalerModular = LoadLibraryA("Data/F4SE/Plugins/Fallout4Upscaler.dll");
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
