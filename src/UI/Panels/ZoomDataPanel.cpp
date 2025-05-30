#include "ZoomDataPanel.h"

namespace ThroughScope
{
    ZoomDataPanel::ZoomDataPanel(PanelManagerInterface* manager) :
        m_Manager(manager)
    {
    }

    bool ZoomDataPanel::Initialize()
    {
        return true;
    }

    void ZoomDataPanel::Render()
    {
        auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

        RenderWeaponInformation();

        if (!weaponInfo.currentConfig) {
            ImGui::TextColored(m_WarningColor, "No configuration found for this weapon");
            return;
        }

        // Check if UI values need reinitialization
        std::string currentConfigKey = fmt::format("{:08X}_{}",
            weaponInfo.weaponFormID,
            weaponInfo.currentConfig->modelName);

        if (!m_UIValuesInitialized || m_LastLoadedConfigKey != currentConfigKey) {
            LoadFromConfig(weaponInfo.currentConfig);
            m_LastLoadedConfigKey = currentConfigKey;
            m_UIValuesInitialized = true;
        }

        RenderZoomDataControls();
        RenderActionButtons();
    }

    void ZoomDataPanel::Update()
    {
        if (HasChanges()) {
            ApplyAllSettings();
            UpdatePreviousValues();
            isSaved = false;
        }
    }

    void ZoomDataPanel::RenderWeaponInformation()
    {
        auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

        RenderSectionHeader(LOC("zoom.weapon_info"));

        if (!weaponInfo.weapon || !weaponInfo.instanceData) {
            ImGui::TextColored(m_WarningColor, LOC("zoom.no_weapon"));
            return;
        }

        ImGui::Text(LOC("zoom.weapon_label"), weaponInfo.weaponFormID, weaponInfo.weaponModName.c_str());

        if (weaponInfo.selectedModForm) {
            ImGui::Text(LOC("zoom.config_source_mod"),
                weaponInfo.selectedModForm->GetLocalFormID(),
                weaponInfo.selectedModForm->GetFile()->filename,
                weaponInfo.configSource.c_str());
        } else if (weaponInfo.currentConfig) {
            ImGui::Text(LOC("zoom.config_source_weapon"), weaponInfo.configSource.c_str());
        }

        ImGui::Spacing();
    }

    void ZoomDataPanel::RenderZoomDataControls()
    {
        RenderSectionHeader(LOC("zoom.settings_title"));

        // FOV Multiplier
        ImGui::SliderFloat(LOC("zoom.fov_multiplier"), &m_CurrentValues.fovMult, 0.1f, 5.0f, "%.2f");
        RenderHelpTooltip(LOC("tooltip.fov_multiplier"));

        // Position Offsets
        if (ImGui::CollapsingHeader(LOC("zoom.position_offsets"), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "OffsetColumns", false);

            // Left column - Sliders
            ImGui::Text(LOC("zoom.precise_adjustment"));
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##X", &m_CurrentValues.offsetX, -50.0f, 50.0f, "X: %.3f");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##Y", &m_CurrentValues.offsetY, -50.0f, 50.0f, "Y: %.3f");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##Z", &m_CurrentValues.offsetZ, -50.0f, 50.0f, "Z: %.3f");

            ImGui::NextColumn();

            // Right column - Fine adjustment buttons
            ImGui::Text(LOC("zoom.fine_tuning"));
            if (ImGui::Button("X-0.1", ImVec2(50, 0)))
                m_CurrentValues.offsetX -= 0.1f;
            ImGui::SameLine();
            if (ImGui::Button("X+0.1", ImVec2(50, 0)))
                m_CurrentValues.offsetX += 0.1f;

            if (ImGui::Button("Y-0.1", ImVec2(50, 0)))
                m_CurrentValues.offsetY -= 0.1f;
            ImGui::SameLine();
            if (ImGui::Button("Y+0.1", ImVec2(50, 0)))
                m_CurrentValues.offsetY += 0.1f;

            if (ImGui::Button("Z-0.1", ImVec2(50, 0)))
                m_CurrentValues.offsetZ -= 0.1f;
            ImGui::SameLine();
            if (ImGui::Button("Z+0.1", ImVec2(50, 0)))
                m_CurrentValues.offsetZ += 0.1f;

            ImGui::Columns(1);
        }
    }

