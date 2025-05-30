#pragma once

#include "BasePanelInterface.h"
#include "../Localization/LocalizationManager.h"

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
        void UpdateOutSideUI() override;
        const char* GetPanelName() const override { return LOC("ui.menu.settings"); }
		bool GetSaved() const override { return isSaved; }

        // 设置管理
        struct UISettings
        {
            bool showHelpTooltips = true;
            bool autoSaveEnabled = true;
            bool confirmBeforeReset = true;
            bool realTimeAdjustment = true;
            // 语言设置已移动到GlobalSettings中
        };
        
        struct KeyBindingSettings
        {
            ImGuiKey menuToggleKey = ImGuiKey_F2;
            ImGuiKey quickSaveKey = ImGuiKey_F5;
            ImGuiKey quickLoadKey = ImGuiKey_F9;
            ImGuiKey resetKey = ImGuiKey_F12;
            
            // 组合键设置结构体（简化为最多两个键）
            struct CombinationKeys {
                ImGuiKey primaryKey = ImGuiKey_None;    // 主键
                ImGuiKey modifier = ImGuiKey_None;      // 修饰键（最多一个）
            };
            
            // 夜视和热成像效果的组合键设置
            CombinationKeys nightVisionKeys;
            CombinationKeys thermalVisionKeys;
        };
        
        const UISettings& GetUISettings() const { return m_UISettings; }
        const KeyBindingSettings& GetKeyBindingSettings() const { return m_KeyBindingSettings; }
        
        void SetUISettings(const UISettings& settings) { m_UISettings = settings; }
        void SetKeyBindingSettings(const KeyBindingSettings& settings) { m_KeyBindingSettings = settings; }
        
        // 设置持久化
        bool LoadSettings();
        bool SaveSettings();
        void ResetToDefaults();
        
    private:
        PanelManagerInterface* m_Manager;
		bool isSaved = true;

        UISettings m_UISettings;
        KeyBindingSettings m_KeyBindingSettings;
        
        // UI状态
        bool m_SettingsChanged = false;
        bool m_ShowAdvancedSettings = false;
        int m_SelectedTab = 0;
        
        // 渲染函数
        void RenderInterfaceSettings();
        void RenderKeyBindingSettings();
        void RenderAdvancedSettings();
        void RenderActionButtons();
        
        // 辅助函数
        void MarkSettingsChanged() { m_SettingsChanged = true; isSaved = false; }
        bool IsKeyPressed(ImGuiKey key);
		const char* GetKeyName(ImGuiKey key);
        bool CheckCombinationKeys(const KeyBindingSettings::CombinationKeys& keys);
		bool CheckCombinationKeysAsync(const KeyBindingSettings::CombinationKeys& keys);
		
		// ImGuiKey 和 VK 码转换函数
		int ImGuiKeyToVK(ImGuiKey key);
		ImGuiKey VKToImGuiKey(int vk);
        
        // 设置应用
        void ApplyUISettings();
    };
}
