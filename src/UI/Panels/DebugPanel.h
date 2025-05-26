#pragma once

#include "BasePanelInterface.h"
#include "ScopeCamera.h"
#include "D3DHooks.h"
#include "Utilities.h"

namespace ThroughScope
{
    class DebugPanel : public BasePanelInterface
    {
    public:
        DebugPanel(PanelManagerInterface* manager);
        ~DebugPanel() override = default;
        
        // 基础接口实现
        void Render() override;
        void Update() override;
        const char* GetPanelName() const override { return "Debug"; }
        
        // 调试功能
        void PrintNodeHierarchy();
        void RefreshTTSNode();
        void CopyValuesToClipboard();
        
    private:
        PanelManagerInterface* m_Manager;
        
        // 调试状态
        bool m_VerboseLogging = false;
        bool m_ShowAdvancedInfo = false;
        bool m_AutoRefresh = true;
        float m_RefreshInterval = 1.0f;
        float m_LastRefreshTime = 0.0f;
        
        // 缓存的调试信息
        struct DebugInfo
        {
            // 相机信息
            RE::NiPoint3 cameraLocalPos{};
            RE::NiPoint3 cameraWorldPos{};
            RE::NiPoint3 cameraLocalRot{};
            RE::NiPoint3 cameraWorldRot{};
            float currentFOV = 0.0f;
            
            // TTSNode信息
            RE::NiPoint3 ttsLocalPos{};
            RE::NiPoint3 ttsWorldPos{};
            RE::NiPoint3 ttsLocalRot{};
            RE::NiPoint3 ttsWorldRot{};
            float ttsLocalScale = 0.0f;
            bool ttsNodeExists = false;
            
            // 渲染状态
            bool renderingEnabled = false;
            bool isForwardStage = false;
            bool isRenderingForScope = false;
            
            // 性能信息
            float frameTime = 0.0f;
            int frameCount = 0;
        } m_DebugInfo;
        
        // 渲染函数
        void RenderCameraInformation();
        void RenderTTSNodeInformation();
        void RenderRenderingStatus();
        void RenderPerformanceInfo();
        void RenderActionButtons();
        void RenderAdvancedDebugInfo();
        
        // 辅助函数
        void UpdateDebugInfo();
        void FormatVector3(const RE::NiPoint3& vec, char* buffer, size_t bufferSize, const char* format = "[%.3f, %.3f, %.3f]");
        void FormatRotationDegrees(const RE::NiPoint3& rot, char* buffer, size_t bufferSize);
        std::string GetRenderingStatusText();
        std::string GetNodeStatusText();
    };
}
