#include "CameraAdjustmentPanel.h"
#include "NiFLoader.h"

namespace ThroughScope
{
	// 实现文件内容
	CameraAdjustmentPanel::CameraAdjustmentPanel(PanelManagerInterface* manager) :
		m_Manager(manager)
	{
	}

	bool CameraAdjustmentPanel::Initialize()
	{
		return true;
	}

	void CameraAdjustmentPanel::Render()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		RenderWeaponInformation();

		if (!weaponInfo.currentConfig) {
			RenderConfigurationSection();
			return;
		}

		// 检查是否需要重新初始化UI值
		std::string currentConfigKey = fmt::format("{}_{:08X}",
			weaponInfo.currentConfig->weaponConfig.modFileName.c_str(),
			weaponInfo.currentConfig->weaponConfig.localFormID);

		if (!m_UIValuesInitialized || m_LastLoadedConfigKey != currentConfigKey) {
			LoadFromConfig(weaponInfo.currentConfig);
			m_LastLoadedConfigKey = currentConfigKey;
			m_UIValuesInitialized = true;
		}

		RenderAdjustmentControls();
		RenderScopeSettings();
		RenderParallaxSettings();
		RenderActionButtons();
	}

	void CameraAdjustmentPanel::Update()
	{
		if (!m_RealTimeAdjustment)
			return;

		if (HasChanges()) {
			ApplyAllAdjustments();
			UpdatePreviousValues();
			m_Manager->MarkUnsavedChanges();
		}
	}

	void CameraAdjustmentPanel::RenderWeaponInformation()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		RenderSectionHeader("Current Weapon Information");

		if (!weaponInfo.weapon || !weaponInfo.instanceData) {
			ImGui::TextColored(m_WarningColor, "No valid weapon equipped");
			ImGui::TextWrapped("Please equip a weapon with scope capabilities to configure settings.");
			return;
		}

		ImGui::Text("Weapon: [%08X] %s", weaponInfo.weaponFormID, weaponInfo.weaponModName.c_str());

		if (weaponInfo.selectedModForm) {
			ImGui::Text("Config Source: [%08X] %s (%s)",
				weaponInfo.selectedModForm->GetLocalFormID(),
				weaponInfo.selectedModForm->GetFile()->filename,
				weaponInfo.configSource.c_str());
		} else if (weaponInfo.currentConfig) {
			ImGui::Text("Config Source: Weapon (%s)", weaponInfo.configSource.c_str());
		}

		if (weaponInfo.currentConfig && !weaponInfo.currentConfig->modelName.empty()) {
			ImGui::Text("Current Model: %s", weaponInfo.currentConfig->modelName.c_str());

			ImGui::SameLine();
			if (ImGui::Button("Reload TTSNode")) {
				if (CreateTTSNodeFromConfig(weaponInfo.currentConfig)) {
					m_Manager->SetDebugText(fmt::format("TTSNode reloaded from: {}",
						weaponInfo.currentConfig->modelName)
							.c_str());
				} else {
					m_Manager->SetDebugText(fmt::format("Failed to reload TTSNode from: {}",
						weaponInfo.currentConfig->modelName)
							.c_str());
				}
			}
			RenderHelpTooltip("Click to recreate the TTSNode from the saved model file");
		}

		ImGui::Spacing();
	}

	void CameraAdjustmentPanel::RenderConfigurationSection()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		ImGui::Separator();
		ImGui::TextColored(m_WarningColor, "No configuration found for this weapon");
		ImGui::TextWrapped("Create a new configuration to start customizing your scope settings.");

		ScanForNIFFiles();

		// Configuration target selection
		ImGui::Spacing();
		ImGui::TextColored(m_AccentColor, "Configuration Target:");

		// Store createOption per weapon to prevent cross-weapon contamination
		static std::unordered_map<uint32_t, int> weaponCreateOptions;
		int& createOption = weaponCreateOptions[weaponInfo.weaponFormID];

		// Ensure createOption is always valid for current weapon
		if (createOption < 0 || createOption > static_cast<int>(weaponInfo.availableMods.size())) {
			createOption = 0;  // Reset to base weapon if invalid
		}

		// Build combo label safely
		std::string comboLabel;
		if (createOption == 0) {
			comboLabel = "Base Weapon";
		} else if (createOption <= static_cast<int>(weaponInfo.availableMods.size())) {
			auto modForm = weaponInfo.availableMods[createOption - 1];
			comboLabel = fmt::format("Modification #{}, {}", createOption, modForm->GetFormEditorID());
		} else {
			comboLabel = "Invalid Selection";
			createOption = 0;  // Force reset to base weapon
		}

		if (ImGui::BeginCombo("Create Config For", comboLabel.c_str())) {
			// Base weapon option
			if (ImGui::Selectable("Base Weapon", createOption == 0)) {
				createOption = 0;
			}
			RenderHelpTooltip("Create configuration for the base weapon");

			// Modification options
			for (size_t i = 0; i < weaponInfo.availableMods.size(); i++) {
				auto modForm = weaponInfo.availableMods[i];
				std::string label = fmt::format("Modification #{} - {}",
					i + 1, modForm->GetFormEditorID());

				if (ImGui::Selectable(label.c_str(), createOption == static_cast<int>(i + 1))) {
					createOption = static_cast<int>(i + 1);
				}
				RenderHelpTooltip("Create configuration specific to this modification");
			}

			ImGui::EndCombo();
		}

		// NIF File Selection
		ImGui::Spacing();
		ImGui::TextColored(m_AccentColor, "Scope Shape (Model File):");

		if (m_AvailableNIFFiles.empty()) {
			ImGui::TextColored(m_WarningColor, "No NIF files found in Meshes/TTS/ScopeShape/");
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
		}

		// Create Configuration Button
		ImGui::Spacing();
		ImGui::Separator();

		bool canCreateConfig = !m_AvailableNIFFiles.empty() && m_SelectedNIFIndex < m_AvailableNIFFiles.size();

		if (!canCreateConfig) {
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Create Configuration", ImVec2(-1, 0))) {
			if (canCreateConfig) {
				std::string selectedNIF = m_AvailableNIFFiles[m_SelectedNIFIndex];
				auto dataPersistence = DataPersistence::GetSingleton();

				bool success = false;
				if (createOption == 0) {
					success = dataPersistence->GeneratePresetConfig(
						weaponInfo.weaponFormID,
						weaponInfo.weaponModName,
						selectedNIF);
				} else if (createOption > 0 && createOption <= static_cast<int>(weaponInfo.availableMods.size())) {
					auto modForm = weaponInfo.availableMods[createOption - 1];
					success = dataPersistence->GeneratePresetConfig(
						modForm->GetLocalFormID(),
						modForm->GetFile()->filename,
						selectedNIF);
				}

				if (success) {
					m_Manager->SetDebugText(fmt::format("Configuration created successfully! Model: {}",
						selectedNIF)
							.c_str());
					dataPersistence->LoadAllConfigs();
					weaponInfo = m_Manager->GetCurrentWeaponInfo();
					ScopeCamera::SetupScopeForWeapon(weaponInfo);
				} else {
					m_Manager->SetDebugText("Failed to create configuration!");
				}
			}
		}

		if (!canCreateConfig) {
			ImGui::EndDisabled();
			ImGui::TextColored(m_WarningColor, "Please select a model file to create configuration");
		}
	}

	void CameraAdjustmentPanel::RenderAdjustmentControls()
	{
		RenderSectionHeader("Camera Position & Orientation");

		// Position controls
		if (ImGui::CollapsingHeader("Position Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "PositionColumns", false);

			// Left column - Sliders
			ImGui::Text("Precise Adjustment:");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##X", &m_CurrentValues.deltaPosX, -50.0f, 50.0f, "X: %.3f");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Y", &m_CurrentValues.deltaPosY, -50.0f, 50.0f, "Y: %.3f");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Z", &m_CurrentValues.deltaPosZ, -50.0f, 50.0f, "Z: %.3f");

			ImGui::NextColumn();

			// Right column - Fine adjustment buttons
			ImGui::Text("Fine Tuning:");

			if (ImGui::Button("X-0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaPosX -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("X+0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaPosX += 0.1f;

			if (ImGui::Button("Y-0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaPosY -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("Y+0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaPosY += 0.1f;

			if (ImGui::Button("Z-0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaPosZ -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("Z+0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaPosZ += 0.1f;

			ImGui::Columns(1);
		}

		// Rotation controls
		if (ImGui::CollapsingHeader("Rotation Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "RotationColumns", false);

			ImGui::Text("Precise Adjustment:");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Pitch", &m_CurrentValues.deltaRot[0], -180.0f, 180.0f, "Pitch: %.1f°");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Yaw", &m_CurrentValues.deltaRot[1], -180.0f, 180.0f, "Yaw: %.1f°");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Roll", &m_CurrentValues.deltaRot[2], -180.0f, 180.0f, "Roll: %.1f°");

			ImGui::NextColumn();

			ImGui::Text("Fine Tuning:");

			if (ImGui::Button("P-1°", ImVec2(50, 0)))
				m_CurrentValues.deltaRot[0] -= 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("P+1°", ImVec2(50, 0)))
				m_CurrentValues.deltaRot[0] += 1.0f;

			if (ImGui::Button("Y-1°", ImVec2(50, 0)))
				m_CurrentValues.deltaRot[1] -= 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("Y+1°", ImVec2(50, 0)))
				m_CurrentValues.deltaRot[1] += 1.0f;

			if (ImGui::Button("R-1°", ImVec2(50, 0)))
				m_CurrentValues.deltaRot[2] -= 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("R+1°", ImVec2(50, 0)))
				m_CurrentValues.deltaRot[2] += 1.0f;

			ImGui::Columns(1);
		}

		// Scale controls
		if (ImGui::CollapsingHeader("Scale Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "ScaleColumns", false);

			ImGui::Text("Scale:");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Scale", &m_CurrentValues.deltaScale, 0.1f, 10.0f, "%.3f");

			ImGui::NextColumn();

			ImGui::Text("Fine Tuning:");
			if (ImGui::Button("-0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaScale -= 0.1f;
			ImGui::SameLine();
			if (ImGui::Button("+0.1", ImVec2(50, 0)))
				m_CurrentValues.deltaScale += 0.1f;

			ImGui::Columns(1);
		}
	}

	void CameraAdjustmentPanel::RenderScopeSettings()
	{
		if (ImGui::CollapsingHeader("Scope Settings")) {
			auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
			if (weaponInfo.currentConfig) {
				static int minFOV = weaponInfo.currentConfig->scopeSettings.minFOV;
				static int maxFOV = weaponInfo.currentConfig->scopeSettings.maxFOV;
				static bool nightVision = weaponInfo.currentConfig->scopeSettings.nightVision;
				static bool thermalVision = weaponInfo.currentConfig->scopeSettings.thermalVision;

				ImGui::SliderInt("Minimum FOV", &minFOV, 1, 180);
				ImGui::SliderInt("Maximum FOV", &maxFOV, 1, 180);
				ImGui::Checkbox("Night Vision", &nightVision);
				ImGui::Checkbox("Thermal Vision", &thermalVision);
			}
		}
	}

	void CameraAdjustmentPanel::RenderParallaxSettings()
	{
		if (ImGui::CollapsingHeader("Parallax Settings")) {
			ImGui::SliderFloat("Relative Fog Radius", &m_CurrentValues.relativeFogRadius, 0.0f, 1.0f);
			ImGui::SliderFloat("Scope Sway Amount", &m_CurrentValues.scopeSwayAmount, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Travel", &m_CurrentValues.maxTravel, 0.0f, 1.0f);
			ImGui::SliderFloat("Radius", &m_CurrentValues.parallaxRadius, 0.0f, 1.0f);
		}
	}

	void CameraAdjustmentPanel::RenderActionButtons()
	{
		ImGui::Spacing();
		ImGui::Separator();

		if (ImGui::Button("Reset Adjustments")) {
			if (m_ConfirmBeforeReset) {
				// Show confirmation dialog
				ImGui::OpenPopup("Reset Confirmation");
			} else {
				ResetAllAdjustments();
			}
		}

		// Reset confirmation modal
		if (ImGui::BeginPopupModal("Reset Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to reset all adjustments?");
			ImGui::Text("This will restore default position, rotation, and scale values.");
			ImGui::Spacing();

			if (ImGui::Button("Yes, Reset", ImVec2(120, 0))) {
				ResetAllAdjustments();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Save Settings")) {
			auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
			if (weaponInfo.currentConfig) {
				DataPersistence::ScopeConfig modifiedConfig = *weaponInfo.currentConfig;
				SaveToConfig(modifiedConfig);

				auto dataPersistence = DataPersistence::GetSingleton();
				if (dataPersistence->SaveConfig(modifiedConfig)) {
					m_Manager->SetDebugText("Settings saved successfully!");
					dataPersistence->LoadAllConfigs();
				} else {
					m_Manager->SetDebugText("Failed to save settings!");
				}
			}
		}
	}

	void CameraAdjustmentPanel::SetCurrentValues(const AdjustmentValues& values)
	{
		m_CurrentValues = values;
		UpdatePreviousValues();
	}

	void CameraAdjustmentPanel::LoadFromConfig(const DataPersistence::ScopeConfig* config)
	{
		if (!config)
			return;

		m_CurrentValues.deltaPosX = config->cameraAdjustments.deltaPosX;
		m_CurrentValues.deltaPosY = config->cameraAdjustments.deltaPosY;
		m_CurrentValues.deltaPosZ = config->cameraAdjustments.deltaPosZ;
		m_CurrentValues.deltaRot[0] = config->cameraAdjustments.deltaRot[0];
		m_CurrentValues.deltaRot[1] = config->cameraAdjustments.deltaRot[1];
		m_CurrentValues.deltaRot[2] = config->cameraAdjustments.deltaRot[2];
		m_CurrentValues.deltaScale = config->cameraAdjustments.deltaScale;

		m_CurrentValues.relativeFogRadius = config->parallaxSettings.relativeFogRadius;
		m_CurrentValues.scopeSwayAmount = config->parallaxSettings.scopeSwayAmount;
		m_CurrentValues.maxTravel = config->parallaxSettings.maxTravel;
		m_CurrentValues.parallaxRadius = config->parallaxSettings.radius;

		UpdatePreviousValues();

		// 应用到TTSNode
		auto ttsNode = m_Manager->GetTTSNode();
		if (ttsNode) {
			ApplyAllAdjustments();
		}
	}

	void CameraAdjustmentPanel::SaveToConfig(DataPersistence::ScopeConfig& config) const
	{
		config.cameraAdjustments.deltaPosX = m_CurrentValues.deltaPosX;
		config.cameraAdjustments.deltaPosY = m_CurrentValues.deltaPosY;
		config.cameraAdjustments.deltaPosZ = m_CurrentValues.deltaPosZ;
		config.cameraAdjustments.deltaRot[0] = m_CurrentValues.deltaRot[0];
		config.cameraAdjustments.deltaRot[1] = m_CurrentValues.deltaRot[1];
		config.cameraAdjustments.deltaRot[2] = m_CurrentValues.deltaRot[2];
		config.cameraAdjustments.deltaScale = m_CurrentValues.deltaScale;

		config.parallaxSettings.relativeFogRadius = m_CurrentValues.relativeFogRadius;
		config.parallaxSettings.scopeSwayAmount = m_CurrentValues.scopeSwayAmount;
		config.parallaxSettings.maxTravel = m_CurrentValues.maxTravel;
		config.parallaxSettings.radius = m_CurrentValues.parallaxRadius;
	}

	void CameraAdjustmentPanel::ResetAllAdjustments()
	{
		m_CurrentValues.deltaPosX = 0.0f;
		m_CurrentValues.deltaPosY = 0.0f;
		m_CurrentValues.deltaPosZ = 7.0f;
		m_CurrentValues.deltaRot[0] = 0.0f;
		m_CurrentValues.deltaRot[1] = 0.0f;
		m_CurrentValues.deltaRot[2] = 0.0f;
		m_CurrentValues.deltaScale = 1.5f;

		if (m_RealTimeAdjustment) {
			ApplyAllAdjustments();
		}

		UpdatePreviousValues();
		m_Manager->SetDebugText("All adjustments reset!");
	}

	bool CameraAdjustmentPanel::HasChanges() const
	{
		const float epsilon = 0.001f;
		return std::abs(m_CurrentValues.deltaPosX - m_PreviousValues.deltaPosX) > epsilon ||
		       std::abs(m_CurrentValues.deltaPosY - m_PreviousValues.deltaPosY) > epsilon ||
		       std::abs(m_CurrentValues.deltaPosZ - m_PreviousValues.deltaPosZ) > epsilon ||
		       std::abs(m_CurrentValues.deltaRot[0] - m_PreviousValues.deltaRot[0]) > 0.1f ||
		       std::abs(m_CurrentValues.deltaRot[1] - m_PreviousValues.deltaRot[1]) > 0.1f ||
		       std::abs(m_CurrentValues.deltaRot[2] - m_PreviousValues.deltaRot[2]) > 0.1f ||
		       std::abs(m_CurrentValues.deltaScale - m_PreviousValues.deltaScale) > epsilon ||
		       std::abs(m_CurrentValues.relativeFogRadius - m_PreviousValues.relativeFogRadius) > epsilon ||
		       std::abs(m_CurrentValues.scopeSwayAmount - m_PreviousValues.scopeSwayAmount) > epsilon ||
		       std::abs(m_CurrentValues.maxTravel - m_PreviousValues.maxTravel) > epsilon ||
		       std::abs(m_CurrentValues.parallaxRadius - m_PreviousValues.parallaxRadius) > epsilon;
	}

	void CameraAdjustmentPanel::UpdatePreviousValues()
	{
		m_PreviousValues = m_CurrentValues;
	}

	void CameraAdjustmentPanel::ApplyPositionAdjustment()
	{
		auto ttsNode = m_Manager->GetTTSNode();
		if (ttsNode) {
			ttsNode->local.translate.x = m_CurrentValues.deltaPosX;
			ttsNode->local.translate.y = m_CurrentValues.deltaPosY;
			ttsNode->local.translate.z = m_CurrentValues.deltaPosZ;

			RE::NiUpdateData tempData{};
			tempData.camera = ScopeCamera::GetScopeCamera();
			if (tempData.camera) {
				ttsNode->Update(tempData);
			}
		}
	}

	void CameraAdjustmentPanel::ApplyRotationAdjustment()
	{
		auto ttsNode = m_Manager->GetTTSNode();
		if (ttsNode) {
			float pitch = m_CurrentValues.deltaRot[0] * 0.01745329251f;  // PI/180
			float yaw = m_CurrentValues.deltaRot[1] * 0.01745329251f;
			float roll = m_CurrentValues.deltaRot[2] * 0.01745329251f;

			RE::NiMatrix3 rotMat;
			rotMat.MakeIdentity();
			rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);

			ttsNode->local.rotate = rotMat;

			RE::NiUpdateData tempData{};
			tempData.camera = ScopeCamera::GetScopeCamera();
			if (tempData.camera) {
				ttsNode->Update(tempData);
			}
		}
	}

	void CameraAdjustmentPanel::ApplyScaleAdjustment()
	{
		auto ttsNode = m_Manager->GetTTSNode();
		if (ttsNode) {
			ttsNode->local.scale = m_CurrentValues.deltaScale;

			RE::NiUpdateData tempData{};
			tempData.camera = ScopeCamera::GetScopeCamera();
			if (tempData.camera) {
				ttsNode->Update(tempData);
			}
		}
	}

	void CameraAdjustmentPanel::ApplyAllAdjustments()
	{
		ApplyPositionAdjustment();
		ApplyRotationAdjustment();
		ApplyScaleAdjustment();

		// 应用视差设置
		D3DHooks::UpdateScopeSettings(
			m_CurrentValues.relativeFogRadius,
			m_CurrentValues.scopeSwayAmount,
			m_CurrentValues.maxTravel,
			m_CurrentValues.parallaxRadius);
	}

	void CameraAdjustmentPanel::ScanForNIFFiles()
	{
		if (m_NIFFilesScanned) {
			return;
		}

		m_AvailableNIFFiles.clear();

		try {
			std::filesystem::path dataPath = std::filesystem::current_path() / "Data" / "Meshes" / "TTS" / "ScopeShape";

			if (!std::filesystem::exists(dataPath)) {
				m_Manager->SetDebugText("ScopeShape directory not found!");
				return;
			}

			for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
				if (entry.is_regular_file() && entry.path().extension() == ".nif") {
					std::string fileName = entry.path().filename().string();
					m_AvailableNIFFiles.push_back(fileName);
				}
			}

			m_NIFFilesScanned = true;
			m_Manager->SetDebugText(fmt::format("Found {} NIF files", m_AvailableNIFFiles.size()).c_str());
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error scanning NIF files: {}", e.what()).c_str());
		}
	}

	bool CameraAdjustmentPanel::CreateTTSNodeFromNIF(const std::string& nifFileName)
	{
		try {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (!playerCharacter || !playerCharacter->Get3D()) {
				return false;
			}

			auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
			if (!weaponNode || !weaponNode->IsNode()) {
				return false;
			}

			auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
			RemoveExistingTTSNode();

			std::string fullPath = "Meshes\\TTS\\ScopeShape\\" + nifFileName;
			auto nifLoader = NIFLoader::GetSington();
			RE::NiNode* loadedNode = nifLoader->LoadNIF(fullPath.c_str());

			if (!loadedNode) {
				return false;
			}

			ScopeCamera::s_CurrentScopeNode = loadedNode;

			// 设置初始变换
			loadedNode->local.translate = RE::NiPoint3(0, 0, 7);
			loadedNode->local.rotate.MakeIdentity();
			loadedNode->local.scale = 1.5f;

			weaponNiNode->AttachChild(loadedNode, false);

			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				loadedNode->Update(updateData);
				weaponNiNode->Update(updateData);
			}

			return true;
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Exception creating TTSNode: {}", e.what()).c_str());
			return false;
		}
	}

	bool CameraAdjustmentPanel::CreateTTSNodeFromConfig(const DataPersistence::ScopeConfig* config)
	{
		if (CreateTTSNodeFromNIF(config->modelName)) {
			LoadFromConfig(config);
			return true;
		}
		return false;
	}

	void CameraAdjustmentPanel::RemoveExistingTTSNode()
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
				weaponNiNode->DetachChild(existingTTSNode);

				RE::NiUpdateData updateData{};
				updateData.camera = ScopeCamera::GetScopeCamera();
				if (updateData.camera) {
					weaponNiNode->Update(updateData);
				}
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error removing TTSNode: {}", e.what()).c_str());
		}
	}

	bool CameraAdjustmentPanel::AutoLoadTTSNodeFromConfig(const DataPersistence::ScopeConfig* config)
	{
		if (!config || config->modelName.empty()) {
			return false;
		}

		if (CreateTTSNodeFromConfig(config)) {
			m_Manager->SetDebugText(fmt::format("TTSNode auto-loaded: {}", config->modelName).c_str());
			return true;
		}

		return false;
	}
}
