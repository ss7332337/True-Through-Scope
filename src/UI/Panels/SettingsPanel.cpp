#include "SettingsPanel.h"

#include "shellapi.h"

namespace ThroughScope
{
	// 实现
	SettingsPanel::SettingsPanel(PanelManagerInterface* manager) :
		m_Manager(manager)
	{
		LoadSettings();
	}

	void SettingsPanel::Render()
	{
		HandleKeyCapture();

		// 选项卡界面
		if (ImGui::BeginTabBar("SettingsTabs")) {
			if (ImGui::BeginTabItem("Interface")) {
				RenderInterfaceSettings();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Performance")) {
				RenderPerformanceSettings();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Key Bindings")) {
				RenderKeyBindingSettings();
				ImGui::EndTabItem();
			}

			//if (m_ShowAdvancedSettings) {
			//	if (ImGui::BeginTabItem("Advanced")) {
			//		RenderAdvancedSettings();
			//		ImGui::EndTabItem();
			//	}
			//}

			ImGui::EndTabBar();
		}

		ImGui::Spacing();
		ImGui::Separator();
		RenderActionButtons();
	}

	void SettingsPanel::Update()
	{
		// 检查按键状态变化
		static bool lastF2State = false;
		bool currentF2State = IsKeyPressed(m_KeyBindingSettings.menuToggleKey);

		if (currentF2State && !lastF2State) {
			// 菜单切换键被按下
		}
		lastF2State = currentF2State;
	}

