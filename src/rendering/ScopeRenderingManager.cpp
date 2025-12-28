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
        static bool s_isExecutingSceneRendering = false;
        if (s_isExecutingSceneRendering || m_sceneRenderingCompleteThisFrame) {
            return m_sceneRenderingCompleteThisFrame;
        }
        
        auto renderStateMgr = RenderStateManager::GetSingleton();
        if (!renderStateMgr->IsScopeReady() || !renderStateMgr->IsRenderReady() || !D3DHooks::IsEnableRender()) {
            return false;
        }

        if (!m_cachedContext || !m_cachedDevice || !m_cachedD3DHooks) {
            m_cachedD3DHooks = D3DHooks::GetSingleton();
            if (m_cachedD3DHooks) {
                m_cachedContext = m_cachedD3DHooks->GetContext();
                m_cachedDevice = m_cachedD3DHooks->GetDevice();
            }
        }
        
        if (!m_cachedContext || !m_cachedDevice) {
            return false;
        }

        s_isExecutingSceneRendering = true;
        m_currentRenderer = std::make_unique<SecondPassRenderer>(m_cachedContext, m_cachedDevice, m_cachedD3DHooks);
        
        D3DPERF_BeginEvent(0xFF00FF00, L"TrueThroughScope_ScenePhase_FO4TestCompat");
        bool success = m_currentRenderer->ExecuteSceneRendering();
        D3DPERF_EndEvent();
        
        s_isExecutingSceneRendering = false;
        
        if (success) {
            m_sceneRenderingCompleteThisFrame = true;
        } else {
            m_currentRenderer.reset();
        }
        
        return success;
    }

    bool ScopeRenderingManager::ExecutePostProcessingPhase()
    {
        if (!m_sceneRenderingCompleteThisFrame || !m_currentRenderer) {
            return false;
        }
        
        D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_PostPhase_FO4TestCompat");
        bool success = m_currentRenderer->ExecutePostProcessing();
        D3DPERF_EndEvent();
        
        m_currentRenderer.reset();
        
        return success;
    }
}
