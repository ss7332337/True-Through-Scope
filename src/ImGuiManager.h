#pragma once

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "ScopeCamera.h"
#include "D3DHooks.h"
#include "Constants.h"
#include "RenderUtilities.h"

#include "DataPersistence.h"

namespace ThroughScope
{
    class ImGuiManager
    {
    public:
        static ImGuiManager* GetSingleton();
        
        bool Initialize();
        void Shutdown();
        
        void Render();
        void Update();
        
        // Helper methods
        bool IsInitialized() const { return m_Initialized; }
        bool IsMenuOpen() const { return m_MenuOpen; }
        void ToggleMenu() { m_MenuOpen = !m_MenuOpen; }
        
    private:
        ImGuiManager() = default;
        ~ImGuiManager() = default;
        
        // UI sections
        void RenderMainMenu();
        void RenderCameraAdjustmentPanel();
        void RenderRenderingPanel();
        void RenderDebugPanel();    
		void RenderSettingsPanel();
		void RenderModelSwitcher();
		void RenderHelpTooltip(const char* text);
		// Helper functions for real-time adjustments
		void ApplyPositionAdjustment();
		void ApplyRotationAdjustment();
		void ApplyScaleAdjustment();
		void ResetAllAdjustments();
		RE::NiAVObject* GetTTSNode();

		void ScanForNIFFiles();
		bool CreateTTSNodeFromNIF(const ThroughScope::DataPersistence::ScopeConfig* config);
		bool CreateTTSNodeFromNIF(const std::string& nifFileName);
		void RemoveExistingTTSNode();
		bool AutoLoadTTSNodeFromConfig(const ThroughScope::DataPersistence::ScopeConfig* config);
		std::vector<std::string> GetNIFFileList() const { return m_AvailableNIFFiles; }
		void ShowResetConfirmationDialog();
		void CheckAutoSave();
		void RenderStatusBar();
		void OptimizedNIFScan();
		void ShowErrorDialog(const std::string& title, const std::string& message);
		
		void ApplyConfigToTTSNode(const ThroughScope::DataPersistence::ScopeConfig* config);
		void ApplyUIValuesToTTSNode();

        // State variables
        bool m_Initialized = false;
        bool m_MenuOpen = false;

		bool m_ConfigLoaded = false;             // 标记配置是否已加载
		bool m_UIValuesInitialized = false;      // 标记UI值是否已初始化
		std::string m_LastLoadedConfigKey = "";  // 上次加载的配置标识
        
		// Camera adjustment settings
		float m_DeltaPosX = 0.0f;
		float m_DeltaPosY = 0.0f;
		float m_DeltaPosZ = 6.5f;
		float m_DeltaRot[3] = { 0.0f, 0.0f, 0.0f };    // Pitch, Yaw, Roll in degrees
		float m_DeltaScale = 1.5f;                   // X, Y, Z scale factors

		float m_DeltaRelativeFogRadius = 0.5f;
		float m_DeltaScopeSwayAmount = 0.1f;
		float m_DeltaMaxTravel = 0.05f;
		float m_DeltaParallaxRadius = 0.3f;


		float m_AdjustmentSpeed = DEFAULT_ADJUSTMENT_SPEED;

		// Previous values for change detection
		float m_PrevDeltaPosX = 0.0f;
		float m_PrevDeltaPosY = 0.0f;
		float m_PrevDeltaPosZ = 6.5f;
		float m_PrevDeltaRot[3] = { 0.0f, 0.0f, 0.0f };
		float m_PrevDeltaScale = 1.5f;

		float m_PrevDeltaRelativeFogRadius = 0.5f;
		float m_PrevDeltaScopeSwayAmount = 0.1f;
		float m_PrevDeltaMaxTravel = 0.05f;
		float m_PrevDeltaParallaxRadius = 0.3f;

        bool m_EnableRendering = false;
		bool m_RealTimeAdjustment = true;
		bool m_AutoApply = true;

		std::vector<std::string> m_AvailableNIFFiles;
		int m_SelectedNIFIndex = 0;
		bool m_NIFFilesScanned = false;

		bool m_ShowHelpTooltips = true;    // 是否显示帮助提示
		bool m_AutoSaveEnabled = true;     // 是否启用自动保存
		float m_LastSaveTime = 0.0f;       // 上次保存时间
		bool m_HasUnsavedChanges = false;  // 是否有未保存的更改
		std::string m_LastUsedNIF = "";    // 记住上次使用的NIF文件
		bool m_ConfirmBeforeReset = true;  // 重置前确认

		// 性能优化
		float m_NextNIFScanTime = 0.0f;         // 下次扫描NIF文件的时间
		const float NIF_SCAN_INTERVAL = 10.0f;  // NIF文件扫描间隔（秒）
        
        // Debug variables
        char m_DebugText[1024] = "";
        
        // UI style settings
		ImVec4 m_AccentColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
		ImVec4 m_WarningColor = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
		ImVec4 m_SuccessColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    };
}
