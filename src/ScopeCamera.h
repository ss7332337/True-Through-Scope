#pragma once

#include "Constants.h"
#include <RE/NetImmerse/NiCamera.hpp>

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
        
        static bool IsAdjustmentMode() { return s_AdjustmentMode; }
        static void ToggleAdjustmentMode() { s_AdjustmentMode = !s_AdjustmentMode; }
        
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
        
    private:
        // Camera objects
        static RE::NiCamera* s_ScopeCamera;
        static RE::NiCamera* s_OriginalCamera;
        
        // Adjustment settings
        static bool s_AdjustmentMode;
        static float s_AdjustmentSpeed;
        static float s_TargetFOV;
        static AdjustmentTarget s_CurrentAdjustmentTarget;
        static int s_CurrentAdjustmentAxis;
        
        // Position/rotation deltas
        static RE::NiPoint3 s_DeltaPos;
        static RE::NiPoint3 s_CachedDeltaPos;
        static RE::NiMatrix3 s_DeltaRot;
        static RE::NiMatrix3 s_CachedDeltaRot;
		static RE::NiPoint3 s_DeltaScale;
		static RE::NiPoint3 s_CachedDeltaScale;
		static float minFov;
		static float maxFov;
        
        // State flags
        static bool s_OriginalFirstPerson;
        static bool s_OriginalRenderDecals;
        static bool s_IsRenderingForScope;

        static void ResetCamera();
    };
}
