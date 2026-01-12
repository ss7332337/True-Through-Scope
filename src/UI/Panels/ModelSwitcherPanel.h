#pragma once

#include "BasePanelInterface.h"
#include <filesystem>
#include "../Localization/LocalizationManager.h"

namespace ThroughScope
{
    class ModelSwitcherPanel : public BasePanelInterface
    {
    public:
        ModelSwitcherPanel(PanelManagerInterface* manager);
        ~ModelSwitcherPanel() override = default;
        
        // 基础接口实现
        void Render() override;
        void Update() override;
        bool Initialize() override;
        const char* GetPanelName() const override { return LOC("ui.menu.models"); }
        const char* GetPanelID() const override { return "ModelSwitcher"; }
        bool ShouldShow() const override;
		bool GetSaved() const override { return isSaved; }

        // 刷新NIF文件列表
        void RefreshNIFFiles();
        
        // 获取可用的NIF文件列表
        const std::vector<std::string>& GetAvailableNIFFiles() const { return m_AvailableNIFFiles; }
        
    private:
        PanelManagerInterface* m_Manager;
		bool isSaved = true;

        // NIF文件管理
        std::vector<std::string> m_AvailableNIFFiles;
        bool m_NIFFilesScanned = false;
        float m_NextScanTime = 0.0f;
        const float SCAN_INTERVAL = 10.0f;  // 10秒扫描间隔
        
        // UI状态
        int m_SelectedModelIndex = -1;
		//char m_SearchFilter[1024];
		std::string m_SearchFilter = "";

        bool m_ShowModelPreview = false;
        
        // 渲染函数
        void RenderCurrentModelInfo();
        void RenderModelSelection();
        void RenderQuickActions();
        
        // 模型操作
        bool SwitchToModel(const std::string& modelName);
        bool PreviewModel(const std::string& modelName);
        void ReloadCurrentModel();
        void RemoveCurrentModel();
        
        // NIF文件扫描
        void ScanForNIFFiles();
        void OptimizedScan();
        
        // 辅助函数
        int FindModelIndex(const std::string& modelName) const;
        bool IsValidNIFFile(const std::filesystem::path& filePath) const;
        std::string GetModelDisplayName(const std::string& fileName, bool isCurrent = false) const;
    };
}
