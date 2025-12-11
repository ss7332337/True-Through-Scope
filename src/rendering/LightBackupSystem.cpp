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

        // 备份可见性计数器（DeferredLightsImpl 依赖这些计数器决定是否渲染光源）
        m_visibleNonShadowLights = shadowNode->uiVisibleNonShadowLights;
        m_visibleShadowLights = shadowNode->uiVisibleShadowLights;
        m_visibleAmbientLights = shadowNode->uiVisibleAmbientLights;

        // 计算所有光源列表的总大小，预分配空间
        size_t totalLights = shadowNode->lLightList.size() + 
                            shadowNode->lShadowLightList.size() + 
                            shadowNode->lAmbientLightList.size();
        m_lightBackups.reserve(totalLights);

        logger::debug("Backing up lights from ShadowSceneNode: {} normal, {} shadow, {} ambient (visible: {}/{}/{})",
            shadowNode->lLightList.size(),
            shadowNode->lShadowLightList.size(),
            shadowNode->lAmbientLightList.size(),
            m_visibleNonShadowLights,
            m_visibleShadowLights,
            m_visibleAmbientLights);

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

        m_backupCount++;
        logger::debug("Successfully backed up {} light states (operation #{})",
            m_lightBackups.size(), m_backupCount);
    }


    void LightBackupSystem::ApplyLightStatesForScope(bool limitCount, size_t maxLights)
    {
        if (m_lightBackups.empty()) {
            logger::warn("ApplyLightStatesForScope: No backup states available");
            return;
        }

        logger::debug("Applying {} light states for scope rendering", m_lightBackups.size());

        // DeferredLightsImpl 依赖这些计数器来决定渲染多少个光源
        if (m_shadowNode) {
            m_shadowNode->uiVisibleNonShadowLights = m_visibleNonShadowLights;
            m_shadowNode->uiVisibleShadowLights = m_visibleShadowLights;
            m_shadowNode->uiVisibleAmbientLights = m_visibleAmbientLights;
            logger::debug("Set visible light counts: NonShadow={}, Shadow={}, Ambient={}",
                m_visibleNonShadowLights, m_visibleShadowLights, m_visibleAmbientLights);
        }

        // 如果启用光源数量限制
        if (limitCount && m_lightBackups.size() > maxLights) {
            // 获取相机位置用于优先级计算
            auto scopeCamera = ThroughScope::ScopeCamera::GetScopeCamera();
            RE::NiPoint3 cameraPos(0, 0, 0);
            if (scopeCamera) {
                cameraPos = scopeCamera->world.translate;
            }

            // 创建光源优先级列表
            std::vector<std::pair<float, const LightStateBackup*>> lightPriorities;
            lightPriorities.reserve(m_lightBackups.size());

            for (const auto& backup : m_lightBackups) {
                float priority = CalculateLightPriority(backup, cameraPos);
                lightPriorities.push_back({ priority, &backup });
            }

            // 按优先级降序排序（高优先级在前）
            std::sort(lightPriorities.begin(), lightPriorities.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });

            // 只应用前N个最重要的光源
            size_t applyCount = std::min(maxLights, lightPriorities.size());
            logger::info("Limiting scope lights from {} to {}", m_lightBackups.size(), applyCount);

            for (size_t i = 0; i < applyCount; i++) {
                ApplySingleLightState(*lightPriorities[i].second);
            }
        } else {
            // 不限制光源数量，应用所有光源
            for (const auto& firstRenderState : m_lightBackups) {
                ApplySingleLightState(firstRenderState);
            }
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

    float LightBackupSystem::CalculateLightPriority(const LightStateBackup& backup, const RE::NiPoint3& cameraPos) const
    {
        auto bsLight = backup.light.get();
        if (!bsLight) {
            return 0.0f;
        }

        float priority = 0.0f;

        // 1. 距离因素（距离越近优先级越高）
        RE::NiPoint3 lightPos = bsLight->bPointPosition;
        float distance = (lightPos - cameraPos).Length();
        float distanceFactor = 1.0f / (1.0f + distance * 0.01f); // 归一化距离影响
        priority += distanceFactor * 100.0f;

        // 2. 光源强度因素（亮度越高优先级越高）
        if (bsLight->fLODDimmer > 0.0f) {
            priority += bsLight->fLODDimmer * 50.0f;
        }

        // 3. 动态光源优先级更高（通常是重要的游戏光源）
        if (bsLight->bDynamicLight) {
            priority += 30.0f;
        }

        // 4. 未被遮挡的光源优先级更高
        if (!backup.occluded) {
            priority += 20.0f;
        }

        // 5. 非临时光源优先级更高（场景主光源）
        if (!backup.temporary) {
            priority += 15.0f;
        }

        // 6. 视锥体内的光源优先级更高
        if (backup.frustumCull == 0xFF || backup.frustumCull == 0xFE) {
            priority += 25.0f;
        }

        return priority;
    }
}
