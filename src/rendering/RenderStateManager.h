#pragma once

#include "PCH.h"

namespace ThroughScope
{
    /**
     * @brief 管理渲染状态的单例类
     *
     * 负责管理全局渲染标志、Frustum设置、ViewPort配置等渲染相关的状态信息
     * 将原本分散在main.cpp中的全局变量集中管理，提供统一的访问接口
     */

	using namespace RE;
    class RenderStateManager
    {
    public:
        static RenderStateManager* GetSingleton();
        void Initialize();
        void Reset();

        // ========== 渲染状态查询接口 ==========
        bool IsRenderReady() const { return m_isRenderReady; }
        bool IsScopeReady() const { return m_isScopCamReady; }
        bool IsFirstCopy() const { return m_isFirstCopy; }
        bool IsImGuiManagerInit() const { return m_isImguiManagerInit; }
        bool IsFirstSpawnNode() const { return m_isFirstSpawnNode; }
        bool IsEnableTAA() const { return m_isEnableTAA; }
        bool IsScopeOnlyRender() const { return m_isScopeOnlyRender; }
        bool IsFrustumBackedUp() const { return m_frustumBackedUp; }

        // ========== 渲染状态设置接口 ==========

        void SetRenderReady(bool ready) { m_isRenderReady = ready; }
        void SetScopeReady(bool ready) { m_isScopCamReady = ready; }
        void SetFirstCopy(bool first) { m_isFirstCopy = first; }
        void SetImGuiManagerInit(bool init) { m_isImguiManagerInit = init; }
        void SetFirstSpawnNode(bool first) { m_isFirstSpawnNode = first; }
        void SetEnableTAA(bool enable) { m_isEnableTAA = enable; }
        void SetScopeOnlyRender(bool scopeOnly) { m_isScopeOnlyRender = scopeOnly; }
        void SetFrustumBackedUp(bool backed) { m_frustumBackedUp = backed; }

        // ========== Frustum和ViewPort管理 ==========
        NiFrustum& GetOriginalCamera1stViewFrustum() { return m_originalCamera1stviewFrustum; }
        const NiFrustum& GetOriginalCamera1stViewFrustum() const { return m_originalCamera1stviewFrustum; }

        NiFrustum& GetOriginalFrustum() { return m_originalFrustum; }
        const NiFrustum& GetOriginalFrustum() const { return m_originalFrustum; }

        NiFrustum& GetScopeFrustum() { return m_scopeFrustum; }
        const NiFrustum& GetScopeFrustum() const { return m_scopeFrustum; }

        NiFrustum& GetBackupFrustum() { return m_backupFrustum; }
        const NiFrustum& GetBackupFrustum() const { return m_backupFrustum; }

        NiRect<float>& GetScopeViewPort() { return m_scopeViewPort; }
        const NiRect<float>& GetScopeViewPort() const { return m_scopeViewPort; }

        // ========== 相机指针管理 ==========
        NiCamera* GetWorldFirstCam() const { return m_worldFirstCam; }
        void SetWorldFirstCam(NiCamera* camera) { m_worldFirstCam = camera; }

        // ========== 时间相关 ==========
        std::chrono::steady_clock::time_point GetDelayStartTime() const { return m_delayStartTime; }
        void SetDelayStartTime(const std::chrono::steady_clock::time_point& time) { m_delayStartTime = time; }

    private:
        RenderStateManager() = default;
        ~RenderStateManager() = default;
        RenderStateManager(const RenderStateManager&) = delete;
        RenderStateManager& operator=(const RenderStateManager&) = delete;

        // ========== 渲染状态标志 ==========
        bool m_isRenderReady = false;
        bool m_isScopCamReady = false;
        bool m_isFirstCopy = false;
        bool m_isImguiManagerInit = false;
        bool m_isFirstSpawnNode = false;
        bool m_isEnableTAA = false;
        bool m_isScopeOnlyRender = false;
        bool m_frustumBackedUp = false;

        // ========== Frustum和ViewPort配置 ==========
        NiFrustum m_originalCamera1stviewFrustum{};
        NiFrustum m_originalFrustum{};
        NiFrustum m_scopeFrustum{};
        NiFrustum m_backupFrustum{};
        NiRect<float> m_scopeViewPort{};

        // ========== 相机指针 ==========
        NiCamera* m_worldFirstCam = nullptr;

        // ========== 时间相关 ==========
        std::chrono::steady_clock::time_point m_delayStartTime;
    };
}
