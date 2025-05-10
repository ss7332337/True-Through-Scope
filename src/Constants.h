#pragma once
#include <Windows.h>
namespace ThroughScope
{
    constexpr float DEFAULT_ADJUSTMENT_SPEED = 1.0f;
    constexpr float DEFAULT_FOV = 90.0f;
    
    // Hotkeys
    constexpr int TOGGLE_ADJUSTMENT_KEY = VK_NUMPAD0;
    constexpr int TOGGLE_ADJUSTMENT_TARGET_KEY = VK_DIVIDE;
    constexpr int CYCLE_ADJUSTMENT_AXIS_KEY = VK_MULTIPLY;
    constexpr int DECREASE_ADJUSTMENT_SPEED_KEY = VK_OEM_MINUS;
    constexpr int INCREASE_ADJUSTMENT_SPEED_KEY = VK_OEM_PLUS;
    constexpr int INCREASE_FOV_KEY = VK_NUMPAD9;
    constexpr int DECREASE_FOV_KEY = VK_NUMPAD3;
    constexpr int PRINT_VALUES_KEY = VK_F8;
    constexpr int RESET_CAMERA_KEY = VK_F9;

    // Camera adjustment targets
    enum class AdjustmentTarget
    {
        POSITION,
        ROTATION
    };

    // Adjustment axes
    enum class AdjustmentAxis
    {
        X = 0,
        Y = 1,
        Z = 2
    };
}
