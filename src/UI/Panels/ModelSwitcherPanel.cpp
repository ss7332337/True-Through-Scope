#include "ModelSwitcherPanel.h"
#include "ScopeCamera.h"
#include "NiFLoader.h"

#include "misc/cpp/imgui_stdlib.h"

namespace ThroughScope
{
	ModelSwitcherPanel::ModelSwitcherPanel(PanelManagerInterface* manager) :
		m_Manager(manager)
	{
	}

	bool ModelSwitcherPanel::Initialize()
	{
		RefreshNIFFiles();
		return true;
	}

	bool ModelSwitcherPanel::ShouldShow() const
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		return weaponInfo.currentConfig != nullptr;
	}

	void ModelSwitcherPanel::Render()
	{
		OptimizedScan();

		RenderCurrentModelInfo();
		ImGui::Spacing();
		RenderModelSelection();
		ImGui::Spacing();
		RenderQuickActions();

		if (m_ShowModelPreview) {
			RenderModelPreview();
		}
	}

	void ModelSwitcherPanel::Update()
	{
		// 定期刷新NIF文件列表
		float currentTime = ImGui::GetTime();
		if (currentTime > m_NextScanTime) {
			RefreshNIFFiles();
			m_NextScanTime = currentTime + SCAN_INTERVAL;
		}
	}

	void ModelSwitcherPanel::RenderCurrentModelInfo()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		RenderSectionHeader("Current Model Information");

		if (!weaponInfo.currentConfig) {
			ImGui::TextColored(m_WarningColor, "No configuration available");
			return;
		}

		ImGui::BeginGroup();
		if (!weaponInfo.currentConfig->modelName.empty()) {
			ImGui::TextColored(m_SuccessColor, "Model: %s", weaponInfo.currentConfig->modelName.c_str());

			// 显示模型状态
			auto ttsNode = m_Manager->GetTTSNode();
			if (ttsNode) {
				ImGui::Text("Status: ✓ Loaded and Active");
				ImGui::Text("Position: [%.2f, %.2f, %.2f]",
					ttsNode->local.translate.x,
					ttsNode->local.translate.y,
					ttsNode->local.translate.z);
			} else {
				ImGui::TextColored(m_WarningColor, "Status: ⚠ Not Loaded");

				if (ImGui::Button("Auto-Load from Config")) {
					if (PreviewModel(weaponInfo.currentConfig->modelName)) {
						m_Manager->SetDebugText("Model auto-loaded from configuration");
					} else {
						m_Manager->SetDebugText("Failed to auto-load model from configuration");
					}
				}
				RenderHelpTooltip("Load the model specified in the current configuration");
			}
		} else {
			ImGui::TextColored(m_WarningColor, "No model assigned to this configuration");
		}
		ImGui::EndGroup();
	}

	void ModelSwitcherPanel::RenderModelSelection()
	{
		RenderSectionHeader("Available Models");

		if (m_AvailableNIFFiles.empty()) {
			ImGui::TextColored(m_WarningColor, "No models found in ScopeShape directory");
			if (ImGui::Button("Scan for Models")) {
				RefreshNIFFiles();
			}
			return;
		}

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		int currentModelIndex = FindModelIndex(weaponInfo.currentConfig->modelName);

		// 搜索过滤器
		ImGui::SetNextItemWidth(-100);
		ImGui::InputTextWithHint("##Search", "Search models...", &m_SearchFilter);
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			m_SearchFilter.clear();
		}

		// 模型列表
		ImGui::Spacing();
		std::string previewText = currentModelIndex >= 0 ?
		                              GetModelDisplayName(m_AvailableNIFFiles[currentModelIndex], true) :
		                              "Select Model...";

		ImGui::SetNextItemWidth(-100);
		if (ImGui::BeginCombo("##ModelSelect", previewText.c_str())) {
			for (int i = 0; i < m_AvailableNIFFiles.size(); i++) {
				const std::string& fileName = m_AvailableNIFFiles[i];

				// 应用搜索过滤器
				if (!m_SearchFilter.empty()) {
					std::string lowerFileName = fileName;
					std::string lowerFilter = m_SearchFilter;
					std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
					std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

					if (lowerFileName.find(lowerFilter) == std::string::npos) {
						continue;
					}
				}

				bool isSelected = (i == currentModelIndex);
				bool isCurrent = (fileName == weaponInfo.currentConfig->modelName);

				std::string displayName = GetModelDisplayName(fileName, isCurrent);

				if (ImGui::Selectable(displayName.c_str(), isSelected)) {
					if (fileName != weaponInfo.currentConfig->modelName) {
						if (SwitchToModel(fileName)) {
							m_Manager->SetDebugText(fmt::format("Model switched to: {}", fileName).c_str());
						} else {
							m_Manager->ShowErrorDialog("Model Switch Error",
								"Failed to switch to the selected model. Please check if the file is valid.");
						}
					}
				}

				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}

				// 悬停预览
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Click to switch to this model\nFile: %s", fileName.c_str());
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Refresh")) {
			RefreshNIFFiles();
		}
	}

	void ModelSwitcherPanel::RenderQuickActions()
	{
		RenderSectionHeader("Quick Actions");

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		// 重新加载当前模型
		if (ImGui::Button("Reload Current Model", ImVec2(-1, 0))) {
			ReloadCurrentModel();
		}
		RenderHelpTooltip("Reload the current model from the configuration");

		// 移除当前模型
		if (ImGui::Button("Remove Current Model", ImVec2(-1, 0))) {
			RemoveCurrentModel();
		}
		RenderHelpTooltip("Remove the currently loaded TTSNode");

		ImGui::Spacing();

		// 模型预览控制
		ImGui::Checkbox("Show Model Preview", &m_ShowModelPreview);
		RenderHelpTooltip("Show additional model information and preview options");
	}

	void ModelSwitcherPanel::RenderModelPreview()
	{
		if (!ImGui::CollapsingHeader("Model Preview & Information")) {
			return;
		}

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		auto ttsNode = m_Manager->GetTTSNode();

		if (ttsNode && !weaponInfo.currentConfig->modelName.empty()) {
			ImGui::Text("Model File: %s", weaponInfo.currentConfig->modelName.c_str());

			// 显示文件信息
			try {
				std::filesystem::path modelPath = std::filesystem::current_path() /
				                                  "Data" / "Meshes" / "TTS" / "ScopeShape" / weaponInfo.currentConfig->modelName;

				if (std::filesystem::exists(modelPath)) {
					auto fileSize = std::filesystem::file_size(modelPath);
					auto lastWrite = std::filesystem::last_write_time(modelPath);

					ImGui::Text("File Size: %.2f KB", fileSize / 1024.0f);
					// Note: 时间格式化需要C++20的calendar功能，这里简化处理
					ImGui::Text("Last Modified: (file system time)");
				} else {
					ImGui::TextColored(m_ErrorColor, "File not found!");
				}
			} catch (const std::exception& e) {
				ImGui::TextColored(m_ErrorColor, "Error reading file info: %s", e.what());
			}

			ImGui::Separator();

			// TTSNode信息
			ImGui::Text("Node Information:");
			ImGui::BulletText("Position: [%.3f, %.3f, %.3f]",
				ttsNode->local.translate.x,
				ttsNode->local.translate.y,
				ttsNode->local.translate.z);

			float pitch, yaw, roll;
			ttsNode->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			ImGui::BulletText("Rotation: [%.1f, %.1f, %.1f] degrees",
				pitch * 57.2957795f,  // 弧度转度数
				yaw * 57.2957795f,
				roll * 57.2957795f);

			ImGui::BulletText("Scale: %.3f", ttsNode->local.scale);
		} else {
			ImGui::TextColored(m_WarningColor, "No model currently loaded");
		}
	}

	bool ModelSwitcherPanel::SwitchToModel(const std::string& modelName)
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		if (!weaponInfo.currentConfig) {
			return false;
		}

		try {
			// 创建修改后的配置
			auto modifiedConfig = *weaponInfo.currentConfig;
			modifiedConfig.modelName = modelName;

			// 保存配置
			auto dataPersistence = DataPersistence::GetSingleton();
			if (!dataPersistence->SaveConfig(modifiedConfig)) {
				return false;
			}

			// 重新加载配置
			dataPersistence->LoadAllConfigs();

			// 加载新模型
			return PreviewModel(modelName);

		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error switching model: {}", e.what()).c_str());
			return false;
		}
	}

	bool ModelSwitcherPanel::PreviewModel(const std::string& modelName)
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

			// 移除现有TTSNode
			auto existingTTSNode = weaponNiNode->GetObjectByName("TTSNode");
			if (existingTTSNode) {
				weaponNiNode->DetachChild(existingTTSNode);
			}

			// 加载新模型
			std::string fullPath = "Meshes\\TTS\\ScopeShape\\" + modelName;
			auto nifLoader = NIFLoader::GetSington();
			RE::NiNode* loadedNode = nifLoader->LoadNIF(fullPath.c_str());

			if (!loadedNode) {
				return false;
			}

			ScopeCamera::s_CurrentScopeNode = loadedNode;

			// 设置变换
			loadedNode->local.translate = RE::NiPoint3(0, 0, 7);
			loadedNode->local.rotate.MakeIdentity();
			loadedNode->local.scale = 1.5f;

			// 如果有当前配置，应用配置中的变换
			auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
			if (weaponInfo.currentConfig) {
				loadedNode->local.translate.x = weaponInfo.currentConfig->cameraAdjustments.deltaPosX;
				loadedNode->local.translate.y = weaponInfo.currentConfig->cameraAdjustments.deltaPosY;
				loadedNode->local.translate.z = weaponInfo.currentConfig->cameraAdjustments.deltaPosZ;

				float pitch = weaponInfo.currentConfig->cameraAdjustments.deltaRot[0] * 0.01745329251f;
				float yaw = weaponInfo.currentConfig->cameraAdjustments.deltaRot[1] * 0.01745329251f;
				float roll = weaponInfo.currentConfig->cameraAdjustments.deltaRot[2] * 0.01745329251f;

				RE::NiMatrix3 rotMat;
				rotMat.MakeIdentity();
				rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);
				loadedNode->local.rotate = rotMat;

				loadedNode->local.scale = weaponInfo.currentConfig->cameraAdjustments.deltaScale;
			}

			weaponNiNode->AttachChild(loadedNode, false);

			// 更新节点
			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				loadedNode->Update(updateData);
				weaponNiNode->Update(updateData);
			}

			return true;

		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error previewing model: {}", e.what()).c_str());
			return false;
		}
	}

	void ModelSwitcherPanel::ReloadCurrentModel()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		if (weaponInfo.currentConfig && !weaponInfo.currentConfig->modelName.empty()) {
			if (PreviewModel(weaponInfo.currentConfig->modelName)) {
				m_Manager->SetDebugText(fmt::format("Model reloaded: {}",
					weaponInfo.currentConfig->modelName)
						.c_str());
			} else {
				m_Manager->ShowErrorDialog("Reload Error", "Failed to reload the current model.");
			}
		}
	}

	void ModelSwitcherPanel::RemoveCurrentModel()
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

				m_Manager->SetDebugText("Current model removed");
			} else {
				m_Manager->SetDebugText("No model to remove");
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error removing model: {}", e.what()).c_str());
		}
	}

	void ModelSwitcherPanel::RefreshNIFFiles()
	{
		ScanForNIFFiles();
	}

	void ModelSwitcherPanel::ScanForNIFFiles()
	{
		m_AvailableNIFFiles.clear();

		try {
			std::filesystem::path dataPath = std::filesystem::current_path() / "Data" / "Meshes" / "TTS" / "ScopeShape";

			if (!std::filesystem::exists(dataPath)) {
				m_Manager->SetDebugText("ScopeShape directory not found!");
				return;
			}

			for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
				if (IsValidNIFFile(entry.path())) {
					std::string fileName = entry.path().filename().string();
					m_AvailableNIFFiles.push_back(fileName);
				}
			}

			// 按文件名排序
			std::sort(m_AvailableNIFFiles.begin(), m_AvailableNIFFiles.end());

			m_NIFFilesScanned = true;
			m_Manager->SetDebugText(fmt::format("Found {} NIF files", m_AvailableNIFFiles.size()).c_str());

		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error scanning NIF files: {}", e.what()).c_str());
		}
	}

	void ModelSwitcherPanel::OptimizedScan()
	{
		float currentTime = ImGui::GetTime();
		if (!m_NIFFilesScanned || currentTime > m_NextScanTime) {
			ScanForNIFFiles();
			m_NextScanTime = currentTime + SCAN_INTERVAL;
		}
	}

	int ModelSwitcherPanel::FindModelIndex(const std::string& modelName) const
	{
		auto it = std::find(m_AvailableNIFFiles.begin(), m_AvailableNIFFiles.end(), modelName);
		return it != m_AvailableNIFFiles.end() ? std::distance(m_AvailableNIFFiles.begin(), it) : -1;
	}

	bool ModelSwitcherPanel::IsValidNIFFile(const std::filesystem::path& filePath) const
	{
		return filePath.extension() == ".nif" &&
		       std::filesystem::file_size(filePath) > 0;
	}

	std::string ModelSwitcherPanel::GetModelDisplayName(const std::string& fileName, bool isCurrent) const
	{
		return isCurrent ? fileName + " (Current)" : fileName;
	}
}
