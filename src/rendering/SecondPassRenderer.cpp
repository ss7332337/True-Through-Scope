#include "SecondPassRenderer.h"
#include "GlobalTypes.h"
#include "HookManager.h"

namespace ThroughScope
{
    SecondPassRenderer::SecondPassRenderer(ID3D11DeviceContext* context, ID3D11Device* device, D3DHooks* d3dHooks)
        : m_context(context)
        , m_device(device)
        , m_d3dHooks(d3dHooks)
        , m_lightBackup(LightBackupSystem::GetSingleton())
        , m_renderStateMgr(RenderStateManager::GetSingleton())
    {
        if (!ValidateD3DResources()) {
            logger::error("SecondPassRenderer: Invalid D3D resources provided");
        }
    }

    SecondPassRenderer::~SecondPassRenderer()
    {
        CleanupResources();
    }

    bool SecondPassRenderer::ExecuteSecondPass()
    {
        if (!CanExecuteSecondPass()) {
            logger::warn("Cannot execute second pass: preconditions not met");
            return false;
        }

        logger::debug("Starting second pass rendering...");

        // 初始化相机指针
        m_scopeCamera = ScopeCamera::GetScopeCamera();
        m_playerCamera = *ptr_DrawWorldCamera;

        // 重置状态标志
        m_texturesBackedUp = false;
        m_cameraUpdated = false;
        m_lightingSynced = false;
        m_renderExecuted = false;

        try {
            // 1. 备份第一次渲染的纹理
            if (!BackupFirstPassTextures()) {
                logger::error("Failed to backup first pass textures");
                return false;
            }

            // 2. 更新瞄具相机配置
            if (!UpdateScopeCamera()) {
                logger::error("Failed to update scope camera");
                RestoreFirstPass();
                return false;
            }

            // 3. 清理渲染目标
            ClearRenderTargets();

            // 4. 同步光照状态
            if (!SyncLighting()) {
                logger::error("Failed to sync lighting");
                RestoreFirstPass();
                return false;
            }

            // 5. 执行第二次渲染
            DrawScopeContent();
            m_renderExecuted = true;

            // 6. 恢复第一次渲染状态
            RestoreFirstPass();

            logger::debug("Second pass rendering completed successfully");
            return true;

        } catch (const std::exception& e) {
            logger::error("Exception during second pass rendering: {}", e.what());
            RestoreFirstPass();
            return false;
        } catch (...) {
            logger::error("Unknown exception during second pass rendering");
            RestoreFirstPass();
            return false;
        }
    }

    bool SecondPassRenderer::CanExecuteSecondPass() const
    {
        // 检查渲染状态
        if (!m_renderStateMgr->IsScopeReady() || !m_renderStateMgr->IsRenderReady() || !D3DHooks::IsEnableRender()) {
            return false;
        }

        // 检查瞄具相机
        auto scopeCamera = ScopeCamera::GetScopeCamera();
        if (!scopeCamera || !scopeCamera->parent) {
            return false;
        }

        // 检查玩家相机
        auto playerCamera = *ptr_DrawWorldCamera;
        if (!playerCamera) {
            return false;
        }

        // 检查D3D资源
        if (!ValidateD3DResources()) {
            return false;
        }

        return true;
    }

    bool SecondPassRenderer::BackupFirstPassTextures()
    {
        logger::debug("Backing up first pass textures...");

        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        if (!rendererData || !rendererData->context) {
            m_lastError = "Renderer data or context is null";
            return false;
        }

        // 获取主要的渲染目标
        m_mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
        m_mainDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
        m_mainRTTexture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
        m_mainDSTexture = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;

        // 获取当前绑定的渲染目标
        m_context->OMGetRenderTargets(2, m_savedRTVs, nullptr);

        // 获取后缓冲纹理
        ID3D11Resource* rtResource = nullptr;
        if (m_savedRTVs[1]) {
            m_savedRTVs[1]->GetResource(&rtResource);
            if (rtResource != nullptr) {
                rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&m_rtTexture2D);
                rtResource->Release();
            }
        }

        if (!m_rtTexture2D) {
            m_lastError = "Failed to get render target texture";
            return false;
        }

        // 创建临时后缓冲纹理
        if (!CreateTemporaryBackBuffer()) {
            return false;
        }

        // 复制当前渲染目标内容到临时BackBuffer
        m_context->CopyResource(m_tempBackBufferTex, m_rtTexture2D);

