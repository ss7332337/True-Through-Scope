#pragma once

#include "BasePanelInterface.h"
#include "LocalizationManager.h"

namespace ThroughScope
{
    /**
     * PostProcessPanel - 后处理效果参数调整面板
     * 实时调整 ScopeHDR、BloomPass、LUTPass 的参数
     */
    class PostProcessPanel : public BasePanelInterface
    {
    public:
        PostProcessPanel(PanelManagerInterface* manager);
        ~PostProcessPanel() override = default;

        // 基础接口实现
        void Render() override;
        void Update() override;
        const char* GetPanelName() const override { return "Post Process"; }
        bool GetSaved() const override { return m_IsSaved; }

    private:
        PanelManagerInterface* m_Manager;
        bool m_IsSaved = true;

        // UI 状态
        bool m_ShowHDRSection = true;
        bool m_ShowBloomSection = true;
        bool m_ShowAdvancedHDR = false;
        bool m_ShowAdvancedBloom = false;

        // 渲染各个部分
        void RenderHDRSection();
        void RenderBloomSection();
        void RenderPresetButtons();
        void RenderQuickActions();

        // HDR 预设
        void ApplyHDRPresetDefault();
        void ApplyHDRPresetBright();
        void ApplyHDRPresetCinematic();
        void ApplyHDRPresetHighContrast();

        // 辅助函数
        bool SliderFloatWithReset(const char* label, float* v, float v_min, float v_max, float reset_value, const char* format = "%.2f");
        bool SliderIntWithReset(const char* label, int* v, int v_min, int v_max, int reset_value);
        void MarkChanged();
    };
}