    void ZoomDataPanel::RenderActionButtons()
    {
        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button(LOC("zoom.reset_settings"), ImVec2(120, 0))) {
            ResetAllSettings();
        }

        ImGui::SameLine();

        if (ImGui::Button(LOC("button.save"), ImVec2(120, 0))) {
            auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
            if (weaponInfo.currentConfig) {
                DataPersistence::ScopeConfig modifiedConfig = *weaponInfo.currentConfig;
                SaveToConfig(modifiedConfig);

                auto dataPersistence = DataPersistence::GetSingleton();
                if (dataPersistence->SaveConfig(modifiedConfig)) {
                    m_Manager->SetDebugText(LOC("zoom.settings_saved"));
                    dataPersistence->LoadAllConfigs();
                } else {
                    m_Manager->SetDebugText(LOC("zoom.settings_failed"));
                }
            }

			isSaved = true;
        }
    }

    void ZoomDataPanel::SetCurrentValues(const ZoomDataValues& values)
    {
        m_CurrentValues = values;
        UpdatePreviousValues();
    }

    void ZoomDataPanel::LoadFromConfig(const DataPersistence::ScopeConfig* config)
    {
        if (!config)
            return;

        m_CurrentValues.fovMult = config->zoomDataSettings.fovMult;
        m_CurrentValues.offsetX = config->zoomDataSettings.offsetX;
        m_CurrentValues.offsetY = config->zoomDataSettings.offsetY;
        m_CurrentValues.offsetZ = config->zoomDataSettings.offsetZ;

        UpdatePreviousValues();
    }

    void ZoomDataPanel::SaveToConfig(DataPersistence::ScopeConfig& config) const
    {
        config.zoomDataSettings.fovMult = m_CurrentValues.fovMult;
        config.zoomDataSettings.offsetX = m_CurrentValues.offsetX;
        config.zoomDataSettings.offsetY = m_CurrentValues.offsetY;
        config.zoomDataSettings.offsetZ = m_CurrentValues.offsetZ;
    }

    void ZoomDataPanel::ResetAllSettings()
    {
        m_CurrentValues.fovMult = 1.0f;
        m_CurrentValues.offsetX = 0.0f;
        m_CurrentValues.offsetY = 0.0f;
        m_CurrentValues.offsetZ = 0.0f;

        ApplyAllSettings();
        UpdatePreviousValues();
        m_Manager->SetDebugText(LOC("zoom.settings_reset"));
    }

    bool ZoomDataPanel::HasChanges() const
    {
        const float epsilon = 0.001f;
        return std::abs(m_CurrentValues.fovMult - m_PreviousValues.fovMult) > epsilon ||
               std::abs(m_CurrentValues.offsetX - m_PreviousValues.offsetX) > epsilon ||
               std::abs(m_CurrentValues.offsetY - m_PreviousValues.offsetY) > epsilon ||
               std::abs(m_CurrentValues.offsetZ - m_PreviousValues.offsetZ) > epsilon;
    }

    void ZoomDataPanel::UpdatePreviousValues()
    {
        m_PreviousValues = m_CurrentValues;
    }

    void ZoomDataPanel::ApplyAllSettings()
    {
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		if (!weaponInfo.currentConfig) {
			m_Manager->SetDebugText(LOC("zoom.no_config_loaded"));
			return;
		}

		if (weaponInfo.instanceData && weaponInfo.instanceData->zoomData)
		{
			weaponInfo.instanceData->zoomData->zoomData.fovMult = m_CurrentValues.fovMult;
			weaponInfo.instanceData->zoomData->zoomData.cameraOffset.x = m_CurrentValues.offsetX;
			weaponInfo.instanceData->zoomData->zoomData.cameraOffset.y = m_CurrentValues.offsetY;
			weaponInfo.instanceData->zoomData->zoomData.cameraOffset.z = m_CurrentValues.offsetZ;
		}
    }
}
