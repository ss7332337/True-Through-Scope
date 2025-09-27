#include "LightBackupSystem.h"

namespace ThroughScope
{
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

        // 清空之前的备份
        m_lightBackups.clear();

        // 预分配空间
        m_lightBackups.reserve(shadowNode->lLightList.size());

        logger::debug("Backing up {} lights from ShadowSceneNode", shadowNode->lLightList.size());

        // 保存所有光源的当前状态（第一次渲染后的状态）
        for (size_t i = 0; i < shadowNode->lLightList.size(); i++) {
            auto bsLight = shadowNode->lLightList[i];
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
        }

        m_backupCount++;
        logger::debug("Successfully backed up {} light states (operation #{})",
            m_lightBackups.size(), m_backupCount);
    }

    void LightBackupSystem::ApplyLightStatesForScope()
    {
        if (m_lightBackups.empty()) {
            logger::warn("ApplyLightStatesForScope: No backup states available");
            return;
        }

        logger::debug("Applying {} light states for scope rendering", m_lightBackups.size());

        // 在第二次渲染之前应用优化的光源状态
        for (const auto& firstRenderState : m_lightBackups) {
            ApplySingleLightState(firstRenderState);
        }

        m_applyCount++;
        logger::debug("Applied light states for scope rendering (operation #{})", m_applyCount);
    }

    void LightBackupSystem::RestoreLightStates()
    {
        if (m_lightBackups.empty()) {
            logger::warn("RestoreLightStates: No backup states available");
            return;
        }

        logger::debug("Restoring {} light states", m_lightBackups.size());

        // 恢复原始光源状态
        for (const auto& backup : m_lightBackups) {
            RestoreSingleLightState(backup);
        }

        m_restoreCount++;
        logger::debug("Restored light states (operation #{})", m_restoreCount);
    }

    void LightBackupSystem::Clear()
    {
        logger::debug("Clearing {} light backup states", m_lightBackups.size());
        m_lightBackups.clear();
        m_cullingProcess = nullptr;
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
        logger::debug("Set culling process: 0x{:X}",
            reinterpret_cast<uintptr_t>(cullingProcess));
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
            // 应用视锥体剔除状态
            if (backup.frustumCull == 0xFF || backup.frustumCull == 0xFE) {
                bsLight->usFrustumCull = backup.frustumCull;
            } else {
                bsLight->usFrustumCull = 0xFF;  // BSL_ALL - 确保光源在瞄具渲染中可见
            }

            // 应用基本状态
            bsLight->SetOccluded(backup.occluded);
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