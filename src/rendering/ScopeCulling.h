#pragma once

#include "RE/Bethesda/BSCullingProcess.hpp"
#include "RE/NetImmerse/NiFrustum.hpp"
#include "RE/NetImmerse/NiCamera.hpp"

namespace ThroughScope
{
    /**
     * @brief RAII guard for custom culling planes
     * 
     * Automatically sets up custom culling planes from scope camera on construction
     * and restores original culling state on destruction.
     * 
     * Usage:
     *   ScopedCustomCulling cullGuard(cullingProcess, scopeCamera);
     *   // ... rendering code ...
     *   // Destructor automatically restores original state
     */
    class ScopedCustomCulling
    {
    public:
        /**
         * @brief Construct and setup custom culling planes
         * @param cullingProcess The BSCullingProcess to modify
         * @param scopeCamera The scope camera to calculate frustum planes from
         */
        ScopedCustomCulling(RE::BSCullingProcess* cullingProcess, RE::NiCamera* scopeCamera);
        
        /**
         * @brief Destructor - restores original culling state
         */
        ~ScopedCustomCulling();

        // Disable copy
        ScopedCustomCulling(const ScopedCustomCulling&) = delete;
        ScopedCustomCulling& operator=(const ScopedCustomCulling&) = delete;

        /**
         * @brief Check if custom culling was successfully applied
         */
        bool IsValid() const { return m_valid; }

    private:
        RE::BSCullingProcess* m_cullingProcess;
        bool m_originalCustomCullPlanesFlag;
        RE::NiFrustumPlanes m_originalPlanes;
        bool m_valid;
    };

    /**
     * @brief Setup custom culling planes from scope camera
     * 
     * Calculates frustum planes from the scope camera and sets them as
     * custom culling planes in the culling process.
     * 
     * @param cullingProcess The BSCullingProcess to modify
     * @param scopeCamera The scope camera to calculate frustum planes from
     */
    void SetupScopeCullingPlanes(RE::BSCullingProcess* cullingProcess, RE::NiCamera* scopeCamera);

    /**
     * @brief Restore original culling state
     * 
     * @param cullingProcess The BSCullingProcess to restore
     * @param originalFlag The original bCustomCullPlanes flag value
     * @param originalPlanes The original kCustomCullPlanes value
     */
    void RestoreCullingPlanes(RE::BSCullingProcess* cullingProcess, 
                               bool originalFlag, 
                               const RE::NiFrustumPlanes& originalPlanes);

    // ========== Strategy C: Frustum Testing ==========

    /**
     * @brief Test if a bounding sphere is visible within the scope frustum
     * 
     * Used by hkBSCullingGroupAdd to filter objects during scope rendering.
     * Objects outside the scope frustum are rejected.
     * 
     * @param bound The bounding sphere to test
     * @param scopePlanes The scope frustum planes
     * @return true if the object is potentially visible, false if completely outside
     */
    bool TestBoundAgainstFrustum(const RE::NiBound* bound, const RE::NiFrustumPlanes& scopePlanes);

    /**
     * @brief Get cached scope frustum planes for the current frame
     * 
     * The planes are recalculated each frame when scope rendering begins.
     * 
     * @return Pointer to cached scope frustum planes, or nullptr if not valid
     */
    const RE::NiFrustumPlanes* GetCachedScopeFrustumPlanes();

    /**
     * @brief Update cached scope frustum planes from scope camera
     * 
     * Called at the start of scope rendering to cache planes for the frame.
     * 
     * @param scopeCamera The scope camera to calculate planes from
     */
    void UpdateCachedScopeFrustumPlanes(RE::NiCamera* scopeCamera);

    /**
     * @brief Invalidate cached scope frustum planes
     * 
     * Called at the end of scope rendering.
     */
    void InvalidateCachedScopeFrustumPlanes();

    // ========== Debug Stats ==========

    /**
     * @brief Increment culling counters
     */
    void IncrementCullingTested();
    void IncrementCullingPassed();
    void IncrementCullingFiltered();

    /**
     * @brief Get culling statistics and reset counters
     */
    /**
     * @brief Get culling statistics and reset counters
     */
    void GetAndResetCullingStats(uint32_t& tested, uint32_t& passed, uint32_t& filtered);

    /**
     * @brief Get culling statistics for the last frame (for UI display)
     */
    void GetLastFrameCullingStats(uint32_t& tested, uint32_t& passed, uint32_t& filtered);

    /**
     * @brief Set safety margin for culling (percentage of radius)
     * e.g. 0.05 = 5% extra margin
     */
    void SetCullingSafetyMargin(float margin);

    /**
     * @brief Get current safety margin
     */
    float GetCullingSafetyMargin();

    // ========== Shadow Caster Range ==========

    void SetShadowCasterRange(float range);
    float GetShadowCasterRange();
}
