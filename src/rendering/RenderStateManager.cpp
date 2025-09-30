#include "RenderStateManager.h"
#include "D3DHooks.h"
#include <GlobalTypes.h>

namespace ThroughScope
{
    RenderStateManager* RenderStateManager::GetSingleton()
    {
        static RenderStateManager instance;
        return &instance;
    }

    void RenderStateManager::Initialize()
    {
        logger::info("Initializing RenderStateManager...");

        // 重置所有标志为初始状态
        Reset();

        // 设置原始第一人称相机视锥体的默认值
        m_originalCamera1stviewFrustum.bottom = -0.84f;
        m_originalCamera1stviewFrustum.top = 0.84f;
        m_originalCamera1stviewFrustum.left = 0.472f;
        m_originalCamera1stviewFrustum.right = -0.472f;
        m_originalCamera1stviewFrustum.nearPlane = 10.0f;
        m_originalCamera1stviewFrustum.farPlane = 10240.0f;

        // 设置瞄具视锥体的默认值
        float scopeViewSize = 0.125f;
        m_scopeFrustum.left = -scopeViewSize * 2;
        m_scopeFrustum.right = scopeViewSize * 2;
        m_scopeFrustum.top = scopeViewSize;
        m_scopeFrustum.bottom = -scopeViewSize;
        m_scopeFrustum.nearPlane = 15.0f;
        m_scopeFrustum.farPlane = 353840.0f;
        m_scopeFrustum.ortho = false;

        // 设置瞄具视口的默认值
        m_scopeViewPort.left = 0.4f;
        m_scopeViewPort.right = 0.6f;
        m_scopeViewPort.top = 0.6f;
        m_scopeViewPort.bottom = 0.4f;

        // 将原始视锥体设置为第一人称相机视锥体的副本
        m_originalFrustum = m_originalCamera1stviewFrustum;

        // 尝试获取第一人称相机指针
        try {
            if (*ptr_DrawWorld1stCamera) {
                m_worldFirstCam = *ptr_DrawWorld1stCamera;
                logger::info("World first camera initialized: 0x{:X}",
                    reinterpret_cast<uintptr_t>(m_worldFirstCam));
            } else {
                logger::warn("ptr_DrawWorld1stCamera is null during initialization");
                m_worldFirstCam = nullptr;
            }
        } catch (...) {
            logger::error("Exception occurred while initializing world first camera");
            m_worldFirstCam = nullptr;
        }

        logger::info("RenderStateManager initialized successfully");
        // logger::debug("Scope frustum - Near: {}, Far: {}, Size: {}",
        //     m_scopeFrustum.nearPlane, m_scopeFrustum.farPlane, scopeViewSize);
        // logger::debug("Scope viewport - Left: {}, Right: {}, Top: {}, Bottom: {}",
        //     m_scopeViewPort.left, m_scopeViewPort.right, m_scopeViewPort.top, m_scopeViewPort.bottom);
    }

    void RenderStateManager::Reset()
    {
        logger::info("Resetting RenderStateManager state...");

        // 重置所有布尔标志
        m_isRenderReady = false;
        m_isScopCamReady = false;
        m_isFirstCopy = false;
        m_isImguiManagerInit = false;
        m_isFirstSpawnNode = false;
        m_isEnableTAA = false;
        m_isScopeOnlyRender = false;
        m_frustumBackedUp = false;

        // 重置时间
        m_delayStartTime = std::chrono::steady_clock::time_point{};

        // 重置Frustum（保持默认构造的状态）
        m_originalCamera1stviewFrustum = NiFrustum{};
        m_originalFrustum = NiFrustum{};
        m_scopeFrustum = NiFrustum{};
        m_backupFrustum = NiFrustum{};

        // 重置视口
        m_scopeViewPort = NiRect<float>{};

        // 重置相机指针
        m_worldFirstCam = nullptr;

        // logger::debug("RenderStateManager state reset completed");
    }
}
