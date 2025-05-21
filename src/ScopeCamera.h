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
        
        // Scope camera adjustment
        static void ProcessCameraAdjustment();
        static bool IsAdjustmentMode() { return s_AdjustmentMode; }
        static void ToggleAdjustmentMode() { s_AdjustmentMode = !s_AdjustmentMode; }
        
        // Get target FOV
        static float GetTargetFOV() { return s_TargetFOV; }
        static void SetTargetFOV(float fov) { s_TargetFOV = fov; }
        
        // Flag indicating if we're in scope render mode
        static bool IsRenderingForScope() { return s_IsRenderingForScope; }
        static void SetRenderingForScope(bool value) { s_IsRenderingForScope = value; }
        
        // Cached offsets
        static RE::NiPoint3& GetDeltaPosition() { return s_DeltaPos; }
        static RE::NiMatrix3& GetDeltaRotation() { return s_DeltaRot; }
        
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
        
        // State flags
        static bool s_OriginalFirstPerson;
        static bool s_OriginalRenderDecals;
        static bool s_IsRenderingForScope;

        // Adjustment helper methods
        static void AdjustPosition(float x, float y, float z);
        static void AdjustPositionFTSNode(float x, float y, float z);
        static void AdjustRotation(float x, float y, float z);
		static void AdjustRotationFTSNode(float x, float y, float z);
        static void PrintCurrentValues();
        static void ResetCamera();
    };
}
