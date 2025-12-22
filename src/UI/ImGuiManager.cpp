#include "ImGuiManager.h"

namespace ThroughScope
{
	UINT ImGuiManager::s_unsavedChangeCount = 0;

	ImGuiManager* ImGuiManager::GetSingleton()
	{
		static ImGuiManager instance;
		return &instance;
	}

	bool ImGuiManager::Initialize()
	{
		logger::info("Initializing ImGui...");

		// 检查是否已经有ImGui上下文存在（避免与其他MOD冲突）
		if (ImGui::GetCurrentContext() != nullptr) {
			logger::warn("ImGui context already exists, using existing context");
			// 如果上下文已存在，尝试获取现有的IO对象并继续后续初始化
			ImGuiIO& io = ImGui::GetIO();

			// 但仍需要检查字体是否需要加载
			if (io.Fonts->Fonts.Size == 0) {
				LoadLanguageFonts();
			}

			// 继续初始化面板系统和本地化
			InitializePanels();

			auto localization = LocalizationManager::GetSingleton();
			if (!localization->Initialize()) {
				logger::warn("Failed to initialize localization system, using fallback English text");
			}

			auto dataPersistence = DataPersistence::GetSingleton();
			const auto& globalSettings = dataPersistence->GetGlobalSettings();
			if (globalSettings.selectedLanguage >= 0 &&
				globalSettings.selectedLanguage < static_cast<int>(Language::COUNT)) {
				Language savedLanguage = static_cast<Language>(globalSettings.selectedLanguage);
				localization->SetLanguage(savedLanguage);

			}

			m_Initialized = true;

			return true;
		}

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
		m_CreatedImGuiContext = true;  // 标记我们创建了上下文
		ImGuiIO& io = ImGui::GetIO();
		
		// 加载多语言字体支持
		LoadLanguageFonts();
		
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

		// 初始化本地化系统
		auto localization = LocalizationManager::GetSingleton();
		if (!localization->Initialize()) {
			logger::warn("Failed to initialize localization system, using fallback English text");
		}
		
		// 从DataPersistence加载保存的语言设置
		auto dataPersistence = DataPersistence::GetSingleton();
		const auto& globalSettings = dataPersistence->GetGlobalSettings();
		if (globalSettings.selectedLanguage >= 0 && 
			globalSettings.selectedLanguage < static_cast<int>(Language::COUNT)) {
			Language savedLanguage = static_cast<Language>(globalSettings.selectedLanguage);
			localization->SetLanguage(savedLanguage);

		}

		m_Initialized = true;

		return true;
	}

	void ImGuiManager::Shutdown()
	{
		if (!m_Initialized)
			return;

		ShutdownPanels();

		// 关闭后端
		try {
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();

		} catch (...) {
			logger::warn("Error during ImGui backend shutdown, possibly shared with other mods");
		}

		// 只有当我们创建了ImGui上下文时才销毁它
		if (m_CreatedImGuiContext) {
			ImGui::DestroyContext();
			m_CreatedImGuiContext = false;

		} else {

		}

		m_Initialized = false;

	}

