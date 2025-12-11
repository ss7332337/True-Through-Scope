#pragma once

#include "PCH.h"
#include "GlobalTypes.h"
#include "RE/Bethesda/BSShaderManager.hpp"
#include "RE/Bethesda/Sky.hpp"
#include "RE/Bethesda/ImageSpaceManager.hpp"

namespace ThroughScope
{
    /**
     * @brief 光照状态备份系统
     *
     * 负责在第二次渲染过程中备份、应用和恢复光源状态
     * 确保瞄具渲染时光照计算的正确性，同时不影响主渲染的光源状态
     */
    class LightBackupSystem
    {
    public:
        static LightBackupSystem* GetSingleton();

        /**
         * @brief 备份第一次渲染后的光源状态
         *
         * 在第一次渲染完成后调用，保存所有光源的当前状态
         * 这些状态是第一次渲染后的有效状态，用于第二次渲染
         *
         */
        void BackupLightStates(RE::ShadowSceneNode* shadowNode);

        /**
         * @brief 应用优化的光源状态用于第二次渲染
         *
         * 在第二次渲染之前调用，将第一次渲染的光源状态应用到当前光源
         * 同时进行一些优化设置以确保瞄具渲染的质量
         *
         * @param limitCount 是否限制光源数量
         * @param maxLights 最大光源数量（默认8个）
         */
        void ApplyLightStatesForScope(bool limitCount = false, size_t maxLights = 8);
        void RestoreLightStates();
        void Clear();

        bool HasBackupStates() const;
        size_t GetBackupCount() const;
        void SetCullingProcess(RE::BSCullingProcess* cullingProcess);
    private:
        LightBackupSystem() = default;
        ~LightBackupSystem() = default;
        LightBackupSystem(const LightBackupSystem&) = delete;
        LightBackupSystem& operator=(const LightBackupSystem&) = delete;
        bool IsValidLight(RE::BSLight* light) const;
        void ApplySingleLightState(const LightStateBackup& backup);
        void RestoreSingleLightState(const LightStateBackup& backup);
        float CalculateLightPriority(const LightStateBackup& backup, const RE::NiPoint3& cameraPos) const;

        // ========== 内部数据 ==========

        /// 光源状态备份列表
        std::vector<LightStateBackup> m_lightBackups;

        /// 剔除进程指针，用于光源剔除
        RE::BSCullingProcess* m_cullingProcess = nullptr;

        /// ShadowSceneNode 指针，用于恢复可见性计数器
        RE::ShadowSceneNode* m_shadowNode = nullptr;

        /// 光源可见性计数器备份（DeferredLightsImpl 依赖这些计数器决定是否渲染）
        std::uint32_t m_visibleNonShadowLights = 0;
        std::uint32_t m_visibleShadowLights = 0;
        std::uint32_t m_visibleAmbientLights = 0;

        // ========== 统计信息 ==========
        mutable size_t m_backupCount = 0;
        mutable size_t m_applyCount = 0;
        mutable size_t m_restoreCount = 0;
    };
}
