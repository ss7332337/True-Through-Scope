#pragma once

#include "Constants.h"
#include <RE/NetImmerse/NiCamera.hpp>

#include "DataPersistence.h"

namespace ThroughScope
{
    class ScopeCamera
    {
    public:
        // Initialize scope camera system
        static bool Initialize();
        static void Shutdown();
        
        // Create and setup a scope camera instance
        static void CreateScopeCamera();
        
        // Camera getters/setters
        static RE::NiCamera* GetScopeCamera() { return s_ScopeCamera; }
        static void SetScopeCamera(RE::NiCamera* camera) { s_ScopeCamera = camera; }
        
		static void ApplyScopeTransform(RE::NiNode* scopeNode, const DataPersistence::CameraAdjustments& adjustments);
		static void ApplyScopeSettings(const DataPersistence::ScopeConfig* config);
		static void SetupScopeForWeapon(const DataPersistence::WeaponInfo& weaponInfo);
		static int GetScopeNodeIndexCount();
		static void CleanupScopeResources();
		static void RestoreZoomDataForCurrentWeapon();  // 恢复ZoomData的方法
        // Get target FOV
        static float GetTargetFOV() { return s_TargetFOV; }
		static void SetTargetFOV(float fov)
		{
			s_TargetFOV = std::clamp(fov, minFov, maxFov);
		}

		static void SetFOVMinMax(float _minFov, float _maxFov)
		{
			minFov = _minFov;
			maxFov = _maxFov;
		}
        
        // Flag indicating if we're in scope render mode
        static bool IsRenderingForScope() { return s_IsRenderingForScope; }
        static void SetRenderingForScope(bool value) { s_IsRenderingForScope = value; }

		static bool IsSideAim();
		static RE::BGSKeyword* IsMagnifier();

		static RE::TESFormID s_EquippedWeaponFormID;
		static RE::NiNode* s_CurrentScopeNode;

		static bool hasFirstSpawnNode;
		static bool isDelayStarted;
		static bool isFirstScopeRender;

		static int s_enableNightVision;

		// ZoomData管理
		static void SetZoomDataUserAdjusting(bool adjusting) { s_IsUserAdjustingZoomData = adjusting; }
		static bool IsUserAdjustingZoomData() { return s_IsUserAdjustingZoomData; }

    private:
        // Camera objects
        static RE::NiCamera* s_ScopeCamera;
        static RE::NiCamera* s_OriginalCamera;
        
        // Adjustment settings
        static float s_TargetFOV;
		static float minFov;
		static float maxFov;
        
        // State flags
        static bool s_OriginalFirstPerson;
        static bool s_OriginalRenderDecals;
        static bool s_IsRenderingForScope;
		static bool s_IsUserAdjustingZoomData;  // 标记用户是否正在调整ZoomData

		
		static RE::BGSKeyword* an_45;
		static RE::BGSKeyword* AnimsXM2010_scopeKH45;
		static RE::BGSKeyword* AnimsXM2010_scopeKM;
		static RE::BGSKeyword* AX50_toounScope_K;
		static RE::BGSKeyword* AnimsAX50_scopeKH45;
		static RE::BGSKeyword* QMW_AnimsQBZ191M_on;
		static RE::BGSKeyword* QMW_AnimsQBZ191M_off;
		static RE::BGSKeyword* QMW_AnimsRU556M_on;
		static RE::BGSKeyword* QMW_AnimsRU556M_off;
		static RE::BGSKeyword* AX50_toounScope_L;
		static RE::BGSKeyword* AnimsAX50_scopeK;


        static void ResetCamera();
    };
}
