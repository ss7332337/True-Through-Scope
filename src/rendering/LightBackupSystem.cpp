#include "LightBackupSystem.h"
#include <ScopeCamera.h>
#include "LightBackupSystem.h"
#include <ScopeCamera.h>

namespace ThroughScope
{
    // BSShaderManagerState 全局指针
    static REL::Relocation<RE::BSShaderManagerState*> ptr_BSShaderManagerState{ REL::ID(1327069) };
    LightBackupSystem* LightBackupSystem::GetSingleton()
    {
        static LightBackupSystem instance;
        return &instance;
    }

    void LightBackupSystem::BackupLightStates(RE::ShadowSceneNode* shadowNode)
    {
        if (!shadowNode) {
            logger::warn("BackupLightStates: shadowNode is null");
            return;
        }

        // 保存ShadowSceneNode指针
        m_shadowNode = shadowNode;

        // 清空之前的备份
        m_lightBackups.clear();

        // 备份可见性计数器
        m_visibleNonShadowLights = shadowNode->uiVisibleNonShadowLights;
        m_visibleShadowLights = shadowNode->uiVisibleShadowLights;
        m_visibleAmbientLights = shadowNode->uiVisibleAmbientLights;

        // 计算所有光源列表的总大小
        size_t totalLights = shadowNode->lLightList.size() + 
                            shadowNode->lShadowLightList.size() + 
                            shadowNode->lAmbientLightList.size();
        m_lightBackups.reserve(totalLights);

        m_lightBackups.reserve(totalLights);

        // Lambda函数用于备份单个光源
        auto backupLight = [this](const RE::NiPointer<RE::BSLight>& bsLight) {
            if (bsLight && bsLight.get() && IsValidLight(bsLight.get())) {
                LightStateBackup backup{};
                backup.light = bsLight;
                backup.frustumCull = bsLight->usFrustumCull;
                backup.occluded = bsLight->bOccluded;
                backup.temporary = bsLight->bTemporary;
                backup.dynamic = bsLight->bDynamicLight;
                backup.lodDimmer = bsLight->fLODDimmer;
                backup.camera = bsLight->spCamera;
                backup.cullingProcess = bsLight->pCullingProcess;
                m_lightBackups.push_back(backup);
            }
        };

        // 遍历所有光源列表并备份每个光源
        for (const auto& light : shadowNode->lLightList) {
            backupLight(light);
        }
        for (const auto& light : shadowNode->lShadowLightList) {
            backupLight(light);
        }
        for (const auto& light : shadowNode->lAmbientLightList) {
            backupLight(light);
        }

        // 备份环境光状态
        BackupAmbientLightStates();

        m_backupCount++;
    }


    void LightBackupSystem::ApplyLightStatesForScope(bool limitCount, size_t maxLights)
    {
        if (m_lightBackups.empty()) {
            logger::warn("ApplyLightStatesForScope: No backup states available");
            return;
        }
        // 首先应用环境光状态，确保瞄具渲染时使用正确的环境光
        ApplyAmbientLightStates();

        // DeferredLightsImpl 依赖这些计数器来决定渲染多少个光源
        if (m_shadowNode) {
            m_shadowNode->uiVisibleNonShadowLights = m_visibleNonShadowLights;
            m_shadowNode->uiVisibleShadowLights = m_visibleShadowLights;
            m_shadowNode->uiVisibleAmbientLights = m_visibleAmbientLights;
        }

        // 直接应用所有光源
        for (const auto& backup : m_lightBackups) {
            ApplySingleLightState(backup);
        }

        m_applyCount++;
    }

    void LightBackupSystem::RestoreLightStates()
    {
        if (m_lightBackups.empty()) {
            logger::warn("RestoreLightStates: No backup states available");
            return;
        }

        // 恢复环境光状态
        RestoreAmbientLightStates();

        // 恢复原始光源状态
        for (const auto& backup : m_lightBackups) {
            RestoreSingleLightState(backup);
        }

        m_restoreCount++;
    }

    void LightBackupSystem::Clear()
    {
        logger::debug("Clearing {} light backup states", m_lightBackups.size());
        m_lightBackups.clear();
        m_cullingProcess = nullptr;
        m_ambientBackup.isValid = false;
    }

