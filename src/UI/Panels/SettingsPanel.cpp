#include "SettingsPanel.h"

#include "shellapi.h"
#include "ScopeCamera.h"
#include "D3DHooks.h"
#include "DataPersistence.h"
#include <fmt/format.h>
#include "LocalizationManager.h"
#include "misc/cpp/imgui_stdlib.h"

#include "ImGuiManager.h"



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
		// 选项卡界面
		//if (ImGui::BeginTabBar("SettingsTabs")) {
		//	

		//	ImGui::EndTabBar();
		//}
		RenderKeyBindingSettings();
		RenderInterfaceSettings();
		ImGui::Spacing();
		ImGui::Separator();
		RenderActionButtons();
	}

	void SettingsPanel::Update()
	{

	}

	void SettingsPanel::UpdateOutSideUI()
	{
		// 从DataPersistence获取最新的全局键位设置
		auto dataPersistence = DataPersistence::GetSingleton();
		const auto& globalSettings = dataPersistence->GetGlobalSettings();
		const auto& weaponInfo = dataPersistence->GetCurrentWeaponInfo();
		if (!weaponInfo.currentConfig)
			return;
		
		// 构建临时的键位结构用于检测
		KeyBindingSettings::CombinationKeys nightVisionKeys;
		nightVisionKeys.primaryKey = VKToImGuiKey(globalSettings.nightVisionKeyBindings[0]);
		nightVisionKeys.modifier = VKToImGuiKey(globalSettings.nightVisionKeyBindings[1]);
		
		KeyBindingSettings::CombinationKeys thermalVisionKeys;
		thermalVisionKeys.primaryKey = VKToImGuiKey(globalSettings.thermalVisionKeyBindings[0]);
		thermalVisionKeys.modifier = VKToImGuiKey(globalSettings.thermalVisionKeyBindings[1]);
		
		// 检查夜视效果快捷键（边沿触发）
		if (weaponInfo.currentConfig->scopeSettings.nightVision)
		{
			static bool lastNightVisionState = false;
			bool currentNightVisionState = CheckCombinationKeysAsync(nightVisionKeys);

			if (currentNightVisionState && !lastNightVisionState) {
				D3DHooks::s_EnableNightVision = !D3DHooks::s_EnableNightVision;
				m_Manager->SetDebugText(D3DHooks::s_EnableNightVision ? "Night Vision: ON" : "Night Vision: OFF");
			}
			lastNightVisionState = currentNightVisionState;
		}
		
		if (weaponInfo.currentConfig->scopeSettings.thermalVision) {
			// 检查热成像效果快捷键（边沿触发）
			static bool lastThermalVisionState = false;
			bool currentThermalVisionState = CheckCombinationKeysAsync(thermalVisionKeys);

			if (currentThermalVisionState && !lastThermalVisionState) {
				D3DHooks::s_EnableThermalVision = !D3DHooks::s_EnableThermalVision;
				m_Manager->SetDebugText(D3DHooks::s_EnableThermalVision ? "Thermal Vision: ON" : "Thermal Vision: OFF");
			}
			lastThermalVisionState = currentThermalVisionState;
		}
	}

	void SettingsPanel::RenderInterfaceSettings()
	{
		// 语言选择
		ImGui::Text(LOC("settings.ui.language"));
		auto locManager = LocalizationManager::GetSingleton();
		
		// 创建语言选择下拉框
		Language currentLanguage = locManager->GetCurrentLanguage();
		int currentLang = static_cast<int>(currentLanguage);
		const char* currentLangName = locManager->GetLanguageName(currentLanguage);
		
		if (ImGui::BeginCombo("##Language", currentLangName)) {
			for (int i = 0; i < static_cast<int>(Language::COUNT); ++i) {
				Language lang = static_cast<Language>(i);
				bool isSelected = (currentLang == i);
				
				if (ImGui::Selectable(locManager->GetLanguageName(lang), isSelected)) {
					// 设置语言到LocalizationManager
					locManager->SetLanguage(lang);
					MarkSettingsChanged();
				}
				
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		RenderHelpTooltip(LOC("tooltip.select_language"));

	}


	void SettingsPanel::RenderKeyBindingSettings()
	{
		RenderSectionHeader("Key Bindings");

		ImGui::Text(LOC("settings.keys.description"));
		ImGui::Spacing();

		// 常用键位选项
		static const char* keyOptions[] = {
			"None", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
			"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
			"Space", "Enter", "Tab", "Escape", "Backspace", "Delete", "Insert", "Home", "End", "Page Up", "Page Down",
			"Up", "Down", "Left", "Right"
		};
		
		static const ImGuiKey keyValues[] = {
			ImGuiKey_None, ImGuiKey_F1, ImGuiKey_F2, ImGuiKey_F3, ImGuiKey_F4, ImGuiKey_F5, ImGuiKey_F6, ImGuiKey_F7, ImGuiKey_F8, ImGuiKey_F9, ImGuiKey_F10, ImGuiKey_F11, ImGuiKey_F12,
			ImGuiKey_A, ImGuiKey_B, ImGuiKey_C, ImGuiKey_D, ImGuiKey_E, ImGuiKey_F, ImGuiKey_G, ImGuiKey_H, ImGuiKey_I, ImGuiKey_J, ImGuiKey_K, ImGuiKey_L, ImGuiKey_M, ImGuiKey_N, ImGuiKey_O, ImGuiKey_P, ImGuiKey_Q, ImGuiKey_R, ImGuiKey_S, ImGuiKey_T, ImGuiKey_U, ImGuiKey_V, ImGuiKey_W, ImGuiKey_X, ImGuiKey_Y, ImGuiKey_Z,
			ImGuiKey_0, ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5, ImGuiKey_6, ImGuiKey_7, ImGuiKey_8, ImGuiKey_9,
			ImGuiKey_Space, ImGuiKey_Enter, ImGuiKey_Tab, ImGuiKey_Escape, ImGuiKey_Backspace, ImGuiKey_Delete, ImGuiKey_Insert, ImGuiKey_Home, ImGuiKey_End, ImGuiKey_PageUp, ImGuiKey_PageDown,
			ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow
		};

		// 修饰键选项
		static const char* modifierOptions[] = { "None", "Ctrl", "Shift", "Alt" };
		static const ImGuiKey modifierValues[] = { ImGuiKey_None, ImGuiKey_ModCtrl, ImGuiKey_ModShift, ImGuiKey_ModAlt };

		// 辅助函数：找到键在数组中的索引
		auto findKeyIndex = [&](ImGuiKey key) -> int {
			for (int i = 0; i < IM_ARRAYSIZE(keyValues); i++) {
				if (keyValues[i] == key) return i;
			}
			return 0; // 默认为 "None"
		};

		auto findModifierIndex = [&](ImGuiKey key) -> int {
			for (int i = 0; i < IM_ARRAYSIZE(modifierValues); i++) {
				if (modifierValues[i] == key) return i;
			}
			return 0; // 默认为 "None"
		};

		ImGui::Columns(2, "KeyBindingColumns", false);
		ImGui::SetColumnWidth(0, 200);

		// Menu Toggle
		ImGui::Text(LOC("settings.keys.menu_toggle"));
		ImGui::NextColumn();
		{
			int currentIndex = findKeyIndex(m_KeyBindingSettings.menuToggleKey);
			if (ImGui::Combo("##MenuToggle", &currentIndex, keyOptions, IM_ARRAYSIZE(keyOptions))) {
				m_KeyBindingSettings.menuToggleKey = keyValues[currentIndex];
				MarkSettingsChanged();
			}
		}

		ImGui::NextColumn();
		ImGui::Text(LOC("settings.keys.night_vision"));
		ImGui::NextColumn();
		{
			ImGui::Text(LOC("settings.keys.modifier"));
			ImGui::SameLine();
			int modifierIndex = findModifierIndex(m_KeyBindingSettings.nightVisionKeys.modifier);
			if (ImGui::Combo("##NightVisionModifier", &modifierIndex, modifierOptions, IM_ARRAYSIZE(modifierOptions))) {
				m_KeyBindingSettings.nightVisionKeys.modifier = modifierValues[modifierIndex];
				MarkSettingsChanged();
			}
			
			ImGui::Text(LOC("settings.keys.key"));
			ImGui::SameLine();
			int keyIndex = findKeyIndex(m_KeyBindingSettings.nightVisionKeys.primaryKey);
			if (ImGui::Combo("##NightVisionKey", &keyIndex, keyOptions, IM_ARRAYSIZE(keyOptions))) {
				m_KeyBindingSettings.nightVisionKeys.primaryKey = keyValues[keyIndex];
				MarkSettingsChanged();
			}
		}

		ImGui::NextColumn();
		ImGui::Text(LOC("settings.keys.thermal_vision"));
		ImGui::NextColumn();
		{
			ImGui::Text(LOC("settings.keys.modifier"));
			ImGui::SameLine();
			int modifierIndex = findModifierIndex(m_KeyBindingSettings.thermalVisionKeys.modifier);
			if (ImGui::Combo("##ThermalVisionModifier", &modifierIndex, modifierOptions, IM_ARRAYSIZE(modifierOptions))) {
				m_KeyBindingSettings.thermalVisionKeys.modifier = modifierValues[modifierIndex];
				MarkSettingsChanged();
			}
			
			ImGui::Text(LOC("settings.keys.key"));
			ImGui::SameLine();
			int keyIndex = findKeyIndex(m_KeyBindingSettings.thermalVisionKeys.primaryKey);
			if (ImGui::Combo("##ThermalVisionKey", &keyIndex, keyOptions, IM_ARRAYSIZE(keyOptions))) {
				m_KeyBindingSettings.thermalVisionKeys.primaryKey = keyValues[keyIndex];
				MarkSettingsChanged();
			}
		}

		ImGui::Columns(1);
		ImGui::Spacing();
		ImGui::Text(LOC("settings.keys.tips"));
		ImGui::BulletText(LOC("tooltip.key_none"));
		ImGui::BulletText(LOC("tooltip.key_combination"));
		ImGui::BulletText(LOC("tooltip.key_immediate"));

	}


	void SettingsPanel::RenderActionButtons()
	{
		// 保存和应用按钮
		bool hasChanges = m_SettingsChanged;

		if (hasChanges) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
		}

		if (ImGui::Button(LOC("button.save"), ImVec2(120, 0))) {
			if (SaveSettings()) {
				ApplyUISettings();
				isSaved = true;
				m_SettingsChanged = false;
				m_Manager->SetDebugText(LOC("status.settings_saved"));
			} else {
				m_Manager->SetDebugText(LOC("status.settings_failed"));
			}
		}

		if (hasChanges) {
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();

		if (ImGui::Button(LOC("button.cancel"), ImVec2(80, 0))) {
			LoadSettings();  // 重新加载设置
			m_SettingsChanged = false;
			isSaved = true;
			m_Manager->SetDebugText(LOC("status.changes_cancelled"));
		}

		ImGui::SameLine();

		if (hasChanges) {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), LOC("status.unsaved_changes"));
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), LOC("status.all_saved"));
		}
	}

	bool SettingsPanel::LoadSettings()
	{
		m_UISettings = UISettings{};
		
		// 从DataPersistence加载全局键位设置
		auto dataPersistence = DataPersistence::GetSingleton();
		const auto& globalSettings = dataPersistence->GetGlobalSettings();
		
		// 加载语言设置并同步到LocalizationManager
		auto locManager = LocalizationManager::GetSingleton();
		if (locManager && globalSettings.selectedLanguage >= 0 && 
			globalSettings.selectedLanguage < static_cast<int>(Language::COUNT)) {
			Language selectedLang = static_cast<Language>(globalSettings.selectedLanguage);
			locManager->SetLanguage(selectedLang);
		}
		
		// 转换数组格式到我们的键位结构
		// MenuToggle (单键)
		m_KeyBindingSettings.menuToggleKey = VKToImGuiKey(globalSettings.menuKeyBindings[0]);
		
		// NightVision (组合键)
		m_KeyBindingSettings.nightVisionKeys.primaryKey = VKToImGuiKey(globalSettings.nightVisionKeyBindings[0]);
		if (globalSettings.nightVisionKeyBindings[1] != 0) {
			m_KeyBindingSettings.nightVisionKeys.modifier = VKToImGuiKey(globalSettings.nightVisionKeyBindings[1]);
		} else {
			m_KeyBindingSettings.nightVisionKeys.modifier = ImGuiKey_None;
		}
		
		// ThermalVision (组合键)
		m_KeyBindingSettings.thermalVisionKeys.primaryKey = VKToImGuiKey(globalSettings.thermalVisionKeyBindings[0]);
		if (globalSettings.thermalVisionKeyBindings[1] != 0) {
			m_KeyBindingSettings.thermalVisionKeys.modifier = VKToImGuiKey(globalSettings.thermalVisionKeyBindings[1]);
		} else {
			m_KeyBindingSettings.thermalVisionKeys.modifier = ImGuiKey_None;
		}

		m_SettingsChanged = false;
		return true;
	}

	bool SettingsPanel::SaveSettings()
	{
		// 将我们的键位设置转换为GlobalSettings格式并保存
		try {
			auto dataPersistence = DataPersistence::GetSingleton();
			DataPersistence::GlobalSettings globalSettings = dataPersistence->GetGlobalSettings();
			
			// 保存语言设置
			auto locManager = LocalizationManager::GetSingleton();
			if (locManager) {
				globalSettings.selectedLanguage = static_cast<int>(locManager->GetCurrentLanguage());
			}
			
			// MenuToggle (单键)
			globalSettings.menuKeyBindings[0] = ImGuiKeyToVK(m_KeyBindingSettings.menuToggleKey);
			globalSettings.menuKeyBindings[1] = 0;  // 无修饰键
			globalSettings.menuKeyBindings[2] = 0;  // 无第二修饰键
			
			// NightVision (组合键)
			globalSettings.nightVisionKeyBindings[0] = ImGuiKeyToVK(m_KeyBindingSettings.nightVisionKeys.primaryKey);
			globalSettings.nightVisionKeyBindings[1] = ImGuiKeyToVK(m_KeyBindingSettings.nightVisionKeys.modifier);
			globalSettings.nightVisionKeyBindings[2] = 0;  // 无第二修饰键
			
			// ThermalVision (组合键)
			globalSettings.thermalVisionKeyBindings[0] = ImGuiKeyToVK(m_KeyBindingSettings.thermalVisionKeys.primaryKey);
			globalSettings.thermalVisionKeyBindings[1] = ImGuiKeyToVK(m_KeyBindingSettings.thermalVisionKeys.modifier);
			globalSettings.thermalVisionKeyBindings[2] = 0;  // 无第二修饰键
			
			// 保存到DataPersistence
			dataPersistence->SetGlobalSettings(globalSettings);
			
			m_SettingsChanged = false;
			return true;
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error saving settings: {}", e.what()).c_str());
			return false;
		}
	}

	void SettingsPanel::ResetToDefaults()
	{
		m_UISettings = UISettings{};
		m_KeyBindingSettings = KeyBindingSettings{};
		MarkSettingsChanged();
	}

	const char* SettingsPanel::GetKeyName(ImGuiKey key)
	{
		if (key == ImGuiKey_None) {
			return "None";
		}
		
		// ImGui 自带的键名获取函数
		const char* keyName = ImGui::GetKeyName(key);
		if (keyName && strlen(keyName) > 0) {
			return keyName;
		}
		
		return "Unknown";
	}

	bool SettingsPanel::IsKeyPressed(ImGuiKey key)
	{
		if (key == ImGuiKey_None) {
			return false;
		}
		
		// 对于修饰键，检查相应的物理键
		if (key == ImGuiKey_ModCtrl) {
			return ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
		}
		if (key == ImGuiKey_ModShift) {
			return ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
		}
		if (key == ImGuiKey_ModAlt) {
			return ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
		}
		
		return ImGui::IsKeyDown(key);
	}

	// 辅助函数：将ImGuiKey转换为Windows虚拟键码
	int SettingsPanel::ImGuiKeyToVK(ImGuiKey key)
	{
		switch (key) {
			// 功能键
			case ImGuiKey_F1: return VK_F1;
			case ImGuiKey_F2: return VK_F2;
			case ImGuiKey_F3: return VK_F3;
			case ImGuiKey_F4: return VK_F4;
			case ImGuiKey_F5: return VK_F5;
			case ImGuiKey_F6: return VK_F6;
			case ImGuiKey_F7: return VK_F7;
			case ImGuiKey_F8: return VK_F8;
			case ImGuiKey_F9: return VK_F9;
			case ImGuiKey_F10: return VK_F10;
			case ImGuiKey_F11: return VK_F11;
			case ImGuiKey_F12: return VK_F12;
			
			// 字母键
			case ImGuiKey_A: return 'A';
			case ImGuiKey_B: return 'B';
			case ImGuiKey_C: return 'C';
			case ImGuiKey_D: return 'D';
			case ImGuiKey_E: return 'E';
			case ImGuiKey_F: return 'F';
			case ImGuiKey_G: return 'G';
			case ImGuiKey_H: return 'H';
			case ImGuiKey_I: return 'I';
			case ImGuiKey_J: return 'J';
			case ImGuiKey_K: return 'K';
			case ImGuiKey_L: return 'L';
			case ImGuiKey_M: return 'M';
			case ImGuiKey_N: return 'N';
			case ImGuiKey_O: return 'O';
			case ImGuiKey_P: return 'P';
			case ImGuiKey_Q: return 'Q';
			case ImGuiKey_R: return 'R';
			case ImGuiKey_S: return 'S';
			case ImGuiKey_T: return 'T';
			case ImGuiKey_U: return 'U';
			case ImGuiKey_V: return 'V';
			case ImGuiKey_W: return 'W';
			case ImGuiKey_X: return 'X';
			case ImGuiKey_Y: return 'Y';
			case ImGuiKey_Z: return 'Z';
			
			// 数字键
			case ImGuiKey_0: return '0';
			case ImGuiKey_1: return '1';
			case ImGuiKey_2: return '2';
			case ImGuiKey_3: return '3';
			case ImGuiKey_4: return '4';
			case ImGuiKey_5: return '5';
			case ImGuiKey_6: return '6';
			case ImGuiKey_7: return '7';
			case ImGuiKey_8: return '8';
			case ImGuiKey_9: return '9';
			
			// 特殊键
			case ImGuiKey_Space: return VK_SPACE;
			case ImGuiKey_Enter: return VK_RETURN;
			case ImGuiKey_Tab: return VK_TAB;
			case ImGuiKey_Escape: return VK_ESCAPE;
			case ImGuiKey_Backspace: return VK_BACK;
			case ImGuiKey_Delete: return VK_DELETE;
			case ImGuiKey_Insert: return VK_INSERT;
			case ImGuiKey_Home: return VK_HOME;
			case ImGuiKey_End: return VK_END;
			case ImGuiKey_PageUp: return VK_PRIOR;
			case ImGuiKey_PageDown: return VK_NEXT;
			case ImGuiKey_UpArrow: return VK_UP;
			case ImGuiKey_DownArrow: return VK_DOWN;
			case ImGuiKey_LeftArrow: return VK_LEFT;
			case ImGuiKey_RightArrow: return VK_RIGHT;
			
			// 修饰键
			case ImGuiKey_ModCtrl: return VK_CONTROL;
			case ImGuiKey_ModShift: return VK_SHIFT;
			case ImGuiKey_ModAlt: return VK_MENU;
			
			default: return 0;
		}
	}

	// 辅助函数：将Windows虚拟键码转换为ImGuiKey
	ImGuiKey SettingsPanel::VKToImGuiKey(int vk)
	{
		switch (vk) {
			// 功能键
			case VK_F1: return ImGuiKey_F1;
			case VK_F2: return ImGuiKey_F2;
			case VK_F3: return ImGuiKey_F3;
			case VK_F4: return ImGuiKey_F4;
			case VK_F5: return ImGuiKey_F5;
			case VK_F6: return ImGuiKey_F6;
			case VK_F7: return ImGuiKey_F7;
			case VK_F8: return ImGuiKey_F8;
			case VK_F9: return ImGuiKey_F9;
			case VK_F10: return ImGuiKey_F10;
			case VK_F11: return ImGuiKey_F11;
			case VK_F12: return ImGuiKey_F12;
			
			// 字母键
			case 'A': return ImGuiKey_A;
			case 'B': return ImGuiKey_B;
			case 'C': return ImGuiKey_C;
			case 'D': return ImGuiKey_D;
			case 'E': return ImGuiKey_E;
			case 'F': return ImGuiKey_F;
			case 'G': return ImGuiKey_G;
			case 'H': return ImGuiKey_H;
			case 'I': return ImGuiKey_I;
			case 'J': return ImGuiKey_J;
			case 'K': return ImGuiKey_K;
			case 'L': return ImGuiKey_L;
			case 'M': return ImGuiKey_M;
			case 'N': return ImGuiKey_N;
			case 'O': return ImGuiKey_O;
			case 'P': return ImGuiKey_P;
			case 'Q': return ImGuiKey_Q;
			case 'R': return ImGuiKey_R;
			case 'S': return ImGuiKey_S;
			case 'T': return ImGuiKey_T;
			case 'U': return ImGuiKey_U;
			case 'V': return ImGuiKey_V;
			case 'W': return ImGuiKey_W;
			case 'X': return ImGuiKey_X;
			case 'Y': return ImGuiKey_Y;
			case 'Z': return ImGuiKey_Z;
			
			// 数字键
			case '0': return ImGuiKey_0;
			case '1': return ImGuiKey_1;
			case '2': return ImGuiKey_2;
			case '3': return ImGuiKey_3;
			case '4': return ImGuiKey_4;
			case '5': return ImGuiKey_5;
			case '6': return ImGuiKey_6;
			case '7': return ImGuiKey_7;
			case '8': return ImGuiKey_8;
			case '9': return ImGuiKey_9;
			
			// 特殊键
			case VK_SPACE: return ImGuiKey_Space;
			case VK_RETURN: return ImGuiKey_Enter;
			case VK_TAB: return ImGuiKey_Tab;
			case VK_ESCAPE: return ImGuiKey_Escape;
			case VK_BACK: return ImGuiKey_Backspace;
			case VK_DELETE: return ImGuiKey_Delete;
			case VK_INSERT: return ImGuiKey_Insert;
			case VK_HOME: return ImGuiKey_Home;
			case VK_END: return ImGuiKey_End;
			case VK_PRIOR: return ImGuiKey_PageUp;
			case VK_NEXT: return ImGuiKey_PageDown;
			case VK_UP: return ImGuiKey_UpArrow;
			case VK_DOWN: return ImGuiKey_DownArrow;
			case VK_LEFT: return ImGuiKey_LeftArrow;
			case VK_RIGHT: return ImGuiKey_RightArrow;
			
			// 修饰键
			case VK_CONTROL: return ImGuiKey_ModCtrl;
			case VK_SHIFT: return ImGuiKey_ModShift;
			case VK_MENU: return ImGuiKey_ModAlt;
			
			default: return ImGuiKey_None;
		}
	}

	void SettingsPanel::ApplyUISettings()
	{
		// 应用UI设置到ImGui或其他系统
		ImGuiIO& io = ImGui::GetIO();
	}

	bool SettingsPanel::CheckCombinationKeys(const KeyBindingSettings::CombinationKeys& keys)
	{
		// 如果没有设置主键，返回false
		if (keys.primaryKey == ImGuiKey_None) {
			return false;
		}

		// 检查主键是否被按下（边沿触发）
		if (!ImGui::IsKeyPressed(keys.primaryKey)) {
			return false;
		}

		// 检查修饰键1（如果设置了）
		if (keys.modifier != ImGuiKey_None && !IsKeyPressed(keys.modifier)) {
			return false;
		}

		// 所有需要的键都被按下
		return true;
	}

	// 使用GetAsyncKeyState检测组合键（用于UI外部检测）
	bool SettingsPanel::CheckCombinationKeysAsync(const KeyBindingSettings::CombinationKeys& keys)
	{
		// 如果没有设置主键，返回false
		if (keys.primaryKey == ImGuiKey_None) {
			return false;
		}

		// 转换为VK码
		int primaryVK = ImGuiKeyToVK(keys.primaryKey);
		if (primaryVK == 0) {
			return false;
		}

		// 检查主键是否被按下
		if (!(GetAsyncKeyState(primaryVK) & 0x8000)) {
			return false;
		}

		// 检查修饰键（如果设置了）
		if (keys.modifier != ImGuiKey_None) {
			int modifierVK = ImGuiKeyToVK(keys.modifier);
			if (modifierVK == 0 || !(GetAsyncKeyState(modifierVK) & 0x8000)) {
				return false;
			}
		}

		// 所有需要的键都被按下
		return true;
	}
}
