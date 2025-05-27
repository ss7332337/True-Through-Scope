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

        RenderSectionHeader("Current Weapon Information");

        if (!weaponInfo.weapon || !weaponInfo.instanceData) {
            ImGui::TextColored(m_WarningColor, "No valid weapon equipped");
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

        ImGui::Spacing();
    }

    void ZoomDataPanel::RenderZoomDataControls()
    {
        RenderSectionHeader("Zoom Data Settings");

        // FOV Multiplier
        ImGui::SliderFloat("FOV Multiplier", &m_CurrentValues.fovMult, 0.1f, 5.0f, "%.2f");
        RenderHelpTooltip("Multiplier applied to the field of view when zooming");

        // Position Offsets
        if (ImGui::CollapsingHeader("Position Offsets", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "OffsetColumns", false);

            // Left column - Sliders
            ImGui::Text("Precise Adjustment:");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##X", &m_CurrentValues.offsetX, -50.0f, 50.0f, "X: %.3f");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##Y", &m_CurrentValues.offsetY, -50.0f, 50.0f, "Y: %.3f");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##Z", &m_CurrentValues.offsetZ, -50.0f, 50.0f, "Z: %.3f");

            ImGui::NextColumn();

            // Right column - Fine adjustment buttons
            ImGui::Text("Fine Tuning:");
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

        if (ImGui::Button("Reset Settings", ImVec2(120, 0))) {
            ResetAllSettings();
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Settings", ImVec2(120, 0))) {
            auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
            if (weaponInfo.currentConfig) {
                DataPersistence::ScopeConfig modifiedConfig = *weaponInfo.currentConfig;
                SaveToConfig(modifiedConfig);

                auto dataPersistence = DataPersistence::GetSingleton();
                if (dataPersistence->SaveConfig(modifiedConfig)) {
                    m_Manager->SetDebugText("Zoom data settings saved successfully!");
                    dataPersistence->LoadAllConfigs();
                } else {
                    m_Manager->SetDebugText("Failed to save zoom data settings!");
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
        m_Manager->SetDebugText("Zoom data settings reset to defaults!");
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
			m_Manager->SetDebugText("No configuration loaded to apply zoom data settings.");
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