    void LightBackupSystem::BackupAmbientLightStates()
    {
        // 获取 BSShaderManagerState
        auto pState = ptr_BSShaderManagerState.get();
        if (!pState) {
            logger::warn("BackupAmbientLightStates: BSShaderManagerState is null");
            m_ambientBackup.isValid = false;
            return;
        }

        // 备份 Transform 和 Specular 参数
        m_ambientBackup.DirectionalAmbientTransform = pState->DirectionalAmbientTransform;
        m_ambientBackup.LocalDirectionalAmbientTransform = pState->LocalDirectionalAmbientTransform;
        m_ambientBackup.AmbientSpecular = pState->AmbientSpecular;
        m_ambientBackup.bAmbientSpecularEnabled = pState->bAmbientSpecularEnabled;
        
        // 备份 BSShaderManager 的环境光颜色数组（这是实际影响渲染的数据！）
        auto directionalColors = RE::BSShaderManager::GetDirectionalAmbientColorsA();
        auto localColors = RE::BSShaderManager::GetLocalDirectionalAmbientColorsA();
        
        if (directionalColors) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    m_ambientBackup.DirectionalAmbientColorsA[i][j] = directionalColors[i][j];
                }
            }
        }
        
        if (localColors) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    m_ambientBackup.LocalDirectionalAmbientColorsA[i][j] = localColors[i][j];
                }
            }
            // logger::debug("Backed up LocalDirectionalAmbientColorsA: [{:.3f},{:.3f},{:.3f}] [{:.3f},{:.3f},{:.3f}] [{:.3f},{:.3f},{:.3f}]",
            //     localColors[0][0].r, localColors[0][0].g, localColors[0][0].b,
            //     localColors[1][0].r, localColors[1][0].g, localColors[1][0].b,
            //     localColors[2][0].r, localColors[2][0].g, localColors[2][0].b);
        }

        m_ambientBackup.isValid = true;
    }

    void LightBackupSystem::RestoreAmbientLightStates()
    {
        if (!m_ambientBackup.isValid) {
            return;
        }

        auto pState = ptr_BSShaderManagerState.get();
        if (pState) {
            pState->DirectionalAmbientTransform = m_ambientBackup.DirectionalAmbientTransform;
            pState->LocalDirectionalAmbientTransform = m_ambientBackup.LocalDirectionalAmbientTransform;
            pState->AmbientSpecular = m_ambientBackup.AmbientSpecular;
            pState->bAmbientSpecularEnabled = m_ambientBackup.bAmbientSpecularEnabled;
        }
        
        // 恢复 BSShaderManager 的环境光颜色数组
        auto directionalColors = RE::BSShaderManager::GetDirectionalAmbientColorsA();
        auto localColors = RE::BSShaderManager::GetLocalDirectionalAmbientColorsA();
        
        if (directionalColors) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    directionalColors[i][j] = m_ambientBackup.DirectionalAmbientColorsA[i][j];
                }
            }
        }
        
        if (localColors) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    localColors[i][j] = m_ambientBackup.LocalDirectionalAmbientColorsA[i][j];
                }
            }
        }
        
    }

    void LightBackupSystem::ApplyAmbientLightStates()
    {
        if (!m_ambientBackup.isValid) {
            return;
        }

        // 应用 BSShaderManagerState 中的环境光参数 (Transforms)
        auto pState = ptr_BSShaderManagerState.get();
        if (pState) {
            pState->DirectionalAmbientTransform = m_ambientBackup.DirectionalAmbientTransform;
            pState->LocalDirectionalAmbientTransform = m_ambientBackup.LocalDirectionalAmbientTransform;
            pState->AmbientSpecular = m_ambientBackup.AmbientSpecular;
            pState->bAmbientSpecularEnabled = m_ambientBackup.bAmbientSpecularEnabled;
        }

        // 应用 BSShaderManager 的环境光颜色数组（关键！这影响实际渲染的环境光颜色）
        auto directionalColors = RE::BSShaderManager::GetDirectionalAmbientColorsA();
        auto localColors = RE::BSShaderManager::GetLocalDirectionalAmbientColorsA();
        
        if (directionalColors) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    directionalColors[i][j] = m_ambientBackup.DirectionalAmbientColorsA[i][j];
                }
            }
        }
        
        if (localColors) {
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    localColors[i][j] = m_ambientBackup.LocalDirectionalAmbientColorsA[i][j];
                }
            }
        }
    }

    bool LightBackupSystem::HasBackupStates() const
    {
        return !m_lightBackups.empty();
    }

    size_t LightBackupSystem::GetBackupCount() const
    {
        return m_lightBackups.size();
    }

    void LightBackupSystem::SetCullingProcess(RE::BSCullingProcess* cullingProcess)
    {
        m_cullingProcess = cullingProcess;
    }

    bool LightBackupSystem::IsValidLight(RE::BSLight* light) const
    {
        if (!light) {
            return false;
        }

        // 检查光源的基本有效性
        try {
            // 尝试访问光源的基本属性来验证其有效性
            volatile auto frustumCull = light->usFrustumCull;
            volatile auto occluded = light->bOccluded;
            (void)frustumCull; // 避免未使用变量警告
            (void)occluded;
            return true;
        } catch (...) {
            logger::error("Invalid light detected: 0x{:X}", reinterpret_cast<uintptr_t>(light));
            return false;
        }
    }

    void LightBackupSystem::ApplySingleLightState(const LightStateBackup& backup)
    {
        auto bsLight = backup.light.get();
        if (!bsLight || !IsValidLight(bsLight)) {
            return;
        }

        try {
            // 应用视锥体剔除状态 - 强制所有光源可见
            // 重要：DeferredLightsImpl 会跳过 usFrustumCull == 255 的光源！
            // 255 表示被完全剔除，0 表示未剔除（可见）
            bsLight->usFrustumCull = 0;  // 设为0表示光源未被剔除，应该被渲染

            // 应用基本状态 - 强制不被遮挡
            bsLight->SetOccluded(false);
            bsLight->SetTemporary(backup.temporary);
            bsLight->SetLODFade(false);  // 暂不持久保存LODFade，保持关闭以提升可见性
            bsLight->fLODDimmer = backup.lodDimmer;

            // 应用动态光源状态
            if (bsLight->bDynamicLight != backup.dynamic) {
                bsLight->SetDynamic(backup.dynamic);
            }

            // 设置光源影响选项（优化瞄具渲染效果）
            bsLight->SetAffectLand(true);
            bsLight->SetAffectWater(true);
            bsLight->SetIgnoreRoughness(false);
            bsLight->SetIgnoreRim(false);
            bsLight->SetAttenuationOnly(false);

            // 应用相机和剔除进程
            bsLight->spCamera = backup.camera;

            if (backup.cullingProcess) {
                bsLight->SetCullingProcess(backup.cullingProcess);
            } else if (m_cullingProcess) {
                bsLight->SetCullingProcess(m_cullingProcess);
            }

        } catch (const std::exception& e) {
            logger::error("Exception while applying light state: {}", e.what());
        } catch (...) {
            logger::error("Unknown exception while applying light state for light: 0x{:X}",
                reinterpret_cast<uintptr_t>(bsLight));
        }
    }

    void LightBackupSystem::RestoreSingleLightState(const LightStateBackup& backup)
    {
        auto bsLight = backup.light.get();
        if (!bsLight || !IsValidLight(bsLight)) {
            return;
        }

        try {
            // 恢复原始状态
            bsLight->usFrustumCull = backup.frustumCull;
            bsLight->SetOccluded(backup.occluded);
            bsLight->SetTemporary(backup.temporary);
            bsLight->fLODDimmer = backup.lodDimmer;
            bsLight->spCamera = backup.camera;
            bsLight->SetCullingProcess(backup.cullingProcess);

            // 恢复动态光源状态
            if (bsLight->bDynamicLight != backup.dynamic) {
                bsLight->SetDynamic(backup.dynamic);
            }

        } catch (const std::exception& e) {
            logger::error("Exception while restoring light state: {}", e.what());
        } catch (...) {
            logger::error("Unknown exception while restoring light state for light: 0x{:X}",
                reinterpret_cast<uintptr_t>(bsLight));
        }
    }


}
