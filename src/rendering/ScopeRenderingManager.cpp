#include "ScopeRenderingManager.h"
#include "RenderStateManager.h"
#include "GlobalTypes.h"

namespace ThroughScope
{
    void ScopeRenderingManager::SetCurrentFrameResources(ID3D11DeviceContext* context, ID3D11Device* device, D3DHooks* d3dHooks)
    {
        m_cachedContext = context;
        m_cachedDevice = device;
        m_cachedD3DHooks = d3dHooks;
    }

    bool ScopeRenderingManager::ExecuteSceneRenderingPhase()
    {
        // 防止重入 - DrawScopeContent 会调用 RenderPreUI 触发 Forward
        static bool s_isExecutingSceneRendering = false;
        if (s_isExecutingSceneRendering) {
            return false;  // 已经在执行中，跳过
        }
        
        // 如果本帧已经完成场景渲染，也跳过
        if (m_sceneRenderingCompleteThisFrame) {
            return true;  // 返回 true 因为之前已经成功了
        }
        
        // 检查渲染状态
        auto renderStateMgr = RenderStateManager::GetSingleton();
        if (!renderStateMgr->IsScopeReady() || !renderStateMgr->IsRenderReady() || !D3DHooks::IsEnableRender()) {
            return false;
        }

        // 确保有有效的 D3D 资源
        if (!m_cachedContext || !m_cachedDevice || !m_cachedD3DHooks) {
            // 尝试从 D3DHooks 获取
            m_cachedD3DHooks = D3DHooks::GetSington();
            if (m_cachedD3DHooks) {
                m_cachedContext = m_cachedD3DHooks->GetContext();
                m_cachedDevice = m_cachedD3DHooks->GetDevice();
            }
        }
        
        if (!m_cachedContext || !m_cachedDevice) {
            logger::error("[ScopeRenderingManager] No valid D3D resources");
            return false;
        }

        // 设置重入保护
        s_isExecutingSceneRendering = true;

        // 创建新的渲染器实例
        m_currentRenderer = std::make_unique<SecondPassRenderer>(m_cachedContext, m_cachedDevice, m_cachedD3DHooks);
        
        // 执行场景渲染
        D3DPERF_BeginEvent(0xFF00FF00, L"TrueThroughScope_ScenePhase_FO4TestCompat");
        bool success = m_currentRenderer->ExecuteSceneRendering();
        D3DPERF_EndEvent();
        
        // 清除重入保护
        s_isExecutingSceneRendering = false;
        
        if (success) {
            m_sceneRenderingCompleteThisFrame = true;
            logger::debug("[ScopeRenderingManager] Scene rendering phase completed");
        } else {
            logger::warn("[ScopeRenderingManager] Scene rendering phase failed");
            m_currentRenderer.reset();
        }
        
        return success;
    }

    bool ScopeRenderingManager::ExecutePostProcessingPhase()
    {
        // 检查场景渲染是否完成
        if (!m_sceneRenderingCompleteThisFrame || !m_currentRenderer) {
            logger::warn("[ScopeRenderingManager] Post processing called but scene not rendered this frame");
            return false;
        }
        
        // 执行后处理
        D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_PostPhase_FO4TestCompat");
        bool success = m_currentRenderer->ExecutePostProcessing();
        D3DPERF_EndEvent();
        
        if (success) {
            logger::debug("[ScopeRenderingManager] Post processing phase completed");
        } else {
            logger::warn("[ScopeRenderingManager] Post processing phase failed");
        }
        
        // 清理当前帧的渲染器
        m_currentRenderer.reset();
        
        return success;
    }
}
