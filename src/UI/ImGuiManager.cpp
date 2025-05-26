#include "ImGuiManager.h"

namespace ThroughScope
{
	ImGuiManager* ImGuiManager::GetSingleton()
	{
		static ImGuiManager instance;
		return &instance;
	}

	bool ImGuiManager::Initialize()
	{
		logger::info("Initializing ImGui...");

		// 获取D3D11设备和上下文
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || !rendererData->device || !rendererData->context) {
			logger::error("Failed to get D3D11 device or context");
			return false;
		}

		auto swapchain = (IDXGISwapChain*)static_cast<void*>(rendererData->renderWindow->swapChain);
		DXGI_SWAP_CHAIN_DESC sd;
		swapchain->GetDesc(&sd);

		auto hwnd = sd.OutputWindow;
		if (!hwnd) {
			logger::error("Failed to get window handle");
			return false;
		}

		// 初始化ImGui上下文
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// 设置ImGui样式
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 6.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 3.0f;
		style.ScrollbarRounding = 2.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.ChildRounding = 3.0f;
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 4.0f);
		style.Alpha = 0.95f;

		// 设置主题色
		style.Colors[ImGuiCol_Header] = m_AccentColor;
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(m_AccentColor.x + 0.1f, m_AccentColor.y + 0.1f, m_AccentColor.z + 0.1f, m_AccentColor.w);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(m_AccentColor.x + 0.2f, m_AccentColor.y + 0.2f, m_AccentColor.z + 0.2f, m_AccentColor.w);
		style.Colors[ImGuiCol_Button] = m_AccentColor;
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(m_AccentColor.x + 0.1f, m_AccentColor.y + 0.1f, m_AccentColor.z + 0.1f, m_AccentColor.w);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(m_AccentColor.x + 0.2f, m_AccentColor.y + 0.2f, m_AccentColor.z + 0.2f, m_AccentColor.w);

		// 初始化ImGui后端
		if (!ImGui_ImplWin32_Init(hwnd)) {
			logger::error("Failed to initialize ImGui Win32 backend");
			return false;
		}

		if (!ImGui_ImplDX11_Init((ID3D11Device*)rendererData->device, (ID3D11DeviceContext*)rendererData->context)) {
			logger::error("Failed to initialize ImGui DX11 backend");
			ImGui_ImplWin32_Shutdown();
			return false;
		}

		// 初始化面板系统
		InitializePanels();

		m_Initialized = true;
		logger::info("ImGui initialized successfully");
		return true;
	}

	void ImGuiManager::Shutdown()
	{
		if (!m_Initialized)
			return;

		ShutdownPanels();

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		m_Initialized = false;
		logger::info("ImGui shut down");
	}

	void ImGuiManager::InitializePanels()
	{
		// 创建面板实例
		m_CameraAdjustmentPanel = std::make_unique<CameraAdjustmentPanel>(this);
		m_ModelSwitcherPanel = std::make_unique<ModelSwitcherPanel>(this);
		m_ZoomDataPanel = std::make_unique<ZoomDataPanel>(this);
		m_DebugPanel = std::make_unique<DebugPanel>(this);
		m_SettingsPanel = std::make_unique<SettingsPanel>(this);
		m_ReticlePanel = std::make_unique<ReticlePanel>(this);

		// 添加到面板列表
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_CameraAdjustmentPanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_ModelSwitcherPanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_ZoomDataPanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_ReticlePanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_DebugPanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_SettingsPanel.get())));

		// 初始化所有面板
		for (auto& panel : m_Panels) {
			if (!panel->Initialize()) {
				logger::warn("Failed to initialize panel: {}", panel->GetPanelName());
			}
		}

		logger::info("Initialized {} panels", m_Panels.size());
	}

	void ImGuiManager::ShutdownPanels()
	{
		// 关闭所有面板
		for (auto& panel : m_Panels) {
			panel->Shutdown();
		}

		m_Panels.clear();

		m_CameraAdjustmentPanel.reset();
		m_ModelSwitcherPanel.reset();
		m_ZoomDataPanel.reset();
		m_ReticlePanel.reset();
		m_DebugPanel.reset();
		m_SettingsPanel.reset();
	}

	void ImGuiManager::Render()
	{
		if (!m_Initialized || !m_MenuOpen)
			return;

		// 开始ImGui帧
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 渲染主菜单
		RenderMainMenu();

		// 渲染错误对话框
		RenderErrorDialog();

		// 完成ImGui帧
		ImGui::Render();

		// 渲染绘制数据
		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Valid && draw_data->CmdListsCount > 0) {
			ImGui_ImplDX11_RenderDrawData(draw_data);
		}
	}

	void ImGuiManager::Update()
	{
		static float lastUpdateTime = 0.0f;
		float currentTime = ImGui::GetTime();
		float deltaTime = currentTime - lastUpdateTime;
		lastUpdateTime = currentTime;

		// 性能优化 - 降低更新频率
		static int frameCounter = 0;
		frameCounter++;
		if (frameCounter % 2 == 0) {
			return;
		}


		bool UIOpen = RE::UI::GetSingleton()->GetMenuOpen("PauseMenu") || RE::UI::GetSingleton()->GetMenuOpen("WorkshopMenu") || RE::UI::GetSingleton()->GetMenuOpen("CursorMenu");
		// 处理菜单切换
		if (!UIOpen && (GetAsyncKeyState(VK_F2) & 0x1))
		{
			ToggleMenu();
			auto mc = RE::MenuCursor::GetSingleton();
			REX::W32::ShowCursor(m_MenuOpen);

			auto input = RE::BSInputDeviceManager::GetSingleton();
			RE::ControlMap::GetSingleton()->ignoreKeyboardMouse = m_MenuOpen;

			if (m_MenuOpen) {
				logger::info("Menu opened");
			}
		}

		// 更新所有面板
		if (!UIOpen && m_MenuOpen && m_Initialized) {
			for (auto& panel : m_Panels) {
				panel->Update();
			}
		}
	}

	void ImGuiManager::RenderMainMenu()
	{
		const float menuWidth = 650.0f;
		const float menuHeight = 750.0f;

		// 居中窗口
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
		ImGui::SetNextWindowPos(ImVec2(center.x - menuWidth / 2, center.y - menuHeight / 2), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_FirstUseEver);

		// 设置窗口标志
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
		if (m_HasUnsavedChanges) {
			windowFlags |= ImGuiWindowFlags_UnsavedDocument;
		}

		if (ImGui::Begin("TrueThroughScope Settings", &m_MenuOpen, windowFlags)) {
			// 顶部状态栏
			RenderStatusBar();
			ImGui::Spacing();

			// 主要内容区域 - 选项卡系统
			if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_Reorderable)) {
				// 遍历所有面板并渲染选项卡
				for (auto& panel : m_Panels) {
					if (!panel->ShouldShow()) {
						continue;  // 跳过不应该显示的面板
					}

					if (ImGui::BeginTabItem(panel->GetPanelName())) {
						panel->Render();
						ImGui::EndTabItem();
					}
				}

				ImGui::EndTabBar();
			}

			// 显示调试文本
			if (strlen(m_DebugText) > 0) {
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::TextWrapped("%s", m_DebugText);
			}
		}
		ImGui::End();
	}

	void ImGuiManager::RenderStatusBar()
	{
		ImGui::Separator();

		// 状态栏分为三列
		ImGui::Columns(3, "StatusColumns", false);

		// 左侧：当前武器信息
		auto weaponInfo = GetCurrentWeaponInfo();
		if (weaponInfo.weapon) {
			ImGui::TextColored(m_SuccessColor, "✓ Weapon Loaded");
		} else {
			ImGui::TextColored(m_WarningColor, "⚠ No Weapon");
		}

		ImGui::NextColumn();

		// 中间：TTSNode状态
		auto ttsNode = GetTTSNode();
		if (ttsNode) {
			ImGui::TextColored(m_SuccessColor, "✓ TTSNode Ready");
		} else {
			ImGui::TextColored(m_WarningColor, "⚠ No TTSNode");
		}

		ImGui::NextColumn();

		// 右侧：保存状态
		if (m_HasUnsavedChanges) {
			ImGui::TextColored(m_WarningColor, "● Unsaved Changes");
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○ All Saved");
		}

		ImGui::Columns(1);
		ImGui::Separator();
	}

	void ImGuiManager::RenderErrorDialog()
	{
		if (m_ErrorDialog.show) {
			ImGui::OpenPopup(m_ErrorDialog.title.c_str());
			m_ErrorDialog.show = false;  // 只打开一次
		}

		if (ImGui::BeginPopupModal(m_ErrorDialog.title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextWrapped("%s", m_ErrorDialog.message.c_str());
			ImGui::Spacing();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
				m_ErrorDialog.title.clear();
				m_ErrorDialog.message.clear();
			}

			ImGui::EndPopup();
		}
	}

	// PanelManagerInterface 实现
	RE::NiAVObject* ImGuiManager::GetTTSNode()
	{
		try {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (playerCharacter && playerCharacter->Get3D()) {
				auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
				if (weaponNode && weaponNode->IsNode()) {
					auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
					return weaponNiNode->GetObjectByName("TTSNode");
				}
			}
		} catch (...) {
			// 忽略异常
		}
		return nullptr;
	}

	void ImGuiManager::SetDebugText(const char* text)
	{
		if (text && strlen(text) < sizeof(m_DebugText)) {
			strcpy_s(m_DebugText, sizeof(m_DebugText), text);
		}
	}

	DataPersistence::WeaponInfo ImGuiManager::GetCurrentWeaponInfo()
	{
		return DataPersistence::GetCurrentWeaponInfo();
	}

	void ImGuiManager::ShowErrorDialog(const std::string& title, const std::string& message)
	{
		m_ErrorDialog.title = title;
		m_ErrorDialog.message = message;
		m_ErrorDialog.show = true;
	}
}
