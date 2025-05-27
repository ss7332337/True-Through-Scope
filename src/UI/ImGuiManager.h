#pragma once

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <memory>
#include <vector>

#include "BasePanelInterface.h"
#include "CameraAdjustmentPanel.h"
#include "DebugPanel.h"
#include "ModelSwitcherPanel.h"
#include "ReticlePanel.h"
#include "SettingsPanel.h"
#include "ZoomDataPanel.h"

#include "Constants.h"
#include "D3DHooks.h"
#include "DataPersistence.h"
#include "RenderUtilities.h"
#include "ScopeCamera.h"

namespace ThroughScope
{
	class ImGuiManager : public PanelManagerInterface
	{
	public:
		static ImGuiManager* GetSingleton();

		bool Initialize();
		void Shutdown();

		void Render();
		void Update();

		// 状态查询
		bool IsInitialized() const { return m_Initialized; }
		bool IsMenuOpen() const { return m_MenuOpen; }
		void ToggleMenu() { m_MenuOpen = !m_MenuOpen; }

		// PanelManagerInterface 实现
		RE::NiAVObject* GetTTSNode() override;
		void SetDebugText(const char* text) override;
		DataPersistence::WeaponInfo GetCurrentWeaponInfo() override;
		void ShowErrorDialog(const std::string& title, const std::string& message) override;
		void MarkUnsavedChanges() override { s_unsavedChangeCount++; }
		void MarkSaved() override { s_unsavedChangeCount--; }

	private:
		ImGuiManager() = default;
		~ImGuiManager() = default;

		// 初始化面板系统
		void InitializePanels();
		void ShutdownPanels();

		// UI渲染
		void RenderMainMenu();
		void RenderStatusBar();
		void RenderErrorDialog();

		// 错误处理
		struct ErrorDialog
		{
			bool show = false;
			std::string title;
			std::string message;
		} m_ErrorDialog;

		// 面板管理
		std::vector<std::unique_ptr<BasePanelInterface>> m_Panels;
		std::unique_ptr<CameraAdjustmentPanel> m_CameraAdjustmentPanel;
		std::unique_ptr<ModelSwitcherPanel> m_ModelSwitcherPanel;
		std::unique_ptr<ReticlePanel> m_ReticlePanel;
		std::unique_ptr<DebugPanel> m_DebugPanel;
		std::unique_ptr<SettingsPanel> m_SettingsPanel;
		std::unique_ptr<ZoomDataPanel> m_ZoomDataPanel;

		// 状态变量
		bool m_Initialized = false;
		bool m_MenuOpen = false;
		bool m_HasUnsavedChanges = false;

		static UINT s_unsavedChangeCount;
		// 调试信息
		char m_DebugText[1024] = "";

		// UI样式设置
		ImVec4 m_AccentColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
		ImVec4 m_WarningColor = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
		ImVec4 m_SuccessColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
		ImVec4 m_ErrorColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
	};
}
