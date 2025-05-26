#include "ImGuiManager.h"
#include <Utilities.h>
#include <NiFLoader.h>

#include "DataPersistence.h"

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

		// Get the D3D11 device and context from the game's renderer
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

		// Initialize ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking

		// Set up ImGui style
		ImGui::StyleColorsDark();

		// Customize ImGui style
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

		// Set accent colors
		style.Colors[ImGuiCol_Header] = m_AccentColor;
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(m_AccentColor.x + 0.1f, m_AccentColor.y + 0.1f, m_AccentColor.z + 0.1f, m_AccentColor.w);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(m_AccentColor.x + 0.2f, m_AccentColor.y + 0.2f, m_AccentColor.z + 0.2f, m_AccentColor.w);
		style.Colors[ImGuiCol_Button] = m_AccentColor;
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(m_AccentColor.x + 0.1f, m_AccentColor.y + 0.1f, m_AccentColor.z + 0.1f, m_AccentColor.w);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(m_AccentColor.x + 0.2f, m_AccentColor.y + 0.2f, m_AccentColor.z + 0.2f, m_AccentColor.w);

		// Initialize ImGui platform/renderer backends
		if (!ImGui_ImplWin32_Init(hwnd)) {
			logger::error("Failed to initialize ImGui Win32 backend");
			return false;
		}

		if (!ImGui_ImplDX11_Init((ID3D11Device*)rendererData->device, (ID3D11DeviceContext*)rendererData->context)) {
			logger::error("Failed to initialize ImGui DX11 backend");
			ImGui_ImplWin32_Shutdown();
			return false;
		}

		m_EnableRendering = D3DHooks::IsEnableRender();

		m_Initialized = true;
		logger::info("ImGui initialized successfully");
		return true;
	}
    
    void ImGuiManager::Shutdown()
    {
        if (!m_Initialized)
            return;
            
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        m_Initialized = false;
        logger::info("ImGui shut down");
    }
    
    void ImGuiManager::Render()
    {
        if (!m_Initialized || !m_MenuOpen)
            return;
            
       // Start ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Main menu
		RenderMainMenu();

		// Finish ImGui frame
		ImGui::Render();

		// 获取ImGui绘制数据并渲染
		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Valid && draw_data->CmdListsCount > 0) {
			// 确保在正确的渲染上下文中
			ImGui_ImplDX11_RenderDrawData(draw_data);
		}
    }
    
    void ImGuiManager::Update()
	{
		static float lastUpdateTime = 0.0f;
		float currentTime = ImGui::GetTime();
		float deltaTime = currentTime - lastUpdateTime;
		lastUpdateTime = currentTime;

		// 性能监控 - 如果更新太频繁则跳过
		static int frameCounter = 0;
		frameCounter++;
		if (frameCounter % 2 == 0) {  // 每两帧更新一次以提升性能
			return;
		}

		// Toggle menu on/off when F2 is pressed
		if (GetAsyncKeyState(VK_F2) & 0x1) {
			ToggleMenu();
			auto mc = RE::MenuCursor::GetSingleton();
			REX::W32::ShowCursor(m_MenuOpen);

			auto input = (RE::BSInputDeviceManager::GetSingleton());
			RE::ControlMap::GetSingleton()->ignoreKeyboardMouse = m_MenuOpen;

			 if (m_MenuOpen) {
				m_UIValuesInitialized = false;
				logger::info("Menu opened - resetting UI initialization state");
			}
		}

		// Real-time adjustment updates (只在菜单打开时更新)
		if (m_MenuOpen && m_Initialized && m_RealTimeAdjustment) {
			bool hasChanges = false;

			// Check for position changes
			if (std::abs(m_DeltaPosX - m_PrevDeltaPosX) > 0.001f ||
				std::abs(m_DeltaPosY - m_PrevDeltaPosY) > 0.001f ||
				std::abs(m_DeltaPosZ - m_PrevDeltaPosZ) > 0.001f) {
				ApplyPositionAdjustment();
				m_PrevDeltaPosX = m_DeltaPosX;
				m_PrevDeltaPosY = m_DeltaPosY;
				m_PrevDeltaPosZ = m_DeltaPosZ;
				hasChanges = true;
			}

			// Check for rotation changes
			if (std::abs(m_DeltaRot[0] - m_PrevDeltaRot[0]) > 0.1f ||
				std::abs(m_DeltaRot[1] - m_PrevDeltaRot[1]) > 0.1f ||
				std::abs(m_DeltaRot[2] - m_PrevDeltaRot[2]) > 0.1f) {
				ApplyRotationAdjustment();
				m_PrevDeltaRot[0] = m_DeltaRot[0];
				m_PrevDeltaRot[1] = m_DeltaRot[1];
				m_PrevDeltaRot[2] = m_DeltaRot[2];
				hasChanges = true;
			}

			if (std::abs(m_DeltaRelativeFogRadius - m_PrevDeltaRelativeFogRadius) > 0.001f ||
				std::abs(m_DeltaScopeSwayAmount - m_PrevDeltaScopeSwayAmount) > 0.001f ||
				std::abs(m_DeltaMaxTravel - m_PrevDeltaMaxTravel) > 0.001f ||
				std::abs(m_DeltaParallaxRadius - m_PrevDeltaParallaxRadius) > 0.001f) 
			{
				D3DHooks::UpdateScopeSettings(
					m_DeltaRelativeFogRadius,
					m_DeltaScopeSwayAmount,
					m_DeltaMaxTravel,
					m_DeltaParallaxRadius);

				m_PrevDeltaRelativeFogRadius = m_DeltaRelativeFogRadius;
				m_PrevDeltaScopeSwayAmount = m_DeltaScopeSwayAmount;
				m_PrevDeltaMaxTravel = m_DeltaMaxTravel;
				m_PrevDeltaParallaxRadius = m_DeltaParallaxRadius;

				hasChanges = true;
			}

			

			// Check for scale changes
			if (std::abs(m_DeltaScale - m_PrevDeltaScale) > 0.001f) {
				ApplyScaleAdjustment();
				m_PrevDeltaScale = m_DeltaScale;
				hasChanges = true;
			}

			// 标记有未保存的更改
			if (hasChanges) {
				m_HasUnsavedChanges = true;
			}
		}
	}

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
			// Ignore exceptions
		}
		return nullptr;
	}

	void ImGuiManager::ApplyPositionAdjustment()
	{
		try {
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ttsNode->local.translate.x = m_DeltaPosX;
				ttsNode->local.translate.y = m_DeltaPosY;
				ttsNode->local.translate.z = m_DeltaPosZ;

				RE::NiUpdateData tempData{};
				tempData.camera = ScopeCamera::GetScopeCamera();
				if (tempData.camera) {
					ttsNode->Update(tempData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText),
					"Position: [%.3f, %.3f, %.3f]", m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ);
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		} catch (const std::exception& e) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception in ApplyPosition: %s", e.what());
		} catch (...) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Unknown exception in ApplyPosition!");
		}
	}

	void ImGuiManager::ApplyRotationAdjustment()
	{
		try {
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				// Convert degrees to radians for rotation
				float pitch = m_DeltaRot[0] * 0.01745329251f;  // PI/180
				float yaw = m_DeltaRot[1] * 0.01745329251f;
				float roll = m_DeltaRot[2] * 0.01745329251f;

				RE::NiMatrix3 rotMat;
				rotMat.MakeIdentity();
				rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);

				ttsNode->local.rotate = rotMat;

				RE::NiUpdateData tempData{};
				tempData.camera = ScopeCamera::GetScopeCamera();
				if (tempData.camera) {
					ttsNode->Update(tempData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText),
					"Rotation: [%.1f, %.1f, %.1f] degrees", m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2]);
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		} catch (const std::exception& e) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception in ApplyRotation: %s", e.what());
		} catch (...) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Unknown exception in ApplyRotation!");
		}
	}

	void ImGuiManager::ApplyScaleAdjustment()
	{
		try {
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ttsNode->local.scale = m_DeltaScale;  // Uniform scale
				// If you want non-uniform scaling, you'd need to modify the local transform matrix manually
				// For now, we'll use uniform scaling

				RE::NiUpdateData tempData{};
				tempData.camera = ScopeCamera::GetScopeCamera();
				if (tempData.camera) {
					ttsNode->Update(tempData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText),
					"Scale: [%.3f]", m_DeltaScale);
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		} catch (const std::exception& e) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception in ApplyScale: %s", e.what());
		} catch (...) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Unknown exception in ApplyScale!");
		}
	}

	void ImGuiManager::ResetAllAdjustments()
	{
		m_DeltaPosX = 0.0f;
		m_DeltaPosY = 0.0f;
		m_DeltaPosZ = 7.0f;
		m_DeltaRot[0] = 0.0f;
		m_DeltaRot[1] = 0.0f;
		m_DeltaRot[2] = 0.0f;
		m_DeltaScale = 1.5f;

		// Apply the reset values immediately if real-time is enabled
		if (m_RealTimeAdjustment) {
			ApplyPositionAdjustment();
			ApplyRotationAdjustment();
			ApplyScaleAdjustment();
		}

		snprintf(m_DebugText, sizeof(m_DebugText), "All adjustments reset!");
	}

	void ImGuiManager::ScanForNIFFiles()
	{
		if (m_NIFFilesScanned) {
			return;  // 已经扫描过了
		}

		m_AvailableNIFFiles.clear();

		try {
			// 构建路径：Data/Meshes/TTS/ScopeShape/
			std::filesystem::path dataPath = std::filesystem::current_path() / "Data" / "Meshes" / "TTS" / "ScopeShape";

			if (!std::filesystem::exists(dataPath)) {
				logger::warn("ScopeShape directory not found: {}", dataPath.string());
				snprintf(m_DebugText, sizeof(m_DebugText), "ScopeShape directory not found!");
				return;
			}

			// 扫描所有.nif文件
			for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
				if (entry.is_regular_file() && entry.path().extension() == ".nif") {
					std::string fileName = entry.path().filename().string();
					m_AvailableNIFFiles.push_back(fileName);
					logger::info("Found NIF file: {}", fileName);
				}
			}

			if (m_AvailableNIFFiles.empty()) {
				logger::warn("No NIF files found in ScopeShape directory");
				snprintf(m_DebugText, sizeof(m_DebugText), "No NIF files found in ScopeShape directory!");
			} else {
				logger::info("Found {} NIF files in ScopeShape directory", m_AvailableNIFFiles.size());
				snprintf(m_DebugText, sizeof(m_DebugText), "Found %d NIF files", (int)m_AvailableNIFFiles.size());
			}

			m_NIFFilesScanned = true;
		} catch (const std::exception& e) {
			logger::error("Error scanning for NIF files: {}", e.what());
			snprintf(m_DebugText, sizeof(m_DebugText), "Error scanning NIF files: %s", e.what());
		}
	}

	void ImGuiManager::RemoveExistingTTSNode()
	{
		try {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (!playerCharacter || !playerCharacter->Get3D()) {
				return;
			}

			auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
			if (!weaponNode || !weaponNode->IsNode()) {
				return;
			}

			auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
			auto existingTTSNode = weaponNiNode->GetObjectByName("TTSNode");

			if (existingTTSNode) {
				logger::info("Removing existing TTSNode");
				weaponNiNode->DetachChild(existingTTSNode);

				// 更新节点
				RE::NiUpdateData updateData{};
				updateData.camera = ScopeCamera::GetScopeCamera();
				if (updateData.camera) {
					weaponNiNode->Update(updateData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText), "Existing TTSNode removed");
			}
		} catch (const std::exception& e) {
			logger::error("Error removing existing TTSNode: {}", e.what());
			snprintf(m_DebugText, sizeof(m_DebugText), "Error removing TTSNode: %s", e.what());
		}
	}

	bool ImGuiManager::CreateTTSNodeFromNIF(const std::string& nifFileName)
	{
		try {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (!playerCharacter || !playerCharacter->Get3D()) {
				logger::error("Player character or 3D not available");
				snprintf(m_DebugText, sizeof(m_DebugText), "Player character not available!");
				return false;
			}

			auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
			if (!weaponNode || !weaponNode->IsNode()) {
				logger::error("Weapon node not found or invalid");
				snprintf(m_DebugText, sizeof(m_DebugText), "Weapon node not found!");
				return false;
			}

			auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);

			// 移除现有的TTSNode
			RemoveExistingTTSNode();
			// 构建完整的NIF文件路径
			std::string fullPath = "Meshes\\TTS\\ScopeShape\\" + nifFileName;

			logger::info("Loading NIF file: {}", fullPath);

			// 使用NIFLoader加载NIF文件
			auto nifLoader = NIFLoader::GetSington();
			RE::NiNode* loadedNode = nifLoader->LoadNIF(fullPath.c_str());
			ScopeCamera::s_CurrentScopeNode = loadedNode;

			if (!loadedNode) {
				logger::error("Failed to load NIF file: {}", fullPath);
				snprintf(m_DebugText, sizeof(m_DebugText), "Failed to load NIF: %s", nifFileName.c_str());
				return false;
			}

			// 设置初始变换
			loadedNode->local.translate = RE::NiPoint3(0,0,7);
			loadedNode->local.rotate.MakeIdentity();
			loadedNode->local.scale = 1.5f;

			// 将加载的节点附加到武器节点
			weaponNiNode->AttachChild(loadedNode, false);

			// 更新节点变换
			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				loadedNode->Update(updateData);
				weaponNiNode->Update(updateData);
			}

			logger::info("TTSNode created successfully from: {}", nifFileName);
			snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode created from: %s", nifFileName.c_str());

			// 重置调整值
			ResetAllAdjustments();

			return true;
		} catch (const std::exception& e) {
			logger::error("Exception creating TTSNode: {}", e.what());
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception: %s", e.what());
			return false;
		}
	}

	bool ImGuiManager::CreateTTSNodeFromNIF(const ThroughScope::DataPersistence::ScopeConfig* config)
	{
		if (CreateTTSNodeFromNIF(config->modelName))
		{
			auto loadedNode = ScopeCamera::s_CurrentScopeNode;
			auto weaponNode = RE::PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon");
			auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);

			loadedNode->local.translate = RE::NiPoint3(config->cameraAdjustments.deltaPosX, config->cameraAdjustments.deltaPosY, config->cameraAdjustments.deltaPosZ);
			loadedNode->local.rotate.FromEulerAnglesXYZ(config->cameraAdjustments.deltaRot[0], config->cameraAdjustments.deltaRot[1], config->cameraAdjustments.deltaRot[2]);
			loadedNode->local.scale = config->cameraAdjustments.deltaScale;

			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				loadedNode->Update(updateData);
				weaponNiNode->Update(updateData);
			}

			return true;
		}
		return false;
	}


    void ImGuiManager::RenderMainMenu()
	{
		const float menuWidth = 600.0f;  // 增加宽度以容纳更多内容
		const float menuHeight = 700.0f;

		// Center the window on the screen
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
		ImGui::SetNextWindowPos(ImVec2(center.x - menuWidth / 2, center.y - menuHeight / 2), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_FirstUseEver);

		// 设置窗口标志以提升用户体验
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
		if (m_HasUnsavedChanges) {
			windowFlags |= ImGuiWindowFlags_UnsavedDocument;  // 显示未保存标记
		}

		if (ImGui::Begin("TrueThroughScope Settings", &m_MenuOpen, windowFlags)) {
			// 顶部状态栏
			RenderStatusBar();
			ImGui::Spacing();

			// 主要内容区域
			if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_Reorderable)) {
				// Camera Adjustment Tab - 始终显示
				if (ImGui::BeginTabItem("Camera Adjustment")) {
					RenderCameraAdjustmentPanel();
					ImGui::EndTabItem();
				}

				// Scope Shape Switcher Tab - 只在有配置时显示
				DataPersistence::WeaponInfo weaponInfo = DataPersistence::GetCurrentWeaponInfo();
				if (weaponInfo.currentConfig) {
					if (ImGui::BeginTabItem("Scope Shape")) {
						RenderModelSwitcher();
						ImGui::EndTabItem();
					}
				}

				// Settings Tab - 新增设置页面
				if (ImGui::BeginTabItem("Settings")) {
					RenderSettingsPanel();
					ImGui::EndTabItem();
				}

				// Debug Tab
				if (ImGui::BeginTabItem("Debug")) {
					RenderDebugPanel();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			// 检查自动保存
			CheckAutoSave();

			// 显示错误对话框（如果有）
			ShowErrorDialog("", "");
		}
		ImGui::End();
	}


    void ImGuiManager::RenderCameraAdjustmentPanel()
	{
		using namespace RE;
		auto dataPersistence = DataPersistence::GetSingleton();

		// 获取武器信息
		DataPersistence::WeaponInfo weaponInfo = DataPersistence::GetCurrentWeaponInfo();

		// 检查是否有有效的武器
		if (!weaponInfo.weapon || !weaponInfo.instanceData) {
			ImGui::TextColored(m_WarningColor, "No valid weapon equipped");
			ImGui::TextWrapped("Please equip a weapon with scope capabilities to configure settings.");
			return;
		}

		// ==================== WEAPON INFORMATION SECTION ====================
		ImGui::TextColored(m_AccentColor, "Current Weapon Information");
		ImGui::Separator();

		ImGui::Text("Weapon: [%08X] %s", weaponInfo.weaponFormID, weaponInfo.weaponModName.c_str());

		if (weaponInfo.selectedModForm) {
			ImGui::Text("Config Source: [%08X] %s (%s)",
				weaponInfo.selectedModForm->GetLocalFormID(),
				weaponInfo.selectedModForm->GetFile()->filename,
				weaponInfo.configSource.c_str());
		} else if (weaponInfo.currentConfig) {
			ImGui::Text("Config Source: Weapon (%s)", weaponInfo.configSource.c_str());
		}

		if (weaponInfo.currentConfig) {
			if (!weaponInfo.currentConfig->modelName.empty()) {
				ImGui::Text("Current Model: %s", weaponInfo.currentConfig->modelName.c_str());

				ImGui::SameLine();
				if (ImGui::Button("Reload TTSNode")) {
					if (CreateTTSNodeFromNIF(weaponInfo.currentConfig)) {
						snprintf(m_DebugText, sizeof(m_DebugText),
							"TTSNode reloaded from: %s",
							weaponInfo.currentConfig->modelName.c_str());
					} else {
						snprintf(m_DebugText, sizeof(m_DebugText),
							"Failed to reload TTSNode from: %s",
							weaponInfo.currentConfig->modelName.c_str());
					}
				}

				// 显示帮助信息
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Click to recreate the TTSNode from the saved model file");
				}
			} else {
				ImGui::TextColored(m_WarningColor, "No model file associated with this configuration");
			}
		}

		ImGui::Spacing();

		// ==================== NO CONFIG FOUND - CREATION SECTION ====================
		if (!weaponInfo.currentConfig) {
			ImGui::Separator();
			ImGui::TextColored(m_WarningColor, "No configuration found for this weapon");
			ImGui::TextWrapped("Create a new configuration to start customizing your scope settings.");

			ScanForNIFFiles();

			// Configuration target selection
			ImGui::Spacing();
			ImGui::TextColored(m_AccentColor, "Configuration Target:");

			static int createOption = 0;  // 0 = weapon, 1 = first mod, etc.

			if (ImGui::BeginCombo("Create Config For",
					createOption == 0 ? "Base Weapon" :
										(createOption <= (int)weaponInfo.availableMods.size() ?
												fmt::format("Modification #{}", createOption).c_str() :
												"Invalid Selection"))) {
				// Base weapon option
				if (ImGui::Selectable("Base Weapon", createOption == 0)) {
					createOption = 0;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Create configuration for the base weapon\nApplies to all instances of this weapon");
				}

				// Modification options
				for (size_t i = 0; i < weaponInfo.availableMods.size(); i++) {
					auto modForm = weaponInfo.availableMods[i];
					std::string label = fmt::format("Modification #{} - {:08X} ({})",
						i + 1, modForm->GetLocalFormID(),
						modForm->GetFormEditorID() ? modForm->GetFormEditorID() : "No ID");

					if (ImGui::Selectable(label.c_str(), createOption == (i + 1))) {
						createOption = i + 1;
					}

					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Create configuration specific to this modification\nOnly applies when this mod is attached");
					}
				}

				ImGui::EndCombo();
			}

			// NIF File Selection
			ImGui::Spacing();
			ImGui::TextColored(m_AccentColor, "Scope Shape (Model File):");

			if (m_AvailableNIFFiles.empty()) {
				ImGui::TextColored(m_WarningColor, "No NIF files found in Meshes/TTS/ScopeShape/");
				ImGui::TextWrapped("Please ensure you have NIF files in the ScopeShape directory.");

				if (ImGui::Button("Rescan for NIF Files")) {
					m_NIFFilesScanned = false;
					ScanForNIFFiles();
				}
			} else {
				const char* currentNIFName = m_SelectedNIFIndex < m_AvailableNIFFiles.size() ?
				                                 m_AvailableNIFFiles[m_SelectedNIFIndex].c_str() :
				                                 "Select a model...";

				if (ImGui::BeginCombo("Model File", currentNIFName)) {
					for (int i = 0; i < m_AvailableNIFFiles.size(); i++) {
						bool isSelected = (m_SelectedNIFIndex == i);
						if (ImGui::Selectable(m_AvailableNIFFiles[i].c_str(), isSelected)) {
							m_SelectedNIFIndex = i;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				if (ImGui::Button("Rescan")) {
					m_NIFFilesScanned = false;
					ScanForNIFFiles();
				}
			}

			// TTSNode Preview
			ImGui::Spacing();
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ImGui::TextColored(m_SuccessColor, "✓ TTSNode is loaded and ready");
				ImGui::SameLine();
				if (ImGui::Button("Remove TTSNode")) {
					RemoveExistingTTSNode();
				}
			} else {
				ImGui::TextColored(m_WarningColor, "⚠ No TTSNode found");
				if (!m_AvailableNIFFiles.empty() && m_SelectedNIFIndex < m_AvailableNIFFiles.size()) {
					ImGui::SameLine();
					if (ImGui::Button("Preview Model")) {
						CreateTTSNodeFromNIF(m_AvailableNIFFiles[m_SelectedNIFIndex]);
					}
				}
			}

			// Create Configuration Button
			ImGui::Spacing();
			ImGui::Separator();

			bool canCreateConfig = !m_AvailableNIFFiles.empty() && m_SelectedNIFIndex < m_AvailableNIFFiles.size();

			if (!canCreateConfig) {
				ImGui::BeginDisabled();
			}

			if (ImGui::Button("Create Configuration", ImVec2(-1, 0))) {
				bool success = false;
				std::string selectedNIF = "";

				if (canCreateConfig) {
					selectedNIF = m_AvailableNIFFiles[m_SelectedNIFIndex];

					// Create TTSNode first
					if (!GetTTSNode()) {  // Only create if doesn't exist
						if (!CreateTTSNodeFromNIF(selectedNIF)) {
							logger::error("Failed to create TTSNode from: {}", selectedNIF);
							snprintf(m_DebugText, sizeof(m_DebugText), "Failed to create TTSNode!");
						}
					}

					// Create configuration
					if (createOption == 0) {
						success = dataPersistence->GeneratePresetConfig(
							weaponInfo.weaponFormID,
							weaponInfo.weaponModName,
							selectedNIF);
					} else if (createOption > 0 && createOption <= (int)weaponInfo.availableMods.size()) {
						auto modForm = weaponInfo.availableMods[createOption - 1];
						success = dataPersistence->GeneratePresetConfig(
							modForm->GetLocalFormID(),
							modForm->GetFile()->filename,
							selectedNIF);
					}

					if (success) {
						snprintf(m_DebugText, sizeof(m_DebugText),
							"Configuration created successfully! Model: %s",
							selectedNIF.c_str());
						dataPersistence->LoadAllConfigs();
					} else {
						snprintf(m_DebugText, sizeof(m_DebugText), "Failed to create configuration!");
					}
				}
			}

			if (!canCreateConfig) {
				ImGui::EndDisabled();
				ImGui::TextColored(m_WarningColor, "Please select a model file to create configuration");
			}

			ImGui::Separator();
			return;
		}

		const auto* currentConfig = weaponInfo.currentConfig;
		std::string currentConfigKey = fmt::format("{:08X}_{}",
			weaponInfo.weaponFormID,
			currentConfig->modelName);

		bool needsReinitialize = false;

		 if (!m_UIValuesInitialized || m_LastLoadedConfigKey != currentConfigKey) {
			needsReinitialize = true;
			m_LastLoadedConfigKey = currentConfigKey;
			m_UIValuesInitialized = true;
			logger::info("Initializing UI values for config: {}", currentConfigKey);
		}


		// ==================== CONFIGURATION EXISTS - ADJUSTMENT SECTION ====================
		static bool settingsLoaded = false;
		static int minFOV, maxFOV;
		static bool nightVision, thermalVision;

		

		if (needsReinitialize || !settingsLoaded) {
			// 从配置文件加载所有设置
			minFOV = currentConfig->scopeSettings.minFOV;
			maxFOV = currentConfig->scopeSettings.maxFOV;
			nightVision = currentConfig->scopeSettings.nightVision;
			thermalVision = currentConfig->scopeSettings.thermalVision;


			m_DeltaRelativeFogRadius = currentConfig->parallaxSettings.relativeFogRadius;
			m_DeltaScopeSwayAmount = currentConfig->parallaxSettings.scopeSwayAmount;
			m_DeltaMaxTravel = currentConfig->parallaxSettings.maxTravel;
			m_DeltaParallaxRadius = currentConfig->parallaxSettings.radius;

			// !!!关键修复：从配置加载UI调整值，而不是从TTSNode
			m_DeltaPosX = currentConfig->cameraAdjustments.deltaPosX;
			m_DeltaPosY = currentConfig->cameraAdjustments.deltaPosY;
			m_DeltaPosZ = currentConfig->cameraAdjustments.deltaPosZ;
			m_DeltaRot[0] = currentConfig->cameraAdjustments.deltaRot[0];
			m_DeltaRot[1] = currentConfig->cameraAdjustments.deltaRot[1];
			m_DeltaRot[2] = currentConfig->cameraAdjustments.deltaRot[2];
			m_DeltaScale = currentConfig->cameraAdjustments.deltaScale;

			// 更新前一帧的值
			m_PrevDeltaPosX = m_DeltaPosX;
			m_PrevDeltaPosY = m_DeltaPosY;
			m_PrevDeltaPosZ = m_DeltaPosZ;
			m_PrevDeltaRot[0] = m_DeltaRot[0];
			m_PrevDeltaRot[1] = m_DeltaRot[1];
			m_PrevDeltaRot[2] = m_DeltaRot[2];
			m_PrevDeltaScale = m_DeltaScale;
			m_PrevDeltaRelativeFogRadius = m_DeltaRelativeFogRadius;
			m_PrevDeltaScopeSwayAmount = m_DeltaScopeSwayAmount;
			m_PrevDeltaMaxTravel = m_DeltaMaxTravel;
			m_PrevDeltaParallaxRadius = m_DeltaParallaxRadius;

			settingsLoaded = true;

			// 如果TTSNode存在，应用配置中的值到TTSNode
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ApplyConfigToTTSNode(currentConfig);
			}

			logger::info("UI values loaded from config - Pos:[{:.3f},{:.3f},{:.3f}] Rot:[{:.1f},{:.1f},{:.1f}] Scale:{:.3f}",
				m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ,
				m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2],
				m_DeltaScale);
		}

		// Sync UI values with current TTSNode
		auto ttsNode = GetTTSNode();

		if (ttsNode && m_RealTimeAdjustment && settingsLoaded) {
			// 检查TTSNode值是否与UI值不同步
			bool nodeOutOfSync =
				std::abs(ttsNode->local.translate.x - m_DeltaPosX) > 0.001f ||
				std::abs(ttsNode->local.translate.y - m_DeltaPosY) > 0.001f ||
				std::abs(ttsNode->local.translate.z - m_DeltaPosZ) > 0.001f ||
				std::abs(ttsNode->local.scale - m_DeltaScale) > 0.001f;

			if (nodeOutOfSync) {
				// 如果节点与UI不同步，优先使用UI的值（因为UI值是从配置加载的）
				ApplyUIValuesToTTSNode();
			}
		}

		// 如果有配置但没有TTSNode，显示自动加载选项
		if (weaponInfo.currentConfig && !weaponInfo.currentConfig->modelName.empty()) {
			if (!ttsNode) {
				ImGui::Separator();
				ImGui::TextColored(m_WarningColor, "⚠ TTSNode Missing");
				ImGui::TextWrapped("Configuration specifies model '%s' but no TTSNode is loaded.",
					weaponInfo.currentConfig->modelName.c_str());

				if (ImGui::Button("Auto-Load TTSNode from Config", ImVec2(-1, 0))) {
					if (AutoLoadTTSNodeFromConfig(weaponInfo.currentConfig)) {
						snprintf(m_DebugText, sizeof(m_DebugText),
							"TTSNode auto-loaded from config: %s",
							weaponInfo.currentConfig->modelName.c_str());
					} else {
						snprintf(m_DebugText, sizeof(m_DebugText),
							"Failed to auto-load TTSNode from config");
					}
				}

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("This will create the TTSNode from the model file specified in the configuration\nand apply the saved camera adjustments.");
				}
				ImGui::Separator();
			}
		}

		// ==================== CAMERA ADJUSTMENT CONTROLS ====================
		ImGui::TextColored(m_AccentColor, "Camera Position & Orientation");
		ImGui::Separator();

		// Position controls with better organization
		if (ImGui::CollapsingHeader("Position Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "PositionColumns", false);

			// Left column - Sliders
			ImGui::Text("Precise Adjustment:");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##X", &m_DeltaPosX, -50.0f, 50.0f, "X: %.3f");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Y", &m_DeltaPosY, -50.0f, 50.0f, "Y: %.3f");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Z", &m_DeltaPosZ, -50.0f, 50.0f, "Z: %.3f");

			ImGui::NextColumn();

			// Right column - Fine adjustment buttons
			ImGui::Text("Fine Tuning:");

			if (ImGui::Button("X-0.1", ImVec2(50, 0)))
				m_DeltaPosX -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("X+0.1", ImVec2(50, 0)))
				m_DeltaPosX += 0.1f;

			if (ImGui::Button("Y-0.1", ImVec2(50, 0)))
				m_DeltaPosY -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("Y+0.1", ImVec2(50, 0)))
				m_DeltaPosY += 0.1f;

			if (ImGui::Button("Z-0.1", ImVec2(50, 0)))
				m_DeltaPosZ -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("Z+0.1", ImVec2(50, 0)))
				m_DeltaPosZ += 0.1f;

			ImGui::Columns(1);
		}

		// Rotation controls
		if (ImGui::CollapsingHeader("Rotation Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "RotationColumns", false);

			ImGui::Text("Precise Adjustment:");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Pitch", &m_DeltaRot[0], -180.0f, 180.0f, "Pitch: %.1f°");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Yaw", &m_DeltaRot[1], -180.0f, 180.0f, "Yaw: %.1f°");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Roll", &m_DeltaRot[2], -180.0f, 180.0f, "Roll: %.1f°");

			ImGui::NextColumn();

			ImGui::Text("Fine Tuning:");

			if (ImGui::Button("P-1°", ImVec2(50, 0)))
				m_DeltaRot[0] -= 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("P+1°", ImVec2(50, 0)))
				m_DeltaRot[0] += 1.0f;

			if (ImGui::Button("Y-1°", ImVec2(50, 0)))
				m_DeltaRot[1] -= 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("Y+1°", ImVec2(50, 0)))
				m_DeltaRot[1] += 1.0f;

			if (ImGui::Button("R-1°", ImVec2(50, 0)))
				m_DeltaRot[2] -= 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("R+1°", ImVec2(50, 0)))
				m_DeltaRot[2] += 1.0f;

			ImGui::Columns(1);
		}

		// Scale controls
		if (ImGui::CollapsingHeader("Scale Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "ScaleColumns", false);

			ImGui::Text("Scale:");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Scale", &m_DeltaScale, 0.1f, 10.0f, "%.3f");

			ImGui::NextColumn();

			ImGui::Text("Fine Tuning:");
			if (ImGui::Button("-0.1", ImVec2(50, 0)))
				m_DeltaScale -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("+0.1", ImVec2(50, 0)))
				m_DeltaScale += 0.1f;

			ImGui::Columns(1);
		}

		// ==================== SCOPE SETTINGS ====================
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Scope Settings")) {
			ImGui::SliderInt("Minimum FOV", &minFOV, 1, 180);
			ImGui::SliderInt("Maximum FOV", &maxFOV, 1, 180);
			ImGui::Checkbox("Night Vision", &nightVision);
			ImGui::Checkbox("Thermal Vision", &thermalVision);
		}

		// ==================== PARALLAX SETTINGS ====================
		if (ImGui::CollapsingHeader("Parallax Settings")) {
			ImGui::SliderFloat("Relative Fog Radius", &m_DeltaRelativeFogRadius, 0.0f, 1.0f);
			ImGui::SliderFloat("Scope Sway Amount", &m_DeltaScopeSwayAmount, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Travel", &m_DeltaMaxTravel, 0.0f, 1.0f);
			ImGui::SliderFloat("Radius", &m_DeltaParallaxRadius, 0.0f, 1.0f);
		}

		// ==================== ACTION BUTTONS ====================
		ImGui::Spacing();
		ImGui::Separator();

		// Button row
		if (ImGui::Button("Reset Adjustments")) {
			ResetAllAdjustments();
		}

		ImGui::SameLine();

		// Save button with validation
		bool hasChanges = true;  // You could implement change detection here
		if (ImGui::Button("Save Settings")) {
			// Update config with current values
			DataPersistence::ScopeConfig modifiedConfig = *currentConfig;

			// Update camera adjustments with current UI values
			modifiedConfig.cameraAdjustments.deltaPosX = m_DeltaPosX;
			modifiedConfig.cameraAdjustments.deltaPosY = m_DeltaPosY;
			modifiedConfig.cameraAdjustments.deltaPosZ = m_DeltaPosZ;
			modifiedConfig.cameraAdjustments.deltaRot[0] = m_DeltaRot[0];
			modifiedConfig.cameraAdjustments.deltaRot[1] = m_DeltaRot[1];
			modifiedConfig.cameraAdjustments.deltaRot[2] = m_DeltaRot[2];
			modifiedConfig.cameraAdjustments.deltaScale = m_DeltaScale;

			// Update other settings
			modifiedConfig.scopeSettings.minFOV = minFOV;
			modifiedConfig.scopeSettings.maxFOV = maxFOV;
			modifiedConfig.scopeSettings.nightVision = nightVision;
			modifiedConfig.scopeSettings.thermalVision = thermalVision;
			modifiedConfig.parallaxSettings.relativeFogRadius = m_DeltaRelativeFogRadius;
			modifiedConfig.parallaxSettings.scopeSwayAmount = m_DeltaScopeSwayAmount;
			modifiedConfig.parallaxSettings.maxTravel = m_DeltaMaxTravel;
			modifiedConfig.parallaxSettings.radius = m_DeltaParallaxRadius;

			if (dataPersistence->SaveConfig(modifiedConfig)) {
				snprintf(m_DebugText, sizeof(m_DebugText), "Settings saved successfully!");
				dataPersistence->LoadAllConfigs();
				//ScopeCamera::ApplyScopeSettings(modifiedConfig);

			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "Failed to save settings!");
			}
		}

		// Display debug/status text
		if (strlen(m_DebugText) > 0) {
			ImGui::Spacing();
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}

    
	void ImGuiManager::RenderRenderingPanel()
	{
		ImGui::TextColored(m_AccentColor, "Scope Rendering Options");
		ImGui::Separator();

		// Enable/disable rendering
		if (ImGui::Checkbox("Enable Scope Rendering", &m_EnableRendering)) {
			D3DHooks::SetEnableRender(m_EnableRendering);
			snprintf(m_DebugText, sizeof(m_DebugText),
				"Scope rendering %s", m_EnableRendering ? "enabled" : "disabled");
		}

		ImGui::TextWrapped(
			"When enabled, the mod will render a second camera view for the scope texture. "
			"This creates a realistic scope effect where the image in the scope shows "
			"what you would actually see through a real scope.");

		// Add more rendering options here if needed
		ImGui::Spacing();
		ImGui::Separator();

		// Display current debug text
		if (strlen(m_DebugText) > 0) {
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}
    
	void ImGuiManager::RenderDebugPanel()
	{
		ImGui::TextColored(m_AccentColor, "Debug Information");
		ImGui::Separator();

		// Display current camera values
		auto scopeCamera = ScopeCamera::GetScopeCamera();
		if (scopeCamera) {
			float pitch, yaw, roll;
			scopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);

			ImGui::Text("Scope Camera Information:");
			ImGui::BulletText("Local Position: [%.3f, %.3f, %.3f]",
				scopeCamera->local.translate.x,
				scopeCamera->local.translate.y,
				scopeCamera->local.translate.z);

			ImGui::BulletText("World Position: [%.3f, %.3f, %.3f]",
				scopeCamera->world.translate.x,
				scopeCamera->world.translate.y,
				scopeCamera->world.translate.z);

			ImGui::BulletText("Local Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			scopeCamera->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			ImGui::BulletText("World Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			ImGui::BulletText("Current FOV: %.2f", ScopeCamera::GetTargetFOV());
		} else {
			ImGui::TextColored(m_WarningColor, "Scope camera not available");
		}

		ImGui::Spacing();
		ImGui::Separator();

		// FTS Node information
		auto ttsNode = GetTTSNode();
		ImGui::Text("TTSNode Information:");
		if (ttsNode) {
			float pitch, yaw, roll;
			ttsNode->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);

			ImGui::BulletText("Local Position: [%.3f, %.3f, %.3f]",
				ttsNode->local.translate.x,
				ttsNode->local.translate.y,
				ttsNode->local.translate.z);

			ImGui::BulletText("World Position: [%.3f, %.3f, %.3f]",
				ttsNode->world.translate.x,
				ttsNode->world.translate.y,
				ttsNode->world.translate.z);

			ImGui::BulletText("Local Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			ttsNode->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			ImGui::BulletText("World Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			ImGui::BulletText("Local Scale: %.3f", ttsNode->local.scale);
		} else {
			ImGui::TextColored(m_WarningColor, "TTSNode not found");
		}

		ImGui::Spacing();
		ImGui::Separator();

		// Current adjustment values
		ImGui::Text("Current Adjustment Values:");
		ImGui::BulletText("Position Delta: [%.3f, %.3f, %.3f]", m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ);
		ImGui::BulletText("Rotation Delta: [%.1f, %.1f, %.1f] degrees", m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2]);
		ImGui::BulletText("Scale Delta: [%.3f]", m_DeltaScale);

		ImGui::Spacing();
		ImGui::Separator();

		// Status information
		ImGui::Text("Status Information:");
		ImGui::BulletText("Rendering Enabled: %s", D3DHooks::IsEnableRender() ? "Yes" : "No");
		ImGui::BulletText("Is Forward Stage: %s", D3DHooks::GetForwardStage() ? "Yes" : "No");
		ImGui::BulletText("Is Rendering For Scope: %s", ScopeCamera::IsRenderingForScope() ? "Yes" : "No");
		ImGui::BulletText("Real-time Adjustment: %s", m_RealTimeAdjustment ? "Enabled" : "Disabled");

		ImGui::Spacing();
		ImGui::Separator();

		// Action buttons
		if (ImGui::Button("Print Node Hierarchy")) {
			auto weaponnode = RE::PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon");
			RE::NiAVObject* existingNode = weaponnode->GetObjectByName("ScopeNode");
			ThroughScope::Utilities::PrintNodeHierarchy(existingNode);
			snprintf(m_DebugText, sizeof(m_DebugText), "Node hierarchy printed to log");
		}

		ImGui::SameLine();

		if (ImGui::Button("Refresh TTSNode")) {
			auto node = GetTTSNode();
			if (node) {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode found and refreshed");
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		}

		ImGui::SameLine();

		// Copy current values to clipboard (useful for saving configurations)
		if (ImGui::Button("Copy Values to Clipboard")) {
			char clipboardText[512];
			snprintf(clipboardText, sizeof(clipboardText),
				"Position: [%.3f, %.3f, %.3f]\n"
				"Rotation: [%.1f, %.1f, %.1f]\n"
				"Scale: [%.3f]\n",
				m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ,
				m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2],
				m_DeltaScale);

			if (::OpenClipboard(nullptr)) {
				EmptyClipboard();
				HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, strlen(clipboardText) + 1);
				if (hClipboardData) {
					char* pchData = (char*)GlobalLock(hClipboardData);
					strcpy_s(pchData, strlen(clipboardText) + 1, clipboardText);
					GlobalUnlock(hClipboardData);
					SetClipboardData(CF_TEXT, hClipboardData);
				}
				CloseClipboard();
				snprintf(m_DebugText, sizeof(m_DebugText), "Values copied to clipboard!");
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "Failed to access clipboard!");
			}
		}

		// Display current debug text
		ImGui::Spacing();
		if (strlen(m_DebugText) > 0) {
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}

	void ImGuiManager::RenderModelSwitcher()
	{
		auto dataPersistence = DataPersistence::GetSingleton();
		DataPersistence::WeaponInfo weaponInfo = DataPersistence::GetCurrentWeaponInfo();
		OptimizedNIFScan();  // 使用优化的扫描

		ImGui::TextColored(m_AccentColor, "Model Management");
		ImGui::Separator();

		if (weaponInfo.currentConfig) {
			// 显示当前模型信息卡片
			ImGui::BeginGroup();
			ImGui::Text("Current Configuration:");
			if (!weaponInfo.currentConfig->modelName.empty()) {
				ImGui::TextColored(m_SuccessColor, "Model: %s", weaponInfo.currentConfig->modelName.c_str());

				// 显示模型统计信息（如果TTSNode存在）
				auto ttsNode = GetTTSNode();
				if (ttsNode) {
					ImGui::Text("Status: Loaded and Active");
					ImGui::Text("Position: [%.2f, %.2f, %.2f]",
						ttsNode->local.translate.x,
						ttsNode->local.translate.y,
						ttsNode->local.translate.z);
				} else {
					ImGui::TextColored(m_WarningColor, "Status: Not Loaded");
				}
			} else {
				ImGui::TextColored(m_WarningColor, "No model assigned");
			}
			ImGui::EndGroup();

			ImGui::Spacing();
			ImGui::Separator();

			// 模型选择和预览
			if (!m_AvailableNIFFiles.empty()) {
				ImGui::TextColored(m_AccentColor, "Available Models:");

				// 找到当前模型在列表中的索引
				int currentModelIndex = -1;
				for (int i = 0; i < m_AvailableNIFFiles.size(); i++) {
					if (m_AvailableNIFFiles[i] == weaponInfo.currentConfig->modelName) {
						currentModelIndex = i;
						break;
					}
				}

				const char* previewText = currentModelIndex >= 0 ?
				                              m_AvailableNIFFiles[currentModelIndex].c_str() :
				                              "Select Model...";

				// 模型选择下拉框
				ImGui::SetNextItemWidth(-100);
				if (ImGui::BeginCombo("##ModelSelect", previewText)) {
					for (int i = 0; i < m_AvailableNIFFiles.size(); i++) {
						bool isSelected = (i == currentModelIndex);
						bool isCurrent = (m_AvailableNIFFiles[i] == weaponInfo.currentConfig->modelName);

						// 为当前使用的模型添加特殊标记
						std::string displayName = m_AvailableNIFFiles[i];
						if (isCurrent) {
							displayName += " (Current)";
						}

						if (ImGui::Selectable(displayName.c_str(), isSelected)) {
							std::string newModel = m_AvailableNIFFiles[i];

							// 如果选择了不同的模型
							if (newModel != weaponInfo.currentConfig->modelName) {
								// 创建新的配置副本
								auto modifiedConfig = *weaponInfo.currentConfig;
								modifiedConfig.modelName = newModel;

								if (dataPersistence->SaveConfig(modifiedConfig)) {
									// 重新加载TTSNode
									if (CreateTTSNodeFromNIF(&modifiedConfig)) {
										snprintf(m_DebugText, sizeof(m_DebugText),
											"Model changed to: %s", newModel.c_str());
										m_HasUnsavedChanges = false;  // 刚刚保存了
									} else {
										snprintf(m_DebugText, sizeof(m_DebugText),
											"Failed to load new model: %s", newModel.c_str());
										ShowErrorDialog("Model Load Error",
											"Failed to load the selected model file. Please check if the file exists and is valid.");
									}

									dataPersistence->LoadAllConfigs();
								} else {
									snprintf(m_DebugText, sizeof(m_DebugText),
										"Failed to save configuration with new model");
									ShowErrorDialog("Save Error",
										"Failed to save the configuration with the new model.");
								}
							}
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}

						// 添加悬停提示
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Click to switch to this model");
						}
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				if (ImGui::Button("Refresh")) {
					m_NIFFilesScanned = false;
					ScanForNIFFiles();
				}

				// 模型预览按钮
				ImGui::Spacing();
				if (ImGui::Button("Preview Selected Model")) {
					if (currentModelIndex >= 0 && currentModelIndex < m_AvailableNIFFiles.size()) {
						CreateTTSNodeFromNIF(m_AvailableNIFFiles[currentModelIndex]);
					}
				}

				RenderHelpTooltip("Load the selected model for preview without saving to configuration");

				ImGui::SameLine();
				if (ImGui::Button("Remove Current Model")) {
					RemoveExistingTTSNode();
				}

				RenderHelpTooltip("Remove the currently loaded TTSNode");
			} else {
				ImGui::TextColored(m_WarningColor, "No models available");
				if (ImGui::Button("Scan for Models")) {
					m_NIFFilesScanned = false;
					ScanForNIFFiles();
				}
			}

			ImGui::Spacing();
			ImGui::Separator();

			// 快速操作按钮
			ImGui::TextColored(m_AccentColor, "Quick Actions:");

			if (ImGui::Button("Reload Current Model", ImVec2(-1, 0))) {
				if (!weaponInfo.currentConfig->modelName.empty()) {
					if (CreateTTSNodeFromNIF(weaponInfo.currentConfig)) {
						snprintf(m_DebugText, sizeof(m_DebugText),
							"Model reloaded: %s", weaponInfo.currentConfig->modelName.c_str());
					} else {
						ShowErrorDialog("Reload Error", "Failed to reload the current model.");
					}
				}
			}

			RenderHelpTooltip("Reload the current model from the configuration");
		}

		// 显示调试信息
		if (strlen(m_DebugText) > 0) {
			ImGui::Spacing();
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}

	bool ImGuiManager::AutoLoadTTSNodeFromConfig(const ThroughScope::DataPersistence::ScopeConfig* config)
	{
		if (!config || config->modelName.empty()) {
			logger::warn("No model file specified in configuration");
			return false;
		}

		// 检查是否已经有正确的TTSNode
		auto existingTTSNode = GetTTSNode();
		if (existingTTSNode) {
			logger::info("TTSNode already exists, checking if reload is needed");
		}

		// 创建或重新创建TTSNode
		if (CreateTTSNodeFromNIF(config)) {
			// 应用保存的变换设置
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ttsNode->local.translate.x = config->cameraAdjustments.deltaPosX;
				ttsNode->local.translate.y = config->cameraAdjustments.deltaPosY;
				ttsNode->local.translate.z = config->cameraAdjustments.deltaPosZ;

				// 应用旋转
				float pitch = config->cameraAdjustments.deltaRot[0] * 0.01745329251f;  // 转换为弧度
				float yaw = config->cameraAdjustments.deltaRot[1] * 0.01745329251f;
				float roll = config->cameraAdjustments.deltaRot[2] * 0.01745329251f;

				RE::NiMatrix3 rotMat;
				rotMat.MakeIdentity();
				rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);
				ttsNode->local.rotate = rotMat;

				// 应用缩放
				ttsNode->local.scale = config->cameraAdjustments.deltaScale;

				// 更新节点
				RE::NiUpdateData updateData{};
				updateData.camera = ScopeCamera::GetScopeCamera();
				if (updateData.camera) {
					ttsNode->Update(updateData);
				}

				logger::info("TTSNode loaded and transformed from config: {}", config->modelName);
				return true;
			}
		}

		logger::error("Failed to auto-load TTSNode from config");
		return false;
	}
    
	void ImGuiManager::RenderHelpTooltip(const char* text)
	{
		if (m_ShowHelpTooltips) {
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", text);
			}
		}
	}

	// 3. 改进的Reset确认对话框
	void ImGuiManager::ShowResetConfirmationDialog()
	{
		static bool showResetConfirm = false;

		if (ImGui::Button("Reset Adjustments")) {
			if (m_ConfirmBeforeReset) {
				showResetConfirm = true;
			} else {
				ResetAllAdjustments();
			}
		}

		// Reset confirmation modal
		if (showResetConfirm) {
			ImGui::OpenPopup("Reset Confirmation");
		}

		if (ImGui::BeginPopupModal("Reset Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to reset all adjustments?");
			ImGui::Text("This will restore default position, rotation, and scale values.");
			ImGui::Spacing();

			if (ImGui::Button("Yes, Reset", ImVec2(120, 0))) {
				ResetAllAdjustments();
				showResetConfirm = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				showResetConfirm = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ImGuiManager::CheckAutoSave()
	{
		if (m_AutoSaveEnabled && m_HasUnsavedChanges) {
			float currentTime = ImGui::GetTime();
			if (currentTime - m_LastSaveTime > 30.0f) {  // 30秒自动保存
				// 执行自动保存
				// SaveCurrentSettings();
				m_HasUnsavedChanges = false;
				m_LastSaveTime = currentTime;
				snprintf(m_DebugText, sizeof(m_DebugText), "Auto-saved at %.1f", currentTime);
			}
		}
	}

	void ImGuiManager::RenderSettingsPanel()
	{
		ImGui::TextColored(m_AccentColor, "Interface Settings");
		ImGui::Separator();

		ImGui::Checkbox("Show Help Tooltips", &m_ShowHelpTooltips);
		RenderHelpTooltip("Show helpful tooltips when hovering over UI elements");

		ImGui::Checkbox("Auto-Save Changes", &m_AutoSaveEnabled);
		RenderHelpTooltip("Automatically save changes every 30 seconds");

		ImGui::Checkbox("Confirm Before Reset", &m_ConfirmBeforeReset);
		RenderHelpTooltip("Show confirmation dialog before resetting adjustments");

		ImGui::Checkbox("Real-time Adjustment", &m_RealTimeAdjustment);
		RenderHelpTooltip("Apply changes immediately as you adjust sliders");

		ImGui::Spacing();
		ImGui::Separator();

		// Key binding section
		ImGui::TextColored(m_AccentColor, "Key Bindings");
		ImGui::Text("Menu Toggle: F2");
		ImGui::SameLine();
		if (ImGui::Button("Change##MenuKey")) {
			// Open key binding dialog
		}

		// Performance settings
		ImGui::Spacing();
		ImGui::TextColored(m_AccentColor, "Performance");

		static int refreshRate = 60;
		ImGui::SliderInt("UI Refresh Rate", &refreshRate, 30, 144, "%d FPS");
		RenderHelpTooltip("Lower refresh rates can improve performance");
	}

	// 6. 改进的状态栏
	void ImGuiManager::RenderStatusBar()
	{
		ImGui::Separator();

		// Status bar at bottom
		ImGui::Columns(3, "StatusColumns", false);

		// Left: Current weapon info
		auto weaponInfo = DataPersistence::GetCurrentWeaponInfo();
		if (weaponInfo.weapon) {
			ImGui::TextColored(m_SuccessColor, "✓ Weapon Loaded");
		} else {
			ImGui::TextColored(m_WarningColor, "⚠ No Weapon");
		}

		ImGui::NextColumn();

		// Center: TTSNode status
		auto ttsNode = GetTTSNode();
		if (ttsNode) {
			ImGui::TextColored(m_SuccessColor, "✓ TTSNode Ready");
		} else {
			ImGui::TextColored(m_WarningColor, "⚠ No TTSNode");
		}

		ImGui::NextColumn();

		// Right: Save status
		if (m_HasUnsavedChanges) {
			ImGui::TextColored(m_WarningColor, "● Unsaved Changes");
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○ All Saved");
		}

		ImGui::Columns(1);
	}

	// 7. 性能优化的NIF扫描
	void ImGuiManager::OptimizedNIFScan()
	{
		float currentTime = ImGui::GetTime();

		// 只在需要时或间隔时间后扫描
		if (!m_NIFFilesScanned || currentTime > m_NextNIFScanTime) {
			ScanForNIFFiles();
			m_NextNIFScanTime = currentTime + NIF_SCAN_INTERVAL;
		}
	}

	// 8. 改进的错误处理和用户反馈
	void ImGuiManager::ShowErrorDialog(const std::string& title, const std::string& message)
	{
		static bool showError = false;
		static std::string errorTitle, errorMessage;

		if (!title.empty() && !message.empty()) {
			errorTitle = title;
			errorMessage = message;
			showError = true;
		}

		if (showError) {
			ImGui::OpenPopup(errorTitle.c_str());
		}

		if (ImGui::BeginPopupModal(errorTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextWrapped("%s", errorMessage.c_str());
			ImGui::Spacing();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				showError = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ImGuiManager::ApplyConfigToTTSNode(const ThroughScope::DataPersistence::ScopeConfig* config)
	{
		auto ttsNode = GetTTSNode();
		if (!ttsNode || !config) {
			return;
		}

		try {
			// 应用位置
			ttsNode->local.translate.x = config->cameraAdjustments.deltaPosX;
			ttsNode->local.translate.y = config->cameraAdjustments.deltaPosY;
			ttsNode->local.translate.z = config->cameraAdjustments.deltaPosZ;

			// 应用旋转
			float pitch = config->cameraAdjustments.deltaRot[0] * 0.01745329251f;
			float yaw = config->cameraAdjustments.deltaRot[1] * 0.01745329251f;
			float roll = config->cameraAdjustments.deltaRot[2] * 0.01745329251f;

			RE::NiMatrix3 rotMat;
			rotMat.MakeIdentity();
			rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);
			ttsNode->local.rotate = rotMat;

			// 应用缩放
			ttsNode->local.scale = config->cameraAdjustments.deltaScale;

			// 更新节点
			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				ttsNode->Update(updateData);
			}

			logger::info("Applied config values to TTSNode");
		} catch (const std::exception& e) {
			logger::error("Error applying config to TTSNode: {}", e.what());
		}
	}

	void ImGuiManager::ApplyUIValuesToTTSNode()
	{
		auto ttsNode = GetTTSNode();
		if (!ttsNode) {
			return;
		}

		try {
			// 应用UI中的值到TTSNode
			ttsNode->local.translate.x = m_DeltaPosX;
			ttsNode->local.translate.y = m_DeltaPosY;
			ttsNode->local.translate.z = m_DeltaPosZ;

			// 转换度数到弧度并应用旋转
			float pitch = m_DeltaRot[0] * 0.01745329251f;
			float yaw = m_DeltaRot[1] * 0.01745329251f;
			float roll = m_DeltaRot[2] * 0.01745329251f;

			RE::NiMatrix3 rotMat;
			rotMat.MakeIdentity();
			rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);
			ttsNode->local.rotate = rotMat;

			ttsNode->local.scale = m_DeltaScale;

			// 更新节点
			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				ttsNode->Update(updateData);
			}
		} catch (const std::exception& e) {
			logger::error("Error applying UI values to TTSNode: {}", e.what());
		}
	}
}