        // 复制主渲染目标到我们的纹理
        if (m_mainRTTexture && m_mainDSTexture) {
            m_context->CopyResource(RenderUtilities::GetFirstPassColorTexture(), m_mainRTTexture);
            m_context->CopyResource(RenderUtilities::GetFirstPassDepthTexture(), m_mainDSTexture);
            RenderUtilities::SetFirstPassComplete(true);
        } else {
            m_lastError = "Failed to find valid render target textures";
            return false;
        }

        m_texturesBackedUp = true;
        logger::debug("First pass textures backed up successfully");
        return true;
    }

    bool SecondPassRenderer::UpdateScopeCamera()
    {
        logger::debug("Updating scope camera...");

        // 创建原始相机的克隆
        RE::NiCloningProcess tempP{};
        m_originalCamera = (RE::NiCamera*)(m_playerCamera->CreateClone(tempP));
        if (!m_originalCamera) {
            m_lastError = "Failed to clone original camera";
            return false;
        }

        // 更新瞄具相机的位置和旋转
        m_scopeCamera->local.translate = m_originalCamera->local.translate;
        m_scopeCamera->local.rotate = m_originalCamera->local.rotate;
        m_scopeCamera->local.translate.y += 15;

        // 更新父节点
        if (m_scopeCamera->parent) {
            RE::NiUpdateData parentUpdate{};
            parentUpdate.camera = m_scopeCamera;
            m_scopeCamera->parent->Update(parentUpdate);
        }

        // 更新世界变换
        m_scopeCamera->world.translate = m_originalCamera->world.translate;
        m_scopeCamera->world.rotate = m_originalCamera->world.rotate;
        m_scopeCamera->world.scale = m_originalCamera->world.scale;
        m_scopeCamera->world.translate.y += 15;

        m_scopeCamera->UpdateWorldBound();

        // 配置瞄具的视锥体
        ConfigureScopeFrustum(m_scopeCamera, m_originalCamera);

        // 更新相机
        RE::NiUpdateData updateData{};
        updateData.camera = m_scopeCamera;
        m_scopeCamera->Update(updateData);
        m_scopeCamera->UpdateWorldData(&updateData);
        m_scopeCamera->UpdateWorldBound();

        m_cameraUpdated = true;
        logger::debug("Scope camera updated successfully");
        return true;
    }

    void SecondPassRenderer::ClearRenderTargets()
    {
        logger::debug("Clearing render targets...");

        // 清理主输出，准备第二次渲染
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        auto RTVs = rendererData->renderTargets;

        // 清理主要的渲染目标
        for (size_t i = 0; i < 4; i++) {
            if (RTVs[i].rtView) {
                m_context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
            }
        }

        // 清理深度模板缓冲区
        m_context->ClearDepthStencilView(m_mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // 清理延迟渲染的G-Buffer以确保光照信息不会残留
        static const int lightingBuffers[] = { 8, 9, 10, 11, 12, 13, 14, 15 };
        for (int bufIdx : lightingBuffers) {
            if (bufIdx < 100 && rendererData->renderTargets[bufIdx].rtView) {
                float clearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                // 对于法线缓冲区，使用默认法线值
                if (bufIdx == 8) {
                    clearValue[0] = 0.5f;
                    clearValue[1] = 0.5f;
                    clearValue[2] = 1.0f;
                    clearValue[3] = 1.0f;
                }
                m_context->ClearRenderTargetView((ID3D11RenderTargetView*)rendererData->renderTargets[bufIdx].rtView, clearValue);
            }
        }

        logger::debug("Render targets cleared");
    }

    bool SecondPassRenderer::SyncLighting()
    {
        logger::debug("Syncing lighting for scope rendering...");

        auto pShadowSceneNode = *ptr_DrawWorldShadowNode;
        if (!pShadowSceneNode) {
            m_lastError = "Shadow scene node is null";
            return false;
        }

        // 备份第一次渲染后的光源状态
        m_lightBackup->BackupLightStates(pShadowSceneNode);

        // 设置剔除进程
        if (*DrawWorldCullingProcess) {
            m_lightBackup->SetCullingProcess(*DrawWorldCullingProcess);
        }

        // 应用优化的光源状态用于第二次渲染
        m_lightBackup->ApplyLightStatesForScope();

        // 同步累积器的眼睛位置
        SyncAccumulatorEyePosition(m_scopeCamera);

        m_lightingSynced = true;
        logger::debug("Lighting synced successfully");
        return true;
    }

    void SecondPassRenderer::DrawScopeContent()
    {
        logger::debug("Drawing scope content...");

        // 设置性能标记
        D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");

        // 配置DrawWorld系统使用瞄具相机
        DrawWorld::SetCamera(m_scopeCamera);
        DrawWorld::SetUpdateCameraFOV(true);
        DrawWorld::SetAdjusted1stPersonFOV(ScopeCamera::GetTargetFOV());
        DrawWorld::SetCameraFov(ScopeCamera::GetTargetFOV());

        // 同步BSShaderManager的相机指针
        *ptr_BSShaderManagerSpCamera = m_scopeCamera;

        // 设置渲染标志
        ScopeCamera::SetRenderingForScope(true);

        // 备份原始相机指针
        auto SpCam = *ptr_DrawWorldSpCamera;
        auto DrawWorldCamera = *ptr_DrawWorldCamera;
        auto DrawWorldVisCamera = *ptr_DrawWorldVisCamera;

        // 设置瞄具相机为当前相机
        *ptr_DrawWorldCamera = m_scopeCamera;
        *ptr_DrawWorldVisCamera = m_scopeCamera;

        // 执行第二次渲染
        auto hookMgr = HookManager::GetSingleton();
        hookMgr->g_RenderPreUIOriginal(savedDrawWorld);

        // 恢复相机指针
        *ptr_DrawWorldCamera = DrawWorldCamera;
        *ptr_DrawWorldVisCamera = DrawWorldVisCamera;

        // 清除渲染标志
        ScopeCamera::SetRenderingForScope(false);

        D3DPERF_EndEvent();

        logger::debug("Scope content rendered");
    }

    void SecondPassRenderer::RestoreFirstPass()
    {
        logger::debug("Restoring first pass state...");

        if (m_lightingSynced) {
            // 恢复光源状态
            m_lightBackup->RestoreLightStates();

            // 恢复阴影节点状态
            auto pShadowSceneNode = *ptr_DrawWorldShadowNode;
            if (pShadowSceneNode) {
                pShadowSceneNode->bDisableLightUpdate = false;
            }
        }

        if (m_texturesBackedUp) {
            // 复制第二次渲染结果到我们的纹理
            m_context->CopyResource(RenderUtilities::GetSecondPassColorTexture(), m_mainRTTexture);
            m_context->CopyResource(m_rtTexture2D, m_tempBackBufferTex);
            RenderUtilities::SetSecondPassComplete(true);

            // 如果第二次渲染完成，恢复主渲染目标内容用于正常显示
            if (RenderUtilities::IsSecondPassComplete()) {
                m_context->CopyResource(m_mainRTTexture, RenderUtilities::GetFirstPassColorTexture());
                m_context->CopyResource(m_mainDSTexture, RenderUtilities::GetFirstPassDepthTexture());
            }

            // 恢复渲染目标
            if (m_savedRTVs[1]) {
                m_context->OMSetRenderTargets(1, &m_savedRTVs[1], nullptr);
            }

            // 渲染瞄具内容
            int scopeNodeIndexCount = ScopeCamera::GetScopeNodeIndexCount();
            if (scopeNodeIndexCount != -1) {
                try {
                    RenderUtilities::SetRender_PreUIComplete(true);
                    m_d3dHooks->SetScopeTexture(m_context);
                    D3DHooks::isSelfDrawCall = true;
                    m_context->DrawIndexed(scopeNodeIndexCount, 0, 0);
                    D3DHooks::isSelfDrawCall = false;
                    RenderUtilities::SetRender_PreUIComplete(false);
                } catch (...) {
                    logger::error("Exception during scope content rendering");
                    RenderUtilities::SetRender_PreUIComplete(false);
                }
            }
        }

        if (m_cameraUpdated) {
            // 恢复原始相机
            DrawWorld::SetCamera(m_originalCamera);
            DrawWorld::SetUpdateCameraFOV(true);

            RE::NiUpdateData nData;
            nData.camera = m_originalCamera;
            m_originalCamera->Update(nData);
        }

        logger::debug("First pass state restored");
    }

    void SecondPassRenderer::CleanupResources()
    {
        SAFE_RELEASE(m_tempBackBufferTex);
        SAFE_RELEASE(m_tempBackBufferSRV);
        SAFE_RELEASE(m_rtTexture2D);
        SAFE_RELEASE(m_savedRTVs[0]);
        SAFE_RELEASE(m_savedRTVs[1]);

        // 清理Camera克隆
        if (m_originalCamera) {
            if (m_originalCamera->DecRefCount() == 0) {
                m_originalCamera->DeleteThis();
            }
            m_originalCamera = nullptr;
        }

        // 重置状态标志
        m_texturesBackedUp = false;
        m_cameraUpdated = false;
        m_lightingSynced = false;
        m_renderExecuted = false;
    }

    bool SecondPassRenderer::ValidateD3DResources() const
    {
        if (!m_context || !m_device || !m_d3dHooks) {
            m_lastError = "D3D resources are null";
            return false;
        }

        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        if (!rendererData || !rendererData->context) {
            m_lastError = "Renderer data or context is null";
            return false;
        }

        return true;
    }

    bool SecondPassRenderer::CreateTemporaryBackBuffer()
    {
        if (!m_rtTexture2D) {
            m_lastError = "No render target texture available";
            return false;
        }

        D3D11_TEXTURE2D_DESC originalDesc;
        m_rtTexture2D->GetDesc(&originalDesc);

        D3D11_TEXTURE2D_DESC rtTextureDesc = originalDesc;
        rtTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&rtTextureDesc, nullptr, &m_tempBackBufferTex);
        if (FAILED(hr)) {
            m_lastError = "Failed to create temporary BackBuffer texture";
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_tempBackBufferTex, NULL, &m_tempBackBufferSRV);
        if (FAILED(hr)) {
            m_lastError = "Failed to create temporary BackBuffer SRV";
            SAFE_RELEASE(m_tempBackBufferTex);
            return false;
        }

        return true;
    }

    void SecondPassRenderer::ConfigureScopeFrustum(RE::NiCamera* scopeCamera, RE::NiCamera* originalCamera)
    {
        if (!scopeCamera || !originalCamera) {
            return;
        }

        // 使用原始相机的视锥体作为基础
        scopeCamera->viewFrustum = originalCamera->viewFrustum;

        // 调整视锥体参数以适应瞄具的FOV
        float aspectRatio = scopeCamera->viewFrustum.right / scopeCamera->viewFrustum.top;
        float targetFOV = ScopeCamera::GetTargetFOV();

        // 限制FOV范围以避免极端俯仰角下的数值不稳定
        float pitch = asin(-scopeCamera->world.rotate.entry[2][1]);
        float pitchDeg = pitch * 57.295779513f;
        if (abs(pitchDeg) > 70.0f) {
            float factor = (90.0f - abs(pitchDeg)) / 20.0f;
            factor = std::max(0.3f, std::min(1.0f, factor));
            targetFOV = targetFOV * factor;
        }

        float fovRad = targetFOV * 0.01745329251f;
        float halfFovTan = tan(fovRad * 0.5f);

        // 根据FOV重新计算视锥体边界
        scopeCamera->viewFrustum.top = scopeCamera->viewFrustum.nearPlane * halfFovTan;
        scopeCamera->viewFrustum.bottom = -scopeCamera->viewFrustum.top;
        scopeCamera->viewFrustum.right = scopeCamera->viewFrustum.top * aspectRatio;
        scopeCamera->viewFrustum.left = -scopeCamera->viewFrustum.right;

        // 保持远裁剪面距离
        scopeCamera->viewFrustum.farPlane = originalCamera->viewFrustum.farPlane;
    }

    void SecondPassRenderer::SyncAccumulatorEyePosition(RE::NiCamera* scopeCamera)
    {
        if (!scopeCamera) {
            return;
        }

        auto pDrawWorldAccum = *ptr_DrawWorldAccum;
        auto p1stPersonAccum = *ptr_Draw1stPersonAccum;
        auto pShadowSceneNode = *ptr_DrawWorldShadowNode;

        RE::NiPoint3A scopeEyePos = RE::NiPoint3A(scopeCamera->world.translate.x, scopeCamera->world.translate.y, scopeCamera->world.translate.z);

        if (pDrawWorldAccum) {
            pDrawWorldAccum->kEyePosition = scopeEyePos;
            pDrawWorldAccum->ClearActivePasses(false);
            pDrawWorldAccum->m_pkCamera = scopeCamera;
        }

        if (p1stPersonAccum) {
            p1stPersonAccum->kEyePosition = scopeEyePos;
        }

        if (pShadowSceneNode) {
            pShadowSceneNode->kEyePosition = scopeEyePos;
            pShadowSceneNode->bDisableLightUpdate = false;
            pShadowSceneNode->fStoredFarClip = scopeCamera->viewFrustum.farPlane;
            pShadowSceneNode->bAlwaysUpdateLights = true;
        }

        // 配置几何剔除进程
        auto geomListCullProc0 = *DrawWorldGeomListCullProc0;
        auto geomListCullProc1 = *DrawWorldGeomListCullProc1;

        if (geomListCullProc0) {
            geomListCullProc0->m_pkCamera = scopeCamera;
            geomListCullProc0->SetFrustum(&scopeCamera->viewFrustum);
        }

        if (geomListCullProc1) {
            geomListCullProc1->m_pkCamera = scopeCamera;
            geomListCullProc1->SetFrustum(&scopeCamera->viewFrustum);
        }
    }
}