	void ImGuiManager::InitializePanels()
	{
		// 创建面板实例
		m_CameraAdjustmentPanel = std::make_unique<CameraAdjustmentPanel>(this);
		m_ModelSwitcherPanel = std::make_unique<ModelSwitcherPanel>(this);
		m_ZoomDataPanel = std::make_unique<ZoomDataPanel>(this);
		m_DebugPanel = std::make_unique<DebugPanel>(this);
		m_PostProcessPanel = std::make_unique<PostProcessPanel>(this);
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
			static_cast<BasePanelInterface*>(m_PostProcessPanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_SettingsPanel.get())));
		m_Panels.push_back(std::unique_ptr<BasePanelInterface>(
			static_cast<BasePanelInterface*>(m_DebugPanel.get())));


		// 初始化所有面板
		for (auto& panel : m_Panels) {
			if (!panel->Initialize()) {
				logger::warn("Failed to initialize panel: {}", panel->GetPanelName());
			}
		}


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
		// 处理字体重建请求（在渲染循环外）
		if (m_FontRebuildRequested) {
			m_FontRebuildRequested = false;
			RebuildFonts();
			
			// 获取本地化文本并设置状态消息
			auto locManager = LocalizationManager::GetSingleton();
			if (locManager && locManager->IsInitialized()) {
				SetDebugText(locManager->GetText("status.font_rebuilt"));
			} else {
				SetDebugText("Font rebuilt for language support");
			}
		}
		

		bool UIOpen = RE::UI::GetSingleton()->GetMenuOpen("PauseMenu") || RE::UI::GetSingleton()->GetMenuOpen("WorkshopMenu") || RE::UI::GetSingleton()->GetMenuOpen("CursorMenu");
		
		// 从DataPersistence获取菜单键设置
		auto dataPersistence = DataPersistence::GetSingleton();
		const auto& globalSettings = dataPersistence->GetGlobalSettings();
		int menuKey = globalSettings.menuKeyBindings[0];  // 获取菜单键的VK码
		
		// 处理菜单切换 - 使用设置中配置的键
		if (!UIOpen && menuKey != 0 && (GetAsyncKeyState(menuKey) & 0x1))
		{
			ToggleMenu();
			auto mc = RE::MenuCursor::GetSingleton();
			REX::W32::ShowCursor(m_MenuOpen);

			auto input = RE::BSInputDeviceManager::GetSingleton();
			RE::ControlMap::GetSingleton()->ignoreKeyboardMouse = m_MenuOpen;


		}

		for (auto& panel : m_Panels) {
			panel->UpdateOutSideUI();
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
		bool saveResult = true;
		for (auto& i : m_Panels)
		{
			saveResult &= i->GetSaved();
		}

		if (!saveResult) {
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
			ImGui::TextColored(m_SuccessColor, LOC("status.WeaponLoaded"));
		} else {
			ImGui::TextColored(m_WarningColor, LOC("status.WeaponNotLoaded"));
		}

		ImGui::NextColumn();

		// 中间：TTSNode状态
		auto ttsNode = GetTTSNode();
		if (ttsNode) {
			ImGui::TextColored(m_SuccessColor, LOC("status.TTSNodeReady"));
		} else {
			ImGui::TextColored(m_WarningColor, LOC("status.TTSNodeNotReady"));
		}

		ImGui::NextColumn();

		bool saveResult = true;
		for (auto& i : m_Panels) {
			saveResult &= i->GetSaved();
		}
		if (!saveResult) {
			ImGui::TextColored(m_WarningColor, LOC("status.unsaved_changes"));
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), LOC("status.all_saved"));
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

	void ImGuiManager::LoadLanguageFonts()
	{
		ImGuiIO& io = ImGui::GetIO();
		
		// 清除现有字体
		io.Fonts->Clear();
		
		// 字体基础路径
		std::string fontBasePath = "Data/F4SE/Plugins/TrueThroughScope/Languages/Fonts/";
		
		// 创建字体配置
		ImFontConfig fontConfig;
		fontConfig.MergeMode = false;  // 第一个字体不使用合并模式
		fontConfig.PixelSnapH = true;
		
		// 创建扩展的字符范围组合（包含所有需要的字符）
		static const ImWchar ranges[] =
		{
			0x0020, 0x00FF, // 基本拉丁文 + 拉丁文补充
			0x0100, 0x017F, // 拉丁文扩展-A
			0x0400, 0x04FF, // 西里尔文（俄语）
			0x2000, 0x206F, // 一般标点符号
			0x2190, 0x21FF, // 箭头符号
			0x2200, 0x22FF, // 数学运算符
			0x2300, 0x23FF, // 杂项技术符号
			0x2400, 0x243F, // 控制图符
			0x2500, 0x257F, // 制表符
			0x2580, 0x259F, // 方块元素
			0x25A0, 0x25FF, // 几何图形（包含●○等符号）
			0x2600, 0x26FF, // 杂项符号（包含⚠等符号）
			0x2700, 0x27BF, // 装饰符号（包含✓等符号）
			0x3000, 0x30FF, // CJK符号和标点、平假名、片假名
			0x31F0, 0x31FF, // 片假名语音扩展
			0x4E00, 0x9FAF, // CJK统一汉字
			0xAC00, 0xD7AF, // 韩文音节
			0xFF00, 0xFFEF, // 半角及全角形式
			0,
		};
		
		// 首先尝试加载一个支持多种语言的字体作为基础
		ImFont* baseFont = nullptr;
		
		// 尝试加载系统字体作为基础（这些字体通常支持多种语言）
		std::vector<std::string> systemFonts = {
			"C:/Windows/Fonts/msyh.ttc",     // 微软雅黑（支持中文）
			"C:/Windows/Fonts/segoeui.ttf",  // Segoe UI（支持更多符号）
			"C:/Windows/Fonts/NotoSansCJK.ttc", // Noto Sans CJK（如果安装了）
			"C:/Windows/Fonts/arial.ttf"    // Arial作为最后的备选
		};
		
		for (const auto& systemFont : systemFonts) {
			if (std::filesystem::exists(systemFont)) {
				baseFont = io.Fonts->AddFontFromFileTTF(systemFont.c_str(), 16.0f, &fontConfig, ranges);
				if (baseFont) {

					break;
				}
			}
		}
		
		// 如果没有找到合适的系统字体，使用默认字体
		if (!baseFont) {
			baseFont = io.Fonts->AddFontDefault(&fontConfig);
		}
		
		// 然后合并其他特定语言字体以补充缺失的字符
		fontConfig.MergeMode = true;  // 之后的字体都使用合并模式
		
		// 合并简体中文字体
		std::string chineseFontPath = fontBasePath + "sc.ttc";
		if (std::filesystem::exists(chineseFontPath)) {
			io.Fonts->AddFontFromFileTTF(chineseFontPath.c_str(), 16.0f, &fontConfig, 
				io.Fonts->GetGlyphRangesChineseFull());
		}

		// 合并繁体中文字体
		std::string chineseTraditionFontPath = fontBasePath + "tc.ttc";
		if (std::filesystem::exists(chineseTraditionFontPath)) {
			io.Fonts->AddFontFromFileTTF(chineseTraditionFontPath.c_str(), 16.0f, &fontConfig,
				io.Fonts->GetGlyphRangesChineseFull());
		}
		
		// 合并日语字体
		std::string japaneseFontPath = fontBasePath + "jp.ttc";
		if (std::filesystem::exists(japaneseFontPath)) {
			io.Fonts->AddFontFromFileTTF(japaneseFontPath.c_str(), 16.0f, &fontConfig, 
				io.Fonts->GetGlyphRangesJapanese());
		}
		
		// 合并韩语字体
		std::string koreanFontPath = fontBasePath + "ko.ttf";
		if (std::filesystem::exists(koreanFontPath)) {
			io.Fonts->AddFontFromFileTTF(koreanFontPath.c_str(), 16.0f, &fontConfig, 
				io.Fonts->GetGlyphRangesKorean());
		}
		
		// 合并俄语字体（西里尔字符）
		std::string russianFontPath = fontBasePath + "calibri.ttf";
		if (std::filesystem::exists(russianFontPath)) {
			io.Fonts->AddFontFromFileTTF(russianFontPath.c_str(), 16.0f, &fontConfig, 
				io.Fonts->GetGlyphRangesCyrillic());
		}
		
		// 添加符号字体支持
		std::vector<std::string> symbolFonts = {
			"C:/Windows/Fonts/seguisym.ttf",  // Segoe UI Symbol
			"C:/Windows/Fonts/arial.ttf",     // Arial也有一些基本符号
			"C:/Windows/Fonts/calibri.ttf"    // Calibri作为备选
		};
		
		static const ImWchar symbolRanges[] = {
			0x2000, 0x27BF, // 所有符号范围
			0xE000, 0xF8FF, // 私人使用区域（一些字体的特殊符号）
			0,
		};
		
		for (const auto& symbolFont : symbolFonts) {
			if (std::filesystem::exists(symbolFont)) {
				io.Fonts->AddFontFromFileTTF(symbolFont.c_str(), 16.0f, &fontConfig, symbolRanges);

				break; // 只需要一个符号字体即可
			}
		}
		
		// 构建字体图集
		io.Fonts->Build();
		
		// 设置默认字体为合并后的字体
		io.FontDefault = baseFont;
		
		// 检查关键符号是否可用（调试用）

		

	}
	
	void ImGuiManager::UpdateFontsForLanguage(Language language)
	{
		// 现在所有语言都使用同一个合并字体，包含所有字符集
		// 所以不需要切换字体，所有语言的字符都能正确显示
		ImGuiIO& io = ImGui::GetIO();
		
		// 确保使用合并后的字体（通常是第一个字体）
		if (io.Fonts->Fonts.Size > 0) {
			io.FontDefault = io.Fonts->Fonts[0];
		}
		

	}
	
	void ImGuiManager::RebuildFonts()
	{
		if (!m_Initialized) {
			return;
		}
		
		// 重新加载字体
		LoadLanguageFonts();
		
		// 通知ImGui后端字体已重建
		ImGui_ImplDX11_InvalidateDeviceObjects();
		ImGui_ImplDX11_CreateDeviceObjects();
		

	}
	
	void ImGuiManager::RequestFontRebuild()
	{
		m_FontRebuildRequested = true;

	}

}
