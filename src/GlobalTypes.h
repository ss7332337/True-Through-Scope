#pragma once
#include <NiFLoader.h>
#include <Windows.h>

// 前向声明
namespace ThroughScope
{
	class D3DHooks;
}

namespace ThroughScope
{
	// 光源状态备份结构体
	struct LightStateBackup
	{
		RE::NiPointer<RE::BSLight> light;
		uint32_t frustumCull;
		bool occluded;
		bool temporary;
		bool dynamic;
		float lodDimmer;
		RE::NiPointer<RE::NiCamera> camera;
		RE::BSCullingProcess* cullingProcess;
	};

	// 全局变量声明（在main.cpp中定义）
	extern std::vector<LightStateBackup> g_LightStateBackups;
	extern uint64_t savedDrawWorld;
	extern RE::PlayerCharacter* g_pchar;
	extern ThroughScope::D3DHooks* d3dHooks;
	extern NIFLoader* nifloader;
	extern HMODULE upscalerModular;
	extern RE::NiCamera* ggg_ScopeCamera;

	// 保存瞄具渲染时正确的 ViewProjMat（在 SetCameraData 后保存）
	// 用于 BSSkyShader::SetupGeometry Hook
	extern __m128 g_ScopeViewProjMat[4];
	extern bool g_ScopeViewProjMatValid;

	// 保存主相机的 FOV（角度制），在第一次渲染时从 PlayerCamera->firstPersonFOV 获取
	// 用于 BSSkyShader::SetupGeometry Hook 计算缩放因子
	extern float g_MainCameraFOV;

	// REL指针声明
	extern REL::Relocation<RE::ShadowSceneNode**> ptr_DrawWorldShadowNode;
	extern REL::Relocation<RE::NiCamera**> ptr_DrawWorldCamera;
	extern REL::Relocation<RE::NiCamera**> ptr_DrawWorld1stCamera;
	extern REL::Relocation<RE::NiCamera**> ptr_DrawWorldVisCamera;
	extern REL::Relocation<RE::NiCamera**> ptr_BSShaderManagerSpCamera;
	extern REL::Relocation<RE::NiCamera**> ptr_DrawWorldSpCamera;
	extern REL::Relocation<RE::BSShaderAccumulator**> ptr_DrawWorldAccum;
	extern REL::Relocation<RE::BSShaderAccumulator**> ptr_Draw1stPersonAccum;
	extern REL::Relocation<RE::BSGeometryListCullingProcess**> DrawWorldGeomListCullProc0;
	extern REL::Relocation<RE::BSGeometryListCullingProcess**> DrawWorldGeomListCullProc1;
	extern REL::Relocation<RE::BSCullingProcess**> DrawWorldCullingProcess;
	extern REL::Relocation<uint32_t*> FPZPrePassDrawDataCount;
	extern REL::Relocation<uint32_t*> FPAlphaTestZPrePassDrawDataCount;
}
