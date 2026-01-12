#pragma once

#include <imgui.h>
#include "DataPersistence.h"

namespace ThroughScope
{
    // 基础面板接口
    class BasePanelInterface
    {
    public:
        virtual ~BasePanelInterface() = default;
        
        // 渲染面板内容
        virtual void Render() = 0;
        
        // 更新面板逻辑（可选重写）
        virtual void Update() {}
        virtual void UpdateOutSideUI() {}
        
        // 面板初始化（可选重写）
        virtual bool Initialize() { return true; }
        
        // 面板清理（可选重写）
        virtual void Shutdown() {}
        
        // 获取面板名称
        virtual const char* GetPanelName() const = 0;
        
        // 获取面板ID (用于ImGui标识)
        virtual const char* GetPanelID() const = 0;
        
        // 是否显示该面板
        virtual bool ShouldShow() const { return true; }

		virtual bool GetSaved() const { return true; }
        
    protected:
        // 通用颜色定义
        ImVec4 m_AccentColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
        ImVec4 m_WarningColor = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
        ImVec4 m_SuccessColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
        ImVec4 m_ErrorColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        
        // 通用UI帮助函数
        void RenderHelpTooltip(const char* text)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", text);
            }
        }
        
        void RenderStatusText(const char* text, bool isSuccess = true)
        {
            ImGui::TextColored(isSuccess ? m_SuccessColor : m_ErrorColor, "%s", text);
        }
        
        void RenderSectionHeader(const char* text)
        {
            ImGui::TextColored(m_AccentColor, "%s", text);
            ImGui::Separator();
        }
    };
    
    // 面板管理器接口
    class PanelManagerInterface
    {
    public:
        virtual ~PanelManagerInterface() = default;
        
        // 获取TTSNode
        virtual RE::NiAVObject* GetTTSNode() = 0;
        
        // 设置调试文本
        virtual void SetDebugText(const char* text) = 0;
        
        // 获取当前武器信息
        virtual DataPersistence::WeaponInfo GetCurrentWeaponInfo() = 0;
        
        // 显示错误对话框
        virtual void ShowErrorDialog(const std::string& title, const std::string& message) = 0;
        
        // 标记有未保存的更改
        virtual void MarkUnsavedChanges() = 0;
        virtual void MarkSaved() = 0;
    };
}
