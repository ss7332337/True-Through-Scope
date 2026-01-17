#pragma once

#include "Constants.h"
#include <RE/NetImmerse/NiCamera.hpp>

#include "DataPersistence.h"

namespace ThroughScope
{
    class ScopeCamera
    {
	private:
        // Static member declarations (must be before inline methods that use them)
        static RE::NiCamera* s_ScopeCamera;
        static RE::NiCamera* s_OriginalCamera;
        static float s_TargetFOV;
		static float s_MinMagnification;
		static float s_MaxMagnification;
        static bool s_OriginalFirstPerson;
        static bool s_OriginalRenderDecals;
        static bool s_IsRenderingForScope;
		static bool s_IsUserAdjustingZoomData;

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
			// 使用倍率限制来约束FOV
			float baseFOV = GetBaseFOV();
			float minFOV = baseFOV / s_MaxMagnification;  // 最大倍率 = 最小FOV
			float maxFOV = baseFOV / s_MinMagnification;  // 最小倍率 = 最大FOV
			s_TargetFOV = std::clamp(fov, minFOV, maxFOV);
		}

		// 倍率系统
		static float GetBaseFOV();  // 获取玩家的场景 FOV (worldFOV)
		static void SetMagnificationRange(float minMag, float maxMag)
		{
			s_MinMagnification = std::max(1.0f, minMag);  // 最小倍率不能低于1x
			s_MaxMagnification = std::max(s_MinMagnification, maxMag);
			// 实时更新当前FOV以符合新范围
			SetTargetFOV(s_TargetFOV);
		}
		static float CalculateFOVFromMagnification(float magnification)
		{
			return GetBaseFOV() / std::max(1.0f, magnification);
		}
		static float GetCurrentMagnification()
		{
			float baseFOV = GetBaseFOV();
			if (s_TargetFOV > 0.1f) {
				return baseFOV / s_TargetFOV;
			}
			return 1.0f;
		}
		static void SetTargetMagnification(float mag)
		{
			float clampedMag = std::clamp(mag, s_MinMagnification, s_MaxMagnification);
			s_TargetFOV = CalculateFOVFromMagnification(clampedMag);
		}
		static float GetMinMagnification() { return s_MinMagnification; }
		static float GetMaxMagnification() { return s_MaxMagnification; }
        
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
