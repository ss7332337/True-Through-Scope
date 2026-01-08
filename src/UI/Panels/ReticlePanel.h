#pragma once

#include "BasePanelInterface.h"
#include <filesystem>
#include <vector>
#include <string>
#include "../Localization/LocalizationManager.h"

namespace ThroughScope
{
    class ReticlePanel : public BasePanelInterface
    {
    public:
        ReticlePanel(PanelManagerInterface* manager);
        ~ReticlePanel() override = default;
        
        // 基础接口实现
        void Render() override;
        void Update() override;
        bool Initialize() override;
        const char* GetPanelName() const override { return LOC("ui.menu.reticle"); }
        bool ShouldShow() const override;
		bool GetSaved() const override { return isSaved; }
        
        // 刷新纹理文件列表
        void RefreshReticleTextures();
        
        // 获取可用的纹理文件列表
        const std::vector<std::string>& GetAvailableTextures() const { return m_AvailableTextures; }
        
        // 纹理设置
        struct ReticleSettings
        {
            std::string texturePath;    // 相对路径
            float scale = 1.0f;         // 缩放 (0.1 - 32.0)
            float offsetX = 0.5f;       // X偏移 (0.0 - 1.0)
            float offsetY = 0.5f;       // Y偏移 (0.0 - 1.0)
        };
        
        // 获取当前设置
        const ReticleSettings& GetCurrentSettings() const { return m_CurrentSettings; }
		
        
    private:
        PanelManagerInterface* m_Manager;
		bool isSaved = true;
        
        // 纹理文件管理
        std::vector<std::string> m_AvailableTextures;
        bool m_TexturesScanned = false;
        float m_NextScanTime = 0.0f;
        const float SCAN_INTERVAL = 10.0f;  // 10秒扫描间隔
        
        // 当前设置
        ReticleSettings m_CurrentSettings;
        ReticleSettings m_BackupSettings;  // 备份用于取消操作
		ReticleSettings m_PreviousSettings;  // 备份用于取消操作
        bool m_HasUnsavedChanges = false;
        
        // UI状态
        std::string m_SearchFilter = "";
        bool m_ShowPreview = true;
        int m_SelectedTextureIndex = -1;
        
        // 预览相关
        void* m_PreviewTextureID = nullptr;  // ImGui纹理ID
        int m_PreviewWidth = 0;
        int m_PreviewHeight = 0;
        
        // 渲染函数
        void RenderCurrentReticleInfo();
        void RenderTextureSelection();
        void RenderReticleAdjustments();
        void RenderPreviewSection();
        void RenderQuickActions();
        
        // 纹理操作
        bool LoadTexture(const std::string& texturePath);
        bool ApplyReticleSettings(const ReticleSettings& settings);
        void SaveCurrentSettings();
        void LoadSettingsFromConfig();
        void ResetToDefaults();
        
        // 预览相关
        bool CreateTexturePreview(const std::string& texturePath);
        void ReleaseTexturePreview();
        
        // 纹理文件扫描
        void ScanForTextureFiles();
        void OptimizedScan();
        
        // 辅助函数
        int FindTextureIndex(const std::string& texturePath) const;
        bool IsValidTextureFile(const std::filesystem::path& filePath) const;
        std::string GetTextureDisplayName(const std::string& fileName, bool isCurrent = false) const;
        std::string GetFullTexturePath(const std::string& relativePath) const;
        
        // 设置比较
        bool AreSettingsEqual(const ReticleSettings& a, const ReticleSettings& b) const;
		bool HasReticleChanges() const;
		void UpdatePreviousSettings();
		void ApplyReticleSettingsRealtime();

        // 常量
        static constexpr float MIN_SCALE = 0.001f;
        static constexpr float MAX_SCALE = 32.000f;
        static constexpr float MIN_OFFSET = -1.0f;
        static constexpr float MAX_OFFSET = 1.0f;
        static constexpr int PREVIEW_SIZE = 256;  // 预览图像大小
    };
}
