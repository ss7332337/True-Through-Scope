#pragma once

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "ScopeCamera.h"
#include "D3DHooks.h"
#include "Constants.h"
#include "RenderUtilities.h"

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

		// Helper functions for real-time adjustments
		void ApplyPositionAdjustment();
		void ApplyRotationAdjustment();
		void ApplyScaleAdjustment();
		void ResetAllAdjustments();
		RE::NiAVObject* GetTTSNode();
        
        // State variables
        bool m_Initialized = false;
        bool m_MenuOpen = false;
        
		// Camera adjustment settings
		float m_DeltaPosX = 0.0f;
		float m_DeltaPosY = 0.0f;
		float m_DeltaPosZ = 0.0f;
		float m_DeltaRot[3] = { 0.0f, 0.0f, 0.0f };    // Pitch, Yaw, Roll in degrees
		float m_DeltaScale = 1.0f;                   // X, Y, Z scale factors
		float m_AdjustmentSpeed = DEFAULT_ADJUSTMENT_SPEED;

		// Previous values for change detection
		float m_PrevDeltaPosX = 0.0f;
		float m_PrevDeltaPosY = 0.0f;
		float m_PrevDeltaPosZ = 0.0f;
		float m_PrevDeltaRot[3] = { 0.0f, 0.0f, 0.0f };
		float m_PrevDeltaScale = 1.0f;

        bool m_EnableRendering = false;
		bool m_RealTimeAdjustment = true;
		bool m_AutoApply = true;
        
        // Debug variables
        char m_DebugText[1024] = "";
        
        // UI style settings
		ImVec4 m_AccentColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
		ImVec4 m_WarningColor = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
		ImVec4 m_SuccessColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    };
}
