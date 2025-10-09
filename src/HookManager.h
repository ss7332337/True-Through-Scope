#pragma once

#include <d3d11.h>
#include <Windows.h>
#include <cstdint>

// Forward declarations for CommonLibF4 types
namespace RE
{
	namespace BSGraphics
	{
		struct ZPrePassDrawData;
		struct AlphaTestZPrePassDrawData;
		struct RendererShadowState;
		struct TriShape;
		class Renderer;
		class RenderTargetProperties;
		class RenderTargetManager;
		class RenderTarget;
	}

	class BSShaderAccumulator;
	class BSEffectShaderProperty;
	class BSGeometry;
	class BSRenderPass;
	class BSStream;
	class NiBinaryStream;
	class PlayerCharacter;
	class Main;
	class ImageSpaceEffectTemporalAA;
	class BSTriShape;
	class ImageSpaceEffectParam;
	class BSCullingGroup;
	class NiAVObject;
	class NiBound;
	class BSCompoundFrustum;
	class NiCamera;
	class BSLight;
	class BSDynamicTriShape;
	class BSRenderPass;
	class BSStream;
	class NiBinaryStream;
}

namespace ThroughScope
{
	using namespace RE;
	using namespace RE::BSGraphics;

	// Forward declarations for hook functions
	void __fastcall hkDrawWorld_LightUpdate(uint64_t ptr_drawWorld);
	void __fastcall hkRenderer_DoZPrePass(uint64_t thisPtr, NiCamera* apFirstPersonCamera, NiCamera* apWorldCamera, float afFPNear, float afFPFar, float afNear, float afFar);
	void __fastcall hkRenderZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::ZPrePassDrawData* aZPreData, unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode);
	void __fastcall hkRenderAlphaTestZPrePass(BSGraphics::RendererShadowState* rshadowState, BSGraphics::AlphaTestZPrePassDrawData* aZPreData, unsigned __int64* aVertexDesc, unsigned __int16* aCullmode, unsigned __int16* aDepthBiasMode, ID3D11SamplerState** aCurSamplerState);
	void __fastcall hkBSShaderAccumulator_ResetSunOcclusion(BSShaderAccumulator* thisPtr);
	void __fastcall hkBSDistantObjectInstanceRenderer_Render(uint64_t thisPtr);
	void __fastcall hkDecompressDepthStencilTarget(RenderTargetManager* thisPtr, int index);
	void __fastcall hkRender_PreUI(uint64_t ptr_drawWorld);
	void __fastcall hkBegin(uint64_t ptr_drawWorld);
	void __fastcall hkMain_DrawWorldAndUI(uint64_t ptr_drawWorld, bool abBackground);
	void __fastcall hkBSCullingGroup_Process(BSCullingGroup* thisPtr, bool abFirstStageOnly);
	void __fastcall hkMainAccum(uint64_t ptr_drawWorld);
	void __fastcall hkOcclusionMapRender();
	void __fastcall hkMainRenderSetup(uint64_t ptr_drawWorld);
	void __fastcall hkOpaqueWireframe(uint64_t ptr_drawWorld);
	void __fastcall hkDeferredPrePass(uint64_t ptr_drawWorld);
	void __fastcall hkDeferredLightsImpl(uint64_t ptr_drawWorld);
	void __fastcall hkDeferredComposite(uint64_t ptr_drawWorld);
	void __fastcall hkDrawWorld_Forward(uint64_t ptr_drawWorld);
	void __fastcall hkDrawWorld_Refraction(uint64_t this_ptr);
	void __fastcall hkAdd1stPersonGeomToCuller(uint64_t thisPtr);
	void __fastcall hkRTManager_CreateRenderTarget(RenderTargetManager rtm, int aIndex, const RenderTargetProperties* arProperties, TARGET_PERSISTENCY aPersistent);
	void hkBSBatchRenderer_Draw(BSRenderPass* apRenderPass);
	void hkMapDynamicTriShapeDynamicData(Renderer* renderer, BSDynamicTriShape* bsDynamicTriShape, DynamicTriShape* dynamicTriShape, DynamicTriShapeDrawData* drawdata, unsigned int auiSize);
	void hkBSStreamLoad(BSStream* stream, const char* apFileName, NiBinaryStream* apStream);
	void __fastcall hkPCUpdateMainThread(PlayerCharacter* pChar);
	void hkDrawTriShape(BSGraphics::Renderer* thisPtr, BSGraphics::TriShape* apTriShape, unsigned int auiStartIndex, unsigned int auiNumTriangles);
	void __fastcall hkMainPreRender(Main* thisPtr, int auiDestination);
	void __fastcall hkTAA(ImageSpaceEffectTemporalAA*, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param);
	void hkBSCullingGroupAdd(BSCullingGroup* thisPtr, NiAVObject* apObj, const NiBound* aBound, const unsigned int aFlags);
	void __fastcall hkBSShaderAccumulator_RenderBatches(BSShaderAccumulator* thisPtr, int aiShader, bool abAlphaPass, int aeGroup);
	void hkBSCullingGroup_SetCompoundFrustum(BSCullingGroup* thisPtr, BSCompoundFrustum* apCompoundFrustum);
	void hkDrawWorld_Move1stPersonToOrigin(uint64_t thisPtr);
	void __fastcall hkBSShaderAccumulator_RenderBlendedDecals(BSShaderAccumulator* thisPtr);
	void __fastcall hkBSShaderAccumulator_RenderOpaqueDecals(BSShaderAccumulator* thisPtr);
	RenderTarget* __fastcall hkRenderer_CreateRenderTarget(Renderer* renderer, int aId, const wchar_t* apName, const RenderTargetProperties* aProperties);
	void __fastcall hkDoUmbraQuery(uint64_t ptr_drawWorld);

	class HookManager
	{
	public:
		static HookManager* GetSingleton()
		{
			static HookManager instance;
			return &instance;
		}

		void RegisterAllHooks();

		#pragma region Function Type Definitions
		typedef void (*FnDrawWorldLightUpdate)(uint64_t);
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
		typedef void(__fastcall* FnTAA)(ImageSpaceEffectTemporalAA*, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param);
		typedef void (*FnBSCullingGroupAdd)(BSCullingGroup*, NiAVObject* apObj, const NiBound* aBound, const unsigned int aFlags);
		typedef void (*FnBSShaderAccumulator_RenderBatches)(BSShaderAccumulator*, int, bool, int);
		typedef void (*FnBSCullingGroup_SetCompoundFrustum)(BSCullingGroup*, BSCompoundFrustum*);
		typedef void (*FnDrawWorld_Move1stPersonToOrigin)(uint64_t);
		typedef void (*FnBSShaderAccumulator_RenderBlendedDecals)(BSShaderAccumulator*);
		typedef void (*FnBSShaderAccumulator_RenderOpaqueDecals)(BSShaderAccumulator*);
		typedef RenderTarget* (*FnRenderer_CreateRenderTarget)(Renderer*, int, const wchar_t*, const RenderTargetProperties*);
		typedef void (*FnOcclusionMapRender)();
		typedef void (*FnDoUmbraQuery)(uint64_t);
		#pragma endregion

		#pragma region Original Function Pointers
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
		FnBSShaderAccumulator_RenderBlendedDecals g_BSShaderAccumulator_RenderBlendedDecals = nullptr;
		FnBSShaderAccumulator_RenderOpaqueDecals g_BSShaderAccumulator_RenderOpaqueDecals = nullptr;
		FnRenderer_CreateRenderTarget g_Renderer_CreateRenderTarget = nullptr;
		FnOcclusionMapRender g_OcclusionMapRender = nullptr;
		FnDoUmbraQuery g_DoUmbraQuery = nullptr;
		#pragma endregion

	private:
		HookManager() = default;
		~HookManager() = default;
		HookManager(const HookManager&) = delete;
		HookManager& operator=(const HookManager&) = delete;

		#pragma region REL Relocations
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

		REL::Relocation<uintptr_t> DrawWorld_Add1stPersonGeomToCuller_Ori{ REL::ID(414086) };
		REL::Relocation<uintptr_t> BSShaderAccumulator_RenderBatches_Ori{ REL::ID(1048494) };
		REL::Relocation<uintptr_t> BSShaderAccumulator_RenderOpaqueDecals_Ori{ REL::ID(163409) };
		REL::Relocation<uintptr_t> BSShaderAccumulator_RenderBlendedDecals_Ori{ REL::ID(761249) };

		REL::Relocation<uintptr_t> DrawWorld_Begin_Ori{ REL::ID(502840) };
		REL::Relocation<uintptr_t> Main_DrawWorldAndUI_Ori{ REL::ID(408683) };
		REL::Relocation<uintptr_t> PCUpdateMainThread_Ori{ REL::ID(1134912) };

		REL::Relocation<uintptr_t> BSCullingGroup_Process_Ori{ REL::ID(1147875) };
		REL::Relocation<uintptr_t> Renderer_CreateRenderTarget_Ori{ REL::ID(425575) };
		REL::Relocation<uintptr_t> RTM_CreateRenderTarget_Ori{ REL::ID(43433) };

		REL::Relocation<uintptr_t> DrawWorld_LightUpdate_Ori{ REL::ID(918638) };
		REL::Relocation<uintptr_t> Renderer_DoZPrePass_Ori{ REL::ID(1491502) };
		REL::Relocation<uintptr_t> BSGraphics_RenderZPrePass_Ori{ REL::ID(901559) };
		REL::Relocation<uintptr_t> BSGraphics_RenderAlphaTestZPrePass_Ori{ REL::ID(767228) };

		REL::Relocation<uintptr_t> BSDistantObjectInstanceRenderer_Render_Ori{ REL::ID(148163) };
		REL::Relocation<uintptr_t> BSShaderAccumulator_ResetSunOcclusion_Ori{ REL::ID(371166) };
		REL::Relocation<uintptr_t> RenderTargetManager_DecompressDepthStencilTarget_Ori{ REL::ID(338650) };

		REL::Relocation<uintptr_t> BSP_GetRenderPasses_Ori{ REL::ID(1289086) };
		REL::Relocation<uintptr_t> BSBatchRenderer_Draw_Ori{ REL::ID(1152191) };

		REL::Relocation<uintptr_t> DrawTriShape_Ori{ REL::ID(763320) };
		REL::Relocation<uintptr_t> DrawIndexed_Ori{ REL::ID(763320), 0x137 };

		REL::Relocation<uintptr_t> MapDynamicTriShapeDynamicData_Ori{ REL::ID(732935) };
		REL::Relocation<uintptr_t> BSStreamLoad_Ori{ REL::ID(160035) };

		REL::Relocation<uintptr_t> MainPreRender_Ori{ REL::ID(378257) };
		REL::Relocation<uintptr_t> BSCullingGroupAdd_Ori{ REL::ID(1175493) };
		REL::Relocation<uintptr_t> BSCullingGroup_SetCompoundFrustum_Ori{ REL::ID(158202) };

		REL::Relocation<uintptr_t> DrawWorld_Move1stPersonToOrigin_Ori{ REL::ID(76526) };
		REL::Relocation<uintptr_t> DrawWorld_DoUmbraQuery_Ori{ REL::ID(1264353) };
		#pragma endregion

		void RegisterTAAHook();
		void DelayedTAAHook();
	};
}
