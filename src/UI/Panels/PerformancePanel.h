#pragma once

#include "../BasePanelInterface.h"
#include "../../RenderOptimization.h"

namespace ThroughScope
{
    /**
     * @brief 性能优化面板
     *
     * 提供图形化界面让玩家调整瞄具场景的渲染性能优化设置
     */
    class PerformancePanel : public BasePanelInterface
    {
    public:
        PerformancePanel(PanelManagerInterface* manager);
        ~PerformancePanel() override = default;

        // BasePanelInterface 实现
        void Render() override;
        void Update() override;
        const char* GetPanelName() const override { return "Performance"; }
        bool GetSaved() const override { return m_isSaved; }

    private:
        PanelManagerInterface* m_Manager;
        RenderOptimization* m_RenderOpt;

        bool m_isSaved = true;
        bool m_settingsChanged = false;

        // 当前设置（用于编辑）
        RenderOptimization::OptimizationSettings m_currentSettings;

        // UI状态
        int m_selectedQualityLevel = 2;  // 默认Medium
        bool m_showAdvancedSettings = false;

        // 渲染函数
        void RenderQualityPresets();
        void RenderLightingSettings();
        void RenderRenderStageSettings();
        void RenderAdvancedSettings();
        void RenderPerformanceStats();
        void RenderActionButtons();

        // 辅助函数
        void LoadCurrentSettings();
        void ApplySettings();
        void ResetToDefaults();
        void MarkSettingsChanged();
    };
}