	void SettingsPanel::RenderInterfaceSettings()
	{
		RenderSectionHeader("User Interface Settings");

		if (ImGui::Checkbox("Show Help Tooltips", &m_UISettings.showHelpTooltips)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Show helpful tooltips when hovering over UI elements");

		if (ImGui::Checkbox("Auto-Save Changes", &m_UISettings.autoSaveEnabled)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Automatically save changes periodically");

		if (m_UISettings.autoSaveEnabled) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			if (ImGui::SliderFloat("##AutoSaveInterval", &m_UISettings.autoSaveInterval, 10.0f, 300.0f, "%.0fs")) {
				MarkSettingsChanged();
			}
			RenderHelpTooltip("How often to auto-save (in seconds)");
		}

		if (ImGui::Checkbox("Confirm Before Reset", &m_UISettings.confirmBeforeReset)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Show confirmation dialog before resetting adjustments");

		if (ImGui::Checkbox("Real-time Adjustment", &m_UISettings.realTimeAdjustment)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Apply changes immediately as you adjust sliders");

		ImGui::Spacing();
		ImGui::Text("UI Refresh Rate:");
		if (ImGui::SliderInt("##RefreshRate", &m_UISettings.uiRefreshRate, 30, 144, "%d FPS")) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Lower refresh rates can improve performance");

		ImGui::Spacing();
		ImGui::Checkbox("Show Advanced Settings", &m_ShowAdvancedSettings);
		RenderHelpTooltip("Show advanced configuration options");
	}

	void SettingsPanel::RenderPerformanceSettings()
	{
		RenderSectionHeader("Performance Settings");

		if (ImGui::Checkbox("Enable V-Sync", &m_PerformanceSettings.enableVsync)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Synchronize frame rate with display refresh rate");

		if (ImGui::Checkbox("Optimize for Performance", &m_PerformanceSettings.optimizeForPerformance)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Reduce visual quality for better performance");

		ImGui::Text("Maximum FPS:");
		if (ImGui::SliderInt("##MaxFPS", &m_PerformanceSettings.maxFPS, 30, 240, "%d")) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Maximum frame rate limit (0 = unlimited)");

		if (ImGui::Checkbox("Reduced Animations", &m_PerformanceSettings.reducedAnimations)) {
			MarkSettingsChanged();
		}
		RenderHelpTooltip("Reduce UI animations to improve performance");

		ImGui::Spacing();
		ImGui::TextColored(m_AccentColor, "Memory Usage:");
		ImGui::Text("UI Memory: ~%.1f MB", ImGui::GetIO().MetricsRenderVertices * sizeof(ImDrawVert) / (1024.0f * 1024.0f));
		ImGui::Text("Current FPS: %.1f", ImGui::GetIO().Framerate);
	}

	void SettingsPanel::RenderKeyBindingSettings()
	{
		RenderSectionHeader("Key Bindings");

		ImGui::Text("Click on a key binding to change it, then press the new key.");
		ImGui::Spacing();

		ImGui::Columns(2, "KeyBindingColumns", false);
		ImGui::SetColumnWidth(0, 200);

		// Menu Toggle
		ImGui::Text("Menu Toggle:");
		ImGui::NextColumn();

		std::string menuKeyText = m_CapturingKey && m_KeyBeingCaptured == &m_KeyBindingSettings.menuToggleKey ?
		                              "Press key..." :
		                              GetKeyName(m_KeyBindingSettings.menuToggleKey);

		if (ImGui::Button(menuKeyText.c_str(), ImVec2(100, 0))) {
			StartKeyCapture(&m_KeyBindingSettings.menuToggleKey, "Menu Toggle");
		}

		ImGui::Spacing();
		if (ImGui::Button("Reset to Defaults")) {
			m_KeyBindingSettings.menuToggleKey = VK_F2;
			MarkSettingsChanged();
		}
	}

	void SettingsPanel::RenderAdvancedSettings()
	{
		RenderSectionHeader("Advanced Settings");

		ImGui::TextColored(m_WarningColor, "Warning: These settings are for advanced users only!");
		ImGui::Spacing();

		static bool enableDebugMode = false;
		if (ImGui::Checkbox("Enable Debug Mode", &enableDebugMode)) {
			// Apply debug mode settings
		}
		RenderHelpTooltip("Enable additional debug features and logging");

		static bool enableExperimentalFeatures = false;
		if (ImGui::Checkbox("Enable Experimental Features", &enableExperimentalFeatures)) {
			// Apply experimental features
		}
		RenderHelpTooltip("Enable features that are still in development");

		ImGui::Spacing();
		ImGui::Text("Configuration Files:");
		ImGui::BulletText("Settings: Documents/My Games/Fallout4/F4SE/ThroughScope/settings.json");
		ImGui::BulletText("Configs: Documents/My Games/Fallout4/F4SE/ThroughScope/configs/");

		if (ImGui::Button("Open Config Folder")) {
			// Open config folder in explorer
			std::string configPath = std::filesystem::current_path().string() + "\\Data\\F4SE\\Plugins\\ThroughScope";
			ShellExecuteA(nullptr, "open", configPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset All Settings")) {
			ImGui::OpenPopup("Reset Confirmation");
		}

		// Reset confirmation popup
		if (ImGui::BeginPopupModal("Reset Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to reset ALL settings to defaults?");
			ImGui::Text("This action cannot be undone.");
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("Yes, Reset Everything", ImVec2(150, 0))) {
				ResetToDefaults();
				ImGui::CloseCurrentPopup();
				m_Manager->SetDebugText("All settings reset to defaults");
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void SettingsPanel::RenderActionButtons()
	{
		// 保存和应用按钮
		bool hasChanges = m_SettingsChanged;

		if (hasChanges) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
		}

		if (ImGui::Button("Save Settings", ImVec2(120, 0))) {
			if (SaveSettings()) {
				ApplyUISettings();
				ApplyPerformanceSettings();
				ApplyKeyBindingSettings();
				isSaved = true;
				m_Manager->SetDebugText("Settings saved successfully!");
			} else {
				m_Manager->SetDebugText("Failed to save settings!");
			}
		}

		if (hasChanges) {
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();

		if (ImGui::Button("Apply", ImVec2(80, 0))) {
			ApplyUISettings();
			ApplyPerformanceSettings();
			ApplyKeyBindingSettings();
			m_Manager->SetDebugText("Settings applied");
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(80, 0))) {
			LoadSettings();  // 重新加载设置
			m_SettingsChanged = false;
			m_Manager->SetDebugText("Changes cancelled");
		}

		ImGui::SameLine();

		if (hasChanges) {
			ImGui::TextColored(m_WarningColor, "● Unsaved changes");
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○ All saved");
		}
	}

	bool SettingsPanel::LoadSettings()
	{
		// 这里应该从配置文件加载设置
		// 暂时使用默认值
		m_UISettings = UISettings{};
		m_PerformanceSettings = PerformanceSettings{};
		m_KeyBindingSettings = KeyBindingSettings{};

		m_SettingsChanged = false;
		return true;
	}

	bool SettingsPanel::SaveSettings()
	{
		// 这里应该保存设置到配置文件
		try {
			// TODO: 实现JSON配置文件保存
			return true;
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error saving settings: {}", e.what()).c_str());
			return false;
		}
	}

	void SettingsPanel::ResetToDefaults()
	{
		m_UISettings = UISettings{};
		m_PerformanceSettings = PerformanceSettings{};
		m_KeyBindingSettings = KeyBindingSettings{};
		MarkSettingsChanged();
	}

	const char* SettingsPanel::GetKeyName(int vkCode)
	{
		static std::unordered_map<int, const char*> keyNames = {
			{ VK_F1, "F1" }, { VK_F2, "F2" }, { VK_F3, "F3" }, { VK_F4, "F4" },
			{ VK_F5, "F5" }, { VK_F6, "F6" }, { VK_F7, "F7" }, { VK_F8, "F8" },
			{ VK_F9, "F9" }, { VK_F10, "F10" }, { VK_F11, "F11" }, { VK_F12, "F12" },
			{ VK_ESCAPE, "ESC" }, { VK_TAB, "TAB" }, { VK_SPACE, "SPACE" },
			{ VK_RETURN, "ENTER" }, { VK_BACK, "BACKSPACE" }, { VK_DELETE, "DELETE" },
			{ VK_INSERT, "INSERT" }, { VK_HOME, "HOME" }, { VK_END, "END" },
			{ VK_PRIOR, "PAGE UP" }, { VK_NEXT, "PAGE DOWN" },
			{ VK_UP, "UP" }, { VK_DOWN, "DOWN" }, { VK_LEFT, "LEFT" }, { VK_RIGHT, "RIGHT" },
			{ VK_LSHIFT, "LEFT SHIFT" }, { VK_RSHIFT, "RIGHT SHIFT" },
			{ VK_LCONTROL, "LEFT CTRL" }, { VK_RCONTROL, "RIGHT CTRL" },
			{ VK_LMENU, "LEFT ALT" }, { VK_RMENU, "RIGHT ALT" }
		};

		auto it = keyNames.find(vkCode);
		if (it != keyNames.end()) {
			return it->second;
		}

		// 处理字母和数字键
		if (vkCode >= 'A' && vkCode <= 'Z') {
			static char keyChar[2] = { 0 };
			keyChar[0] = (char)vkCode;
			return keyChar;
		}

		if (vkCode >= '0' && vkCode <= '9') {
			static char keyChar[2] = { 0 };
			keyChar[0] = (char)vkCode;
			return keyChar;
		}

		return "Unknown";
	}

	void SettingsPanel::StartKeyCapture(int* keyPtr, const std::string& keyName)
	{
		m_CapturingKey = true;
		m_KeyBeingCaptured = keyPtr;
		m_KeyCaptureName = keyName;
	}

	void SettingsPanel::HandleKeyCapture()
	{
		if (!m_CapturingKey)
			return;

		// 检查ESC键取消
		if (IsKeyPressed(VK_ESCAPE)) {
			m_CapturingKey = false;
			m_KeyBeingCaptured = nullptr;
			m_KeyCaptureName.clear();
			return;
		}

		// 检查其他按键
		for (int vk = 0; vk < 256; vk++) {
			if (vk == VK_ESCAPE)
				continue;  // 已经处理过ESC

			if (IsKeyPressed(vk)) {
				*m_KeyBeingCaptured = vk;
				m_CapturingKey = false;
				m_KeyBeingCaptured = nullptr;
				m_KeyCaptureName.clear();
				MarkSettingsChanged();
				break;
			}
		}
	}

	bool SettingsPanel::IsKeyPressed(int vkCode)
	{
		return (GetAsyncKeyState(vkCode) & 0x8000) != 0;
	}

	void SettingsPanel::ApplyUISettings()
	{
		// 应用UI设置到ImGui或其他系统
		ImGuiIO& io = ImGui::GetIO();
	}

	void SettingsPanel::ApplyPerformanceSettings()
	{
		// 应用性能设置
		if (m_PerformanceSettings.enableVsync) {
			// 启用垂直同步
		}

		if (m_PerformanceSettings.maxFPS > 0) {
			// 设置最大帧率限制
		}
	}

	void SettingsPanel::ApplyKeyBindingSettings()
	{
		// 应用按键绑定设置
		// 这里需要与主应用程序的按键处理系统集成
	}
}
