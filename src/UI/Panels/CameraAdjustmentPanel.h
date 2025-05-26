#pragma once

#include "BasePanelInterface.h"
#include "ScopeCamera.h"
#include "D3DHooks.h"

namespace ThroughScope
{
    class CameraAdjustmentPanel : public BasePanelInterface
    {
    public:
        CameraAdjustmentPanel(PanelManagerInterface* manager);
        ~CameraAdjustmentPanel() override = default;
        
        // 基础接口实现
        void Render() override;
        void Update() override;
        bool Initialize() override;
        const char* GetPanelName() const override { return "Camera Adjustment"; }
        
        // 获取当前调整值
        struct AdjustmentValues
        {
            float deltaPosX = 0.0f;
            float deltaPosY = 0.0f;
            float deltaPosZ = 7.0f;
            float deltaRot[3] = { 0.0f, 0.0f, 0.0f };  // Pitch, Yaw, Roll
            float deltaScale = 1.5f;
            
            // 视差设置
            float relativeFogRadius = 0.5f;
            float scopeSwayAmount = 0.1f;
            float maxTravel = 0.05f;
            float parallaxRadius = 0.3f;
        };
        
        const AdjustmentValues& GetCurrentValues() const { return m_CurrentValues; }
        void SetCurrentValues(const AdjustmentValues& values);
        void LoadFromConfig(const DataPersistence::ScopeConfig* config);
        void SaveToConfig(DataPersistence::ScopeConfig& config) const;
        
        // 实时调整控制
        void SetRealTimeAdjustment(bool enabled) { m_RealTimeAdjustment = enabled; }
        bool IsRealTimeAdjustmentEnabled() const { return m_RealTimeAdjustment; }
        
        // 重置所有调整
        void ResetAllAdjustments();
        
    private:
        PanelManagerInterface* m_Manager;
        AdjustmentValues m_CurrentValues;
        AdjustmentValues m_PreviousValues;  // 用于变化检测
        
        bool m_RealTimeAdjustment = true;
        bool m_UIValuesInitialized = false;
        std::string m_LastLoadedConfigKey = "";
        
        // 渲染函数
        void RenderWeaponInformation();
        void RenderConfigurationSection();
        void RenderAdjustmentControls();
        void RenderScopeSettings();
        void RenderParallaxSettings();
        void RenderActionButtons();
        
        // 应用调整
        void ApplyPositionAdjustment();
        void ApplyRotationAdjustment();
        void ApplyScaleAdjustment();
        void ApplyAllAdjustments();
        
        // 检查变化
        bool HasChanges() const;
        void UpdatePreviousValues();
        
        // NIF文件管理
        void ScanForNIFFiles();
        bool CreateTTSNodeFromNIF(const std::string& nifFileName);
        bool CreateTTSNodeFromConfig(const DataPersistence::ScopeConfig* config);
        void RemoveExistingTTSNode();
        bool AutoLoadTTSNodeFromConfig(const DataPersistence::ScopeConfig* config);
        
        std::vector<std::string> m_AvailableNIFFiles;
        int m_SelectedNIFIndex = 0;
        bool m_NIFFilesScanned = false;
        
        // UI状态
        bool m_ShowAdvancedControls = false;
        bool m_ConfirmBeforeReset = true;
    };
}