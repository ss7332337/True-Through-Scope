#pragma once

#include "ThermalVision.h"
#include "../ImGuiManager.h"

namespace ThroughScope
{
    // 热成像控制接口 - 管理热成像系统的用户界面和控制
    class ThermalVisionControl
    {
    public:
        static ThermalVisionControl* GetSingleton();

        void Initialize();
        void Update(float deltaTime);
        void RenderUI(); // ImGui界面

        // 快捷键控制
        void ToggleThermalVision();
        void CyclePalette();
        void IncreaseSensitivity();
        void DecreaseSensitivity();

        // 热成像控制
        bool IsEnabled() const { return m_enabled; }
        void SetEnabled(bool enabled);

        // 获取热成像系统
        ThermalVision* GetThermalVision() { return m_thermalVision; }

    private:
        ThermalVisionControl() = default;
        ~ThermalVisionControl() = default;

        void UpdateHotkeys();
        void DrawThermalOverlay();

        // 热成像系统
        ThermalVision* m_thermalVision = nullptr;

        // 控制状态
        bool m_enabled = false;
        bool m_showUI = false;
        bool m_autoGain = true;

        // 热键状态
        bool m_keyPressed = false;
        float m_keyHoldTime = 0.0f;

        // 单例
        static ThermalVisionControl* s_instance;
    };
}
