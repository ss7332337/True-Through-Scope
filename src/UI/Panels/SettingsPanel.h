#pragma once

#include "BasePanelInterface.h"

namespace ThroughScope
{
    class SettingsPanel : public BasePanelInterface
    {
    public:
        SettingsPanel(PanelManagerInterface* manager);
        ~SettingsPanel() override = default;
        
        // 基础接口实现
        void Render() override;
        void Update() override;
        const char* GetPanelName() const override { return "Settings"; }
        
        // 设置管理
        struct UISettings
        {
            bool showHelpTooltips = true;
            bool autoSaveEnabled = true;
            bool confirmBeforeReset = true;
            bool realTimeAdjustment = true;
            float autoSaveInterval = 30.0f;  // 秒
            int uiRefreshRate = 60;           // FPS
        };
        
        struct PerformanceSettings
        {
            bool enableVsync = true;
            bool optimizeForPerformance = false;
            int maxFPS = 144;
            bool reducedAnimations = false;
        };
        
        struct KeyBindingSettings
        {
            int menuToggleKey = VK_F2;
            int quickSaveKey = VK_F5;
            int quickLoadKey = VK_F9;
            int resetKey = VK_F12;
        };
        
        const UISettings& GetUISettings() const { return m_UISettings; }
        const PerformanceSettings& GetPerformanceSettings() const { return m_PerformanceSettings; }
        const KeyBindingSettings& GetKeyBindingSettings() const { return m_KeyBindingSettings; }
        
        void SetUISettings(const UISettings& settings) { m_UISettings = settings; }
        void SetPerformanceSettings(const PerformanceSettings& settings) { m_PerformanceSettings = settings; }
        void SetKeyBindingSettings(const KeyBindingSettings& settings) { m_KeyBindingSettings = settings; }
        
        // 设置持久化
        bool LoadSettings();
        bool SaveSettings();
        void ResetToDefaults();
        
    private:
        PanelManagerInterface* m_Manager;
        
        UISettings m_UISettings;
        PerformanceSettings m_PerformanceSettings;
        KeyBindingSettings m_KeyBindingSettings;
        
        // UI状态
        bool m_SettingsChanged = false;
        bool m_ShowAdvancedSettings = false;
        int m_SelectedTab = 0;
        
        // 按键绑定状态
        bool m_CapturingKey = false;
        int* m_KeyBeingCaptured = nullptr;
        std::string m_KeyCaptureName = "";
        
        // 渲染函数
        void RenderInterfaceSettings();
        void RenderPerformanceSettings();
        void RenderKeyBindingSettings();
        void RenderAdvancedSettings();
        void RenderActionButtons();
        
        // 辅助函数
        void MarkSettingsChanged() { m_SettingsChanged = true; }
        const char* GetKeyName(int vkCode);
        void StartKeyCapture(int* keyPtr, const std::string& keyName);
        void HandleKeyCapture();
        bool IsKeyPressed(int vkCode);
        
        // 设置应用
        void ApplyUISettings();
        void ApplyPerformanceSettings();
        void ApplyKeyBindingSettings();
    };
}