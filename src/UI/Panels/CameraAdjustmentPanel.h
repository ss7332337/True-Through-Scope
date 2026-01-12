#pragma once

#include "BasePanelInterface.h"
#include "ScopeCamera.h"
#include "D3DHooks.h"
#include <DataPersistence.h>
#include "../Localization/LocalizationManager.h"

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
        const char* GetPanelName() const override { return LOC("ui.menu.camera"); }
        const char* GetPanelID() const override { return "CameraAdjustment"; }
		bool GetSaved() const override { return isSaved; }
        
        // 获取当前调整值
        struct AdjustmentValues
        {
            float deltaPosX = 0.0f;
            float deltaPosY = 0.0f;
            float deltaPosZ = 7.0f;
            float deltaRot[3] = { 0.0f, 0.0f, 0.0f };  // Pitch, Yaw, Roll
            float deltaScale = 1.5f;
            
            // 新的视差设置
            float parallaxStrength = 0.05f;        // 视差偏移强度
            float parallaxSmoothing = 0.5f;        // 时域平滑
            float exitPupilRadius = 0.45f;         // 出瞳半径
            float exitPupilSoftness = 0.15f;       // 出瞳边缘柔和度
            float vignetteStrength = 0.3f;         // 晕影强度
            float vignetteRadius = 0.7f;           // 晕影起始半径
            float vignetteSoftness = 0.3f;         // 晕影柔和度
            float eyeReliefDistance = 0.5f;        // 眼距
            bool  enableParallax = true;           // 启用视差

			//新添加的检测项
			float nightVisionIntensity = 1.0f;
			float nightVisionNoiseScale = 0.05f;
			float nightVisionNoiseAmount = 0.05f;
			float nightVisionGreenTint = 1.2f;
			bool  enableNightVision = false;

			float thermalIntensity = 1.0f;
			float thermalThreshold = 0.5f;
			float thermalContrast = 1.2f;
			float thermalNoiseAmount = 0.03f;
			bool enableThermalVision = false;

			int minFov = 5;
			int maxFov = 100;

			// 球形畸变设置
			float sphericalDistortionStrength = 0.0f;
			float sphericalDistortionRadius = 0.8f;
			float sphericalDistortionCenterX = 0.0f;
			float sphericalDistortionCenterY = 0.0f;
			bool enableSphericalDistortion = false;
			bool enableChromaticAberration = false;
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
		void ApplySettings();
        
    private:
        PanelManagerInterface* m_Manager;
		bool isSaved = true;
        AdjustmentValues m_CurrentValues;
        AdjustmentValues m_PreviousValues;  // 用于变化检测
        
        bool m_RealTimeAdjustment = true;
        bool m_UIValuesInitialized = false;
        std::string m_LastLoadedConfigKey = "";

		//fov
		int m_MinFov = 5;
		int m_MaxFov = 100;
        
        // 新的视差设置
        float m_ParallaxStrength = 0.05f;
        float m_ParallaxSmoothing = 0.5f;
        float m_ExitPupilRadius = 0.45f;
        float m_ExitPupilSoftness = 0.15f;
        float m_VignetteStrength = 0.3f;
        float m_VignetteRadius = 0.7f;
        float m_VignetteSoftness = 0.3f;
        float m_EyeReliefDistance = 0.5f;
        bool  m_EnableParallax = true;

        // 夜视效果设置
        float m_NightVisionIntensity = 1.0f;
        float m_NightVisionNoiseScale = 0.05f;
        float m_NightVisionNoiseAmount = 0.05f;
        float m_NightVisionGreenTint = 1.2f;
        bool m_EnableNightVision = false;

        // 热成像效果设置
        float m_ThermalIntensity = 1.0f;
        float m_ThermalThreshold = 0.5f;
        float m_ThermalContrast = 1.2f;
        float m_ThermalNoiseAmount = 0.03f;
        bool m_EnableThermalVision = false;

        // 球形畸变效果设置
        float m_SphericalDistortionStrength = 0.0f;
        float m_SphericalDistortionRadius = 0.8f;
        float m_SphericalDistortionCenterX = 0.0f;
        float m_SphericalDistortionCenterY = 0.0f;
        bool m_EnableSphericalDistortion = false;
        bool m_EnableChromaticAberration = false;
        
        // 渲染函数
        void RenderWeaponInformation();
        void RenderConfigurationSection();
        void RenderAdjustmentControls();
        void RenderScopeSettings();
        void RenderParallaxSettings();
        void RenderNightVisionSettings();
        void RenderThermalVisionSettings();
        void RenderSphericalDistortionSettings();
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
        
        // 辅助函数
        void MarkSettingsChanged() { isSaved = false; }
    };
}
