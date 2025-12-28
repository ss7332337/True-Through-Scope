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

		RenderSectionHeader(LOC("models.current_model_info"));

		if (!weaponInfo.currentConfig) {
			ImGui::TextColored(m_WarningColor, LOC("models.no_config_available"));
			return;
		}

		ImGui::BeginGroup();
		if (!weaponInfo.currentConfig->modelName.empty()) {
			ImGui::TextColored(m_SuccessColor, LOC("models.current_model_label"), weaponInfo.currentConfig->modelName.c_str());

			// 显示模型状态
			auto ttsNode = m_Manager->GetTTSNode();
			if (ttsNode) {
				ImGui::Text(LOC("models.status_loaded"), 
					ttsNode->local.translate.x,
					ttsNode->local.translate.y,
					ttsNode->local.translate.z);
			} else {
				ImGui::TextColored(m_WarningColor, LOC("models.status_not_loaded"));

				if (ImGui::Button(LOC("models.auto_load_config"))) {
					if (PreviewModel(weaponInfo.currentConfig->modelName)) {
						m_Manager->SetDebugText(LOC("models.auto_load_success"));
					} else {
						m_Manager->SetDebugText(LOC("models.auto_load_failed"));
					}
				}
				RenderHelpTooltip(LOC("tooltip.auto_load_config"));
			}
		} else {
			ImGui::TextColored(m_WarningColor, LOC("models.no_model_assigned"));
		}
		ImGui::EndGroup();
	}

	void ModelSwitcherPanel::RenderModelSelection()
	{
		RenderSectionHeader(LOC("models.available_models"));

		if (m_AvailableNIFFiles.empty()) {
			ImGui::TextColored(m_WarningColor, LOC("models.no_models_found"));
			if (ImGui::Button(LOC("models.scan_models"))) {
				RefreshNIFFiles();
			}
			return;
		}

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		int currentModelIndex = FindModelIndex(weaponInfo.currentConfig->modelName);

		// 搜索过滤器
		ImGui::SetNextItemWidth(-100);
		ImGui::InputTextWithHint("##Search", LOC("models.search_placeholder"), &m_SearchFilter);
		ImGui::SameLine();
		if (ImGui::Button(LOC("button.clear"))) {
			m_SearchFilter.clear();
		}

		// 模型列表
		ImGui::Spacing();
		std::string previewText = currentModelIndex >= 0 ?
		                              GetModelDisplayName(m_AvailableNIFFiles[currentModelIndex], true) :
		                              LOC("models.select_model");

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
							m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.switch_success")), fileName).c_str());
						} else {
							m_Manager->ShowErrorDialog(LOC("models.switch_error_title"),
								LOC("models.switch_error_desc"));
						}
					}
				}

				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}

				// 悬停预览
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(LOC("tooltip.click_to_switch"), fileName.c_str());
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button(LOC("button.refresh"))) {
			RefreshNIFFiles();
		}
	}

	void ModelSwitcherPanel::RenderQuickActions()
	{
		RenderSectionHeader(LOC("models.quick_actions"));

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		// 重新加载当前模型
		if (ImGui::Button(LOC("models.reload_current"), ImVec2(-1, 0))) {
			ReloadCurrentModel();
		}
		RenderHelpTooltip(LOC("tooltip.reload_current"));

		// 移除当前模型
		if (ImGui::Button(LOC("models.remove_current"), ImVec2(-1, 0))) {
			RemoveCurrentModel();
		}
		RenderHelpTooltip(LOC("tooltip.remove_current"));
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
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.error_switching")), e.what()).c_str());
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
			auto nifLoader = NIFLoader::GetSingleton();
			RE::NiNode* loadedNode = nifLoader->LoadNIF(fullPath.c_str());

			if (!loadedNode) {
				return false;
			}

			// 设置节点名称，确保与渲染流程中的查找一致
			loadedNode->name = "TTSNode";
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
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.error_previewing")), e.what()).c_str());
			return false;
		}
	}

	void ModelSwitcherPanel::ReloadCurrentModel()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		if (weaponInfo.currentConfig && !weaponInfo.currentConfig->modelName.empty()) {
			if (PreviewModel(weaponInfo.currentConfig->modelName)) {
				m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.reload_success")),
					weaponInfo.currentConfig->modelName)
						.c_str());
			} else {
				m_Manager->ShowErrorDialog(LOC("models.reload_error_title"), LOC("models.reload_error_desc"));
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

				m_Manager->SetDebugText(LOC("models.remove_success"));
			} else {
				m_Manager->SetDebugText(LOC("models.no_model_to_remove"));
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.error_removing")), e.what()).c_str());
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
				m_Manager->SetDebugText(LOC("models.directory_not_found"));
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
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.files_found")), m_AvailableNIFFiles.size()).c_str());

		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("models.error_scanning")), e.what()).c_str());
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
		return isCurrent ? fileName + " " + LOC("models.current_suffix") : fileName;
	}
}
