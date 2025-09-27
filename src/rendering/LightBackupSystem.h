#pragma once

#include "PCH.h"
#include "GlobalTypes.h"

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
         */
        void ApplyLightStatesForScope();
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

        // ========== 内部数据 ==========

        /// 光源状态备份列表
        std::vector<LightStateBackup> m_lightBackups;

        /// 剔除进程指针，用于光源剔除
        RE::BSCullingProcess* m_cullingProcess = nullptr;

        // ========== 统计信息 ==========
        mutable size_t m_backupCount = 0;
        mutable size_t m_applyCount = 0;
        mutable size_t m_restoreCount = 0;
    };
}
