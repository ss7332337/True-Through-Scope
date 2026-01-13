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
		RenderNightVisionSettings();

		RenderSphericalDistortionSettings();
		RenderActionButtons();
	}

	void CameraAdjustmentPanel::Update()
	{
		// 同步UI值到CurrentValues结构（无论是否启用实时调整）
		m_CurrentValues.sphericalDistortionStrength = m_SphericalDistortionStrength;
		m_CurrentValues.sphericalDistortionRadius = m_SphericalDistortionRadius;
		m_CurrentValues.sphericalDistortionCenterX = m_SphericalDistortionCenterX;
		m_CurrentValues.sphericalDistortionCenterY = m_SphericalDistortionCenterY;
		m_CurrentValues.enableSphericalDistortion = m_EnableSphericalDistortion;
		m_CurrentValues.enableChromaticAberration = m_EnableChromaticAberration;

		// 同步其他UI值
		m_CurrentValues.enableNightVision = m_EnableNightVision;

		m_CurrentValues.nightVisionIntensity = m_NightVisionIntensity;
		m_CurrentValues.nightVisionNoiseScale = m_NightVisionNoiseScale;
		m_CurrentValues.nightVisionNoiseAmount = m_NightVisionNoiseAmount;
		m_CurrentValues.nightVisionGreenTint = m_NightVisionGreenTint;


		// 无论是否启用实时调整，都应用设置以确保D3DHooks获得最新参数
		ApplySettings();

		if (!m_RealTimeAdjustment)
			return;

		if (HasChanges()) {
			ApplyAllAdjustments();
			UpdatePreviousValues();
			isSaved = false;
		}
	}

	void CameraAdjustmentPanel::RenderWeaponInformation()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		RenderSectionHeader(LOC("camera.weapon_info"));

		if (!weaponInfo.weapon || !weaponInfo.instanceData) {
			ImGui::TextColored(m_WarningColor, LOC("camera.no_weapon"));
			ImGui::TextWrapped(LOC("camera.equip_weapon"));
			return;
		}

		ImGui::Text(LOCF("camera.weapon_label", weaponInfo.weaponFormID, weaponInfo.weaponModName.c_str()));

		if (weaponInfo.selectedModForm) {
			ImGui::Text(LOCF("camera.config_source",
				weaponInfo.selectedModForm->GetLocalFormID(),
				weaponInfo.selectedModForm->GetFile()->filename,
				weaponInfo.configSource.c_str()));
		} else if (weaponInfo.currentConfig) {
			ImGui::Text(LOCF("camera.config_source", 0, "Weapon", weaponInfo.configSource.c_str()));
		}

		if (weaponInfo.currentConfig && !weaponInfo.currentConfig->modelName.empty()) {
			ImGui::Text(LOCF("camera.current_model", weaponInfo.currentConfig->modelName.c_str()));

			ImGui::SameLine();
			if (ImGui::Button(LOC("camera.reload_tts"))) {
				if (CreateTTSNodeFromConfig(weaponInfo.currentConfig)) {
					m_Manager->SetDebugText(LOCF("status.tts_reloaded",
						weaponInfo.currentConfig->modelName.c_str()));
				} else {
					m_Manager->SetDebugText(LOCF("status.tts_reload_failed",
						weaponInfo.currentConfig->modelName.c_str()));
				}
			}
			RenderHelpTooltip(LOC("camera.reload_tooltip"));
		}

		ImGui::Spacing();
	}

	void CameraAdjustmentPanel::RenderConfigurationSection()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		ImGui::Separator();
		ImGui::TextColored(m_WarningColor, LOC("camera.config.no_config"));
		ImGui::TextWrapped(LOC("camera.config.create_desc"));

		ScanForNIFFiles();

		// Configuration target selection
		ImGui::Spacing();
		ImGui::TextColored(m_AccentColor, LOC("camera.config.target"));

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
			comboLabel = LOC("camera.config.base_weapon");
		} else if (createOption <= static_cast<int>(weaponInfo.availableMods.size())) {
			auto modForm = weaponInfo.availableMods[createOption - 1];

			// 使用改进的EditorID获取逻辑
			std::string editorIdStr;
			const char* editorID = modForm->GetFormEditorID();
			if (editorID && strlen(editorID) > 0) {
				editorIdStr = editorID;
			} else {
				// 尝试反查全局EditorID映射（使用锁保护）
				try {
					auto [editorIDMap, lockRef] = RE::TESForm::GetAllFormsByEditorID();
					if (editorIDMap) {
						RE::BSAutoReadLock lock{ lockRef.get() };
						for (auto& [key, value] : *editorIDMap) {
							if (value == modForm) {
								editorIdStr = key.c_str();
								break;
							}
						}
					}
				} catch (...) {
					// 忽略异常
				}

				// 如果还是没有EditorID，尝试获取FullName
				if (editorIdStr.empty()) {
					if (auto mod = modForm->As<RE::BGSMod::Attachment::Mod>()) {
						if (mod->fullName.c_str() && strlen(mod->fullName.c_str()) > 0) {
							editorIdStr = fmt::format("{}[{:08X}]", mod->fullName.c_str(), modForm->GetLocalFormID());
						}
					}
				}

				// 最后的备选方案
				if (editorIdStr.empty()) {
					editorIdStr = fmt::format("OMOD[{:08X}]", modForm->GetLocalFormID());
				}
			}

			comboLabel = fmt::format(fmt::runtime(LOC("camera.config.modification")), createOption, editorIdStr);
		} else {
			comboLabel = LOC("camera.invalid_selection");
			createOption = 0;  // Force reset to base weapon
		}

		if (ImGui::BeginCombo("Create Config For", comboLabel.c_str())) {
			// Base weapon option
			if (ImGui::Selectable(LOC("camera.config.base_weapon"), createOption == 0)) {
				createOption = 0;
			}
			RenderHelpTooltip(LOC("tooltip.base_weapon"));

			// Modification options
			for (size_t i = 0; i < weaponInfo.availableMods.size(); i++) {
				auto modForm = weaponInfo.availableMods[i];
				// 获取EditorID的改进方法
				auto getEditorID = [](RE::TESForm* form) -> std::string {
					if (!form) return "<null>";

					// 方法1: 直接获取EditorID
					const char* editorID = form->GetFormEditorID();
					if (editorID && strlen(editorID) > 0) {
						return std::string(editorID);
					}

					// 方法2: 尝试通过全局EditorID映射反查（使用锁保护）
					try {
						auto [editorIDMap, lockRef] = RE::TESForm::GetAllFormsByEditorID();
						if (editorIDMap) {
							RE::BSAutoReadLock lock{ lockRef.get() };
							for (auto& [key, value] : *editorIDMap) {
								if (value == form) {
									return std::string(key.c_str());
								}
							}
						}
					} catch (...) {
						// 忽略异常
					}

					// 方法3: 如果是BGSMod::Attachment::Mod，尝试获取TESFullName
					if (auto mod = form->As<RE::BGSMod::Attachment::Mod>()) {
						// BGSMod::Attachment::Mod 继承自 TESFullName
						if (mod->fullName.c_str() && strlen(mod->fullName.c_str()) > 0) {
							return fmt::format("{}[{:08X}]", mod->fullName.c_str(), form->GetLocalFormID());
						}
					}

					// 方法4: 返回FormID和FormType
					return fmt::format("OMOD[{:08X}]", form->GetLocalFormID());
				};

				std::string editorIdStr = getEditorID(modForm);
				std::string label = fmt::format(fmt::runtime(LOC("camera.config.modification")),
					i + 1, editorIdStr);

				if (ImGui::Selectable(label.c_str(), createOption == static_cast<int>(i + 1))) {
					createOption = static_cast<int>(i + 1);
				}
				RenderHelpTooltip(LOC("tooltip.modification"));
			}

			ImGui::EndCombo();
		}

		// NIF File Selection
		ImGui::Spacing();
		ImGui::TextColored(m_AccentColor, LOC("camera.config.scope_shape"));

		if (m_AvailableNIFFiles.empty()) {
			ImGui::TextColored(m_WarningColor, LOC("camera.config.no_nif_files"));
			if (ImGui::Button(LOC("button.rescan"))) {
				m_NIFFilesScanned = false;
				ScanForNIFFiles();
			}
		} else {
			const char* currentNIFName = m_SelectedNIFIndex < m_AvailableNIFFiles.size() ?
			                                 m_AvailableNIFFiles[m_SelectedNIFIndex].c_str() :
			                                 LOC("camera.config.select_model");

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

		if (ImGui::Button(LOC("button.create"), ImVec2(-1, 0))) {
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
			ImGui::TextColored(m_WarningColor, LOC("camera.select_model_warning"));
		}
	}

	void CameraAdjustmentPanel::RenderAdjustmentControls()
	{
		RenderSectionHeader(LOC("camera.position"));

		// Position controls
		if (ImGui::CollapsingHeader(LOC("camera.position_controls"), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "PositionColumns", false);

			// Left column - Sliders
			ImGui::Text(LOC("camera.precise_adjustment"));
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##X", &m_CurrentValues.deltaPosX, -50.0f, 50.0f, "X: %.3f");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Y", &m_CurrentValues.deltaPosY, -50.0f, 50.0f, "Y: %.3f");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Z", &m_CurrentValues.deltaPosZ, -50.0f, 50.0f, "Z: %.3f");

			ImGui::NextColumn();

			// Right column - Fine adjustment buttons
			ImGui::Text(LOC("camera.fine_tuning"));

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
		if (ImGui::CollapsingHeader(LOC("camera.rotation_controls"), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "RotationColumns", false);

			ImGui::Text(LOC("camera.precise_adjustment"));
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Pitch", &m_CurrentValues.deltaRot[0], -180.0f, 180.0f, "Pitch: %.1f°");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Yaw", &m_CurrentValues.deltaRot[1], -180.0f, 180.0f, "Yaw: %.1f°");
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Roll", &m_CurrentValues.deltaRot[2], -180.0f, 180.0f, "Roll: %.1f°");

			ImGui::NextColumn();

			ImGui::Text(LOC("camera.fine_tuning"));

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
		if (ImGui::CollapsingHeader(LOC("camera.scale_controls"), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(2, "ScaleColumns", false);

			ImGui::Text(LOC("camera.scale"));
			ImGui::SetNextItemWidth(-1);
			ImGui::SliderFloat("##Scale", &m_CurrentValues.deltaScale, 0.1f, 10.0f, "%.3f");

			ImGui::NextColumn();

			ImGui::Text(LOC("camera.fine_tuning"));
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
		if (ImGui::CollapsingHeader(LOC("camera.scope_settings"))) {
			auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
			if (weaponInfo.currentConfig) {

				ImGui::SliderInt(LOC("camera.min_fov"), &m_MinFov, 1, 180);
				ImGui::SliderInt(LOC("camera.max_fov"), &m_MaxFov, 2, 180);
				ImGui::Checkbox(LOC("camera.night_vision"), &m_EnableNightVision);
				ImGui::SameLine();
				ImGui::Text(LOC("camera.night_vision_Tips"));

			}
		}
	}

	void CameraAdjustmentPanel::RenderParallaxSettings()
	{
		if (ImGui::CollapsingHeader(LOC("camera.parallax"), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox(LOC("camera.enable_parallax"), &m_CurrentValues.enableParallax);

			if (m_CurrentValues.enableParallax) {
				ImGui::Separator();
				ImGui::Text("%s", LOC("camera.parallax_offset"));
				ImGui::SliderFloat(LOC("camera.parallax_strength"), &m_CurrentValues.parallaxStrength, 0.0f, 0.2f, "%.3f");
				ImGui::SliderFloat(LOC("camera.parallax_smoothing"), &m_CurrentValues.parallaxSmoothing, 0.0f, 1.0f, "%.2f");
				ImGui::SliderFloat(LOC("camera.eye_relief"), &m_CurrentValues.eyeReliefDistance, 0.0f, 2.0f, "%.2f");

				ImGui::Separator();
				ImGui::Text("%s", LOC("camera.exit_pupil"));
				ImGui::SliderFloat(LOC("camera.exit_pupil_radius"), &m_CurrentValues.exitPupilRadius, 0.2f, 0.8f, "%.2f");
				ImGui::SliderFloat(LOC("camera.exit_pupil_softness"), &m_CurrentValues.exitPupilSoftness, 0.05f, 0.5f, "%.2f");

				ImGui::Separator();
				ImGui::Text("%s", LOC("camera.vignette"));
				ImGui::SliderFloat(LOC("camera.vignette_strength"), &m_CurrentValues.vignetteStrength, 0.0f, 1.0f, "%.2f");
				ImGui::SliderFloat(LOC("camera.vignette_radius"), &m_CurrentValues.vignetteRadius, 0.3f, 1.0f, "%.2f");
				ImGui::SliderFloat(LOC("camera.vignette_softness"), &m_CurrentValues.vignetteSoftness, 0.05f, 0.5f, "%.2f");
			}
		}
	}

	void CameraAdjustmentPanel::RenderNightVisionSettings()
	{
		if (ImGui::CollapsingHeader(LOC("camera.night_vision_settings"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox(LOC("camera.enable_night_vision"), &m_EnableNightVision);
			ImGui::PushID("NightVision");
			if (m_EnableNightVision)
			{
				ImGui::SliderFloat(LOC("camera.intensity"), &m_NightVisionIntensity, 1.0f, 10.0f, "%.2f");
				ImGui::SliderFloat(LOC("camera.noise_scale"), &m_NightVisionNoiseScale, 0.01f, 0.2f, "%.3f");
				ImGui::SliderFloat(LOC("camera.noise_amount"), &m_NightVisionNoiseAmount, 0.0f, 0.2f, "%.3f");
				ImGui::SliderFloat(LOC("camera.green_tint"), &m_NightVisionGreenTint, 0.0f, 2.0f, "%.2f");
			}
			ImGui::PopID();
		}
	}



	void CameraAdjustmentPanel::RenderSphericalDistortionSettings()
	{
		if (ImGui::CollapsingHeader(LOC("camera.distortion_settings"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox(LOC("camera.enable_distortion"), &m_EnableSphericalDistortion);
			ImGui::SameLine();
			ImGui::Text(LOC("camera.distortion_tooltip"));
			
			ImGui::PushID("SphericalDistortion");
			if (m_EnableSphericalDistortion)
			{
				ImGui::SliderFloat(LOC("camera.distortion_strength"), &m_SphericalDistortionStrength, -0.5f, 0.5f, "%.3f");
				ImGui::Text(LOC("camera.distortion_strength_desc"));
				
				ImGui::SliderFloat(LOC("camera.distortion_radius"), &m_SphericalDistortionRadius, 0.1f, 1.0f, "%.2f");
				ImGui::Text(LOC("camera.distortion_control_range"));
				
				ImGui::Text(LOC("camera.center_offset"));
				ImGui::SliderFloat(LOC("camera.offset_x"), &m_SphericalDistortionCenterX, -0.5f, 0.5f, "%.3f");
				ImGui::SliderFloat(LOC("camera.offset_y"), &m_SphericalDistortionCenterY, -0.5f, 0.5f, "%.3f");
				
				ImGui::Separator();
				ImGui::Checkbox(LOC("camera.enable_chromatic"), &m_EnableChromaticAberration);
				ImGui::SameLine();
				ImGui::Text(LOC("camera.chromatic_tooltip"));
				
				ImGui::Separator();
				ImGui::Text(LOC("camera.presets"));
				
				// Preset Buttons
				if (ImGui::Button(LOC("camera.preset_barrel_slight"))) {
					m_SphericalDistortionStrength = 0.1f;
					m_SphericalDistortionRadius = 0.8f;
					m_SphericalDistortionCenterX = 0.0f;
					m_SphericalDistortionCenterY = 0.0f;
				}
				ImGui::SameLine();
				if (ImGui::Button(LOC("camera.preset_barrel_medium"))) {
					m_SphericalDistortionStrength = 0.2f;
					m_SphericalDistortionRadius = 0.9f;
					m_SphericalDistortionCenterX = 0.0f;
					m_SphericalDistortionCenterY = 0.0f;
				}
				ImGui::SameLine();
				if (ImGui::Button(LOC("camera.preset_barrel_strong"))) {
					m_SphericalDistortionStrength = 0.3f;
					m_SphericalDistortionRadius = 1.0f;
					m_SphericalDistortionCenterX = 0.0f;
					m_SphericalDistortionCenterY = 0.0f;
				}
				
				if (ImGui::Button(LOC("camera.preset_pincushion_slight"))) {
					m_SphericalDistortionStrength = -0.1f;
					m_SphericalDistortionRadius = 0.8f;
					m_SphericalDistortionCenterX = 0.0f;
					m_SphericalDistortionCenterY = 0.0f;
				}
				ImGui::SameLine();
				if (ImGui::Button(LOC("camera.preset_pincushion_medium"))) {
					m_SphericalDistortionStrength = -0.2f;
					m_SphericalDistortionRadius = 0.9f;
					m_SphericalDistortionCenterX = 0.0f;
					m_SphericalDistortionCenterY = 0.0f;
				}
				ImGui::SameLine();
				if (ImGui::Button(LOC("camera.reset_distortion"))) {
					m_SphericalDistortionStrength = 0.0f;
					m_SphericalDistortionRadius = 0.8f;
					m_SphericalDistortionCenterX = 0.0f;
					m_SphericalDistortionCenterY = 0.0f;
					m_EnableSphericalDistortion = false;
					m_EnableChromaticAberration = false;
				}
			}
			ImGui::PopID();
		}
	}

	void CameraAdjustmentPanel::RenderActionButtons()
	{
		ImGui::Spacing();
		ImGui::Separator();

		if (ImGui::Button(LOC("camera.reset_adjustments"))) {
			if (m_ConfirmBeforeReset) {
				// Show confirmation dialog
				ImGui::OpenPopup(LOC("camera.reset_confirm_title"));
			} else {
				ResetAllAdjustments();
			}
		}

		// Reset confirmation modal
		if (ImGui::BeginPopupModal(LOC("camera.reset_confirm_title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text(LOC("camera.reset_confirm_text"));
			ImGui::Text(LOC("camera.reset_confirm_desc"));
			ImGui::Spacing();

			if (ImGui::Button(LOC("camera.yes_reset"), ImVec2(120, 0))) {
				ResetAllAdjustments();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button(LOC("button.cancel"), ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button(LOC("button.save"))) {
			auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
			if (weaponInfo.currentConfig) {
				DataPersistence::ScopeConfig modifiedConfig = *weaponInfo.currentConfig;
				SaveToConfig(modifiedConfig);

				auto dataPersistence = DataPersistence::GetSingleton();
				if (dataPersistence->SaveConfig(modifiedConfig)) {
					m_Manager->SetDebugText(LOC("status.settings_saved"));
					dataPersistence->LoadAllConfigs();
					isSaved = true;
				} else {
					m_Manager->SetDebugText(LOC("status.settings_failed"));
				}
			}
		}
	}

	void CameraAdjustmentPanel::SetCurrentValues(const AdjustmentValues& values)
	{
		m_CurrentValues = values;
		UpdatePreviousValues();
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

		config.parallaxSettings.parallaxStrength = m_CurrentValues.parallaxStrength;
		config.parallaxSettings.parallaxSmoothing = m_CurrentValues.parallaxSmoothing;
		config.parallaxSettings.exitPupilRadius = m_CurrentValues.exitPupilRadius;
		config.parallaxSettings.exitPupilSoftness = m_CurrentValues.exitPupilSoftness;
		config.parallaxSettings.vignetteStrength = m_CurrentValues.vignetteStrength;
		config.parallaxSettings.vignetteRadius = m_CurrentValues.vignetteRadius;
		config.parallaxSettings.vignetteSoftness = m_CurrentValues.vignetteSoftness;
		config.parallaxSettings.eyeReliefDistance = m_CurrentValues.eyeReliefDistance;
		config.parallaxSettings.enableParallax = m_CurrentValues.enableParallax;

		// 保存夜视设置
		config.scopeSettings.nightVision = m_EnableNightVision;
		config.scopeSettings.nightVisionIntensity = m_NightVisionIntensity;
		config.scopeSettings.nightVisionNoiseScale = m_NightVisionNoiseScale;
		config.scopeSettings.nightVisionNoiseAmount = m_NightVisionNoiseAmount;
		config.scopeSettings.nightVisionGreenTint = m_NightVisionGreenTint;


		config.scopeSettings.minFOV = m_MinFov;
		config.scopeSettings.maxFOV = m_MaxFov;

		// 保存球形畸变设置
		config.scopeSettings.enableSphericalDistortion = m_EnableSphericalDistortion;
		config.scopeSettings.enableChromaticAberration = m_EnableChromaticAberration;
		config.scopeSettings.sphericalDistortionStrength = m_SphericalDistortionStrength;
		config.scopeSettings.sphericalDistortionRadius = m_SphericalDistortionRadius;
		config.scopeSettings.sphericalDistortionCenterX = m_SphericalDistortionCenterX;
		config.scopeSettings.sphericalDistortionCenterY = m_SphericalDistortionCenterY;

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

		m_CurrentValues.parallaxStrength = config->parallaxSettings.parallaxStrength;
		m_CurrentValues.parallaxSmoothing = config->parallaxSettings.parallaxSmoothing;
		m_CurrentValues.exitPupilRadius = config->parallaxSettings.exitPupilRadius;
		m_CurrentValues.exitPupilSoftness = config->parallaxSettings.exitPupilSoftness;
		m_CurrentValues.vignetteStrength = config->parallaxSettings.vignetteStrength;
		m_CurrentValues.vignetteRadius = config->parallaxSettings.vignetteRadius;
		m_CurrentValues.vignetteSoftness = config->parallaxSettings.vignetteSoftness;
		m_CurrentValues.eyeReliefDistance = config->parallaxSettings.eyeReliefDistance;
		m_CurrentValues.enableParallax = config->parallaxSettings.enableParallax;

		// 加载夜视设置
		m_EnableNightVision = config->scopeSettings.nightVision;
		m_NightVisionIntensity = config->scopeSettings.nightVisionIntensity;
		m_NightVisionNoiseScale = config->scopeSettings.nightVisionNoiseScale;
		m_NightVisionNoiseAmount = config->scopeSettings.nightVisionNoiseAmount;
		m_NightVisionGreenTint = config->scopeSettings.nightVisionGreenTint;



		m_MinFov = config->scopeSettings.minFOV;
		m_MaxFov = config->scopeSettings.maxFOV;

		// 加载球形畸变设置
		m_EnableSphericalDistortion = config->scopeSettings.enableSphericalDistortion;
		m_EnableChromaticAberration = config->scopeSettings.enableChromaticAberration;
		m_SphericalDistortionStrength = config->scopeSettings.sphericalDistortionStrength;
		m_SphericalDistortionRadius = config->scopeSettings.sphericalDistortionRadius;
		m_SphericalDistortionCenterX = config->scopeSettings.sphericalDistortionCenterX;
		m_SphericalDistortionCenterY = config->scopeSettings.sphericalDistortionCenterY;

		// 同步到CurrentValues
		m_CurrentValues.sphericalDistortionStrength = m_SphericalDistortionStrength;
		m_CurrentValues.sphericalDistortionRadius = m_SphericalDistortionRadius;
		m_CurrentValues.sphericalDistortionCenterX = m_SphericalDistortionCenterX;
		m_CurrentValues.sphericalDistortionCenterY = m_SphericalDistortionCenterY;
		m_CurrentValues.enableSphericalDistortion = m_EnableSphericalDistortion;
		m_CurrentValues.enableChromaticAberration = m_EnableChromaticAberration;

		ApplySettings();

		UpdatePreviousValues();

		// 应用到TTSNode
		auto ttsNode = m_Manager->GetTTSNode();
		if (ttsNode) {
			ApplyAllAdjustments();
		}
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

		// 重置球形畸变设置
		m_SphericalDistortionStrength = 0.0f;
		m_SphericalDistortionRadius = 0.8f;
		m_SphericalDistortionCenterX = 0.0f;
		m_SphericalDistortionCenterY = 0.0f;
		m_EnableSphericalDistortion = false;
		m_EnableChromaticAberration = false;

		m_CurrentValues.sphericalDistortionStrength = m_SphericalDistortionStrength;
		m_CurrentValues.sphericalDistortionRadius = m_SphericalDistortionRadius;
		m_CurrentValues.sphericalDistortionCenterX = m_SphericalDistortionCenterX;
		m_CurrentValues.sphericalDistortionCenterY = m_SphericalDistortionCenterY;
		m_CurrentValues.enableSphericalDistortion = m_EnableSphericalDistortion;
		m_CurrentValues.enableChromaticAberration = m_EnableChromaticAberration;

		if (m_RealTimeAdjustment) {
			ApplyAllAdjustments();
		}

		ApplySettings(); // 应用重置后的设置
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
		       std::abs(m_CurrentValues.parallaxStrength - m_PreviousValues.parallaxStrength) > epsilon ||
		       std::abs(m_CurrentValues.parallaxSmoothing - m_PreviousValues.parallaxSmoothing) > epsilon ||
		       std::abs(m_CurrentValues.exitPupilRadius - m_PreviousValues.exitPupilRadius) > epsilon ||
		       std::abs(m_CurrentValues.exitPupilSoftness - m_PreviousValues.exitPupilSoftness) > epsilon ||
		       std::abs(m_CurrentValues.vignetteStrength - m_PreviousValues.vignetteStrength) > epsilon ||
		       std::abs(m_CurrentValues.vignetteRadius - m_PreviousValues.vignetteRadius) > epsilon ||
		       std::abs(m_CurrentValues.vignetteSoftness - m_PreviousValues.vignetteSoftness) > epsilon ||
		       std::abs(m_CurrentValues.eyeReliefDistance - m_PreviousValues.eyeReliefDistance) > epsilon ||
		       m_CurrentValues.enableParallax != m_PreviousValues.enableParallax ||

		       std::abs(m_CurrentValues.nightVisionIntensity - m_PreviousValues.nightVisionIntensity) > epsilon ||
		       std::abs(m_CurrentValues.nightVisionNoiseScale - m_PreviousValues.nightVisionNoiseScale) > epsilon ||
		       std::abs(m_CurrentValues.nightVisionNoiseAmount - m_PreviousValues.nightVisionNoiseAmount) > epsilon ||
		       std::abs(m_CurrentValues.nightVisionGreenTint - m_PreviousValues.nightVisionGreenTint) > epsilon ||
		       m_CurrentValues.enableNightVision != m_PreviousValues.enableNightVision ||

		       // 球形畸变参数变化检测
		       std::abs(m_CurrentValues.sphericalDistortionStrength - m_PreviousValues.sphericalDistortionStrength) > epsilon ||
		       std::abs(m_CurrentValues.sphericalDistortionRadius - m_PreviousValues.sphericalDistortionRadius) > epsilon ||
		       std::abs(m_CurrentValues.sphericalDistortionCenterX - m_PreviousValues.sphericalDistortionCenterX) > epsilon ||
		       std::abs(m_CurrentValues.sphericalDistortionCenterY - m_PreviousValues.sphericalDistortionCenterY) > epsilon ||
		       m_CurrentValues.enableSphericalDistortion != m_PreviousValues.enableSphericalDistortion ||
		       m_CurrentValues.enableChromaticAberration != m_PreviousValues.enableChromaticAberration;
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

		ScopeCamera::SetFOVMinMax(m_CurrentValues.minFov, m_CurrentValues.maxFov);

		// 应用新的视差设置
		D3DHooks::UpdateScopeParallaxSettings(
			m_CurrentValues.parallaxStrength,
			m_CurrentValues.exitPupilRadius,
			m_CurrentValues.vignetteStrength,
			m_CurrentValues.vignetteRadius);

		D3DHooks::UpdateParallaxAdvancedSettings(
			m_CurrentValues.parallaxSmoothing,
			m_CurrentValues.exitPupilSoftness,
			m_CurrentValues.vignetteSoftness,
			m_CurrentValues.eyeReliefDistance,
			m_CurrentValues.enableParallax ? 1 : 0);
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
			auto nifLoader = NIFLoader::GetSingleton();
			RE::NiNode* loadedNode = nifLoader->LoadNIF(fullPath.c_str());

			if (!loadedNode) {
				return false;
			}

			// 设置节点名称，确保与渲染流程中的查找一致
			loadedNode->name = "TTSNode";
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

	void CameraAdjustmentPanel::ApplySettings()
	{
		// 应用新的视差设置
		D3DHooks::UpdateScopeParallaxSettings(
			m_CurrentValues.parallaxStrength,
			m_CurrentValues.exitPupilRadius,
			m_CurrentValues.vignetteStrength,
			m_CurrentValues.vignetteRadius
		);

		D3DHooks::UpdateParallaxAdvancedSettings(
			m_CurrentValues.parallaxSmoothing,
			m_CurrentValues.exitPupilSoftness,
			m_CurrentValues.vignetteSoftness,
			m_CurrentValues.eyeReliefDistance,
			m_CurrentValues.enableParallax ? 1 : 0
		);

		// 应用夜视设置
		D3DHooks::SetEnableNightVision(m_EnableNightVision);
		D3DHooks::UpdateNightVisionSettings(
			m_NightVisionIntensity,
			m_NightVisionNoiseScale,
			m_NightVisionNoiseAmount,
			m_NightVisionGreenTint
		);



		// 应用球形畸变设置
		D3DHooks::UpdateSphericalDistortionSettings(
			m_SphericalDistortionStrength,
			m_SphericalDistortionRadius,
			m_SphericalDistortionCenterX,
			m_SphericalDistortionCenterY
		);
		
		// 设置球形畸变开关
		D3DHooks::SetEnableSphericalDistortion(m_EnableSphericalDistortion);
		D3DHooks::SetEnableChromaticAberration(m_EnableChromaticAberration);
		
		// 强制更新常量缓冲区以确保实时响应
		D3DHooks::ForceConstantBufferUpdate();
	}
}
