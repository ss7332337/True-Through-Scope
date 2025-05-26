#pragma once

#include "BasePanelInterface.h"
#include "ScopeCamera.h"
#include "D3DHooks.h"

namespace ThroughScope
{
    class ZoomDataPanel : public BasePanelInterface
    {
    public:
        ZoomDataPanel(PanelManagerInterface* manager);
        ~ZoomDataPanel() override = default;
        
        // Base interface implementation
        void Render() override;
        void Update() override;
        bool Initialize() override;
        const char* GetPanelName() const override { return "Zoom Data Settings"; }
        
        // Current values access
        struct ZoomDataValues
        {
            float fovMult = 1.0f;
            float offsetX = 0.0f;
            float offsetY = 0.0f;
            float offsetZ = 0.0f;
        };
        
        const ZoomDataValues& GetCurrentValues() const { return m_CurrentValues; }
        void SetCurrentValues(const ZoomDataValues& values);
        void LoadFromConfig(const DataPersistence::ScopeConfig* config);
        void SaveToConfig(DataPersistence::ScopeConfig& config) const;
        
        // Reset controls
        void ResetAllSettings();
        
    private:
        PanelManagerInterface* m_Manager;
        ZoomDataValues m_CurrentValues;
        ZoomDataValues m_PreviousValues;  // For change detection
        
        bool m_UIValuesInitialized = false;
        std::string m_LastLoadedConfigKey = "";
        
        // Rendering functions
        void RenderWeaponInformation();
        void RenderZoomDataControls();
        void RenderActionButtons();
        
        // Apply changes
        void ApplyAllSettings();
        
        // Change detection
        bool HasChanges() const;
        void UpdatePreviousValues();
    };
}