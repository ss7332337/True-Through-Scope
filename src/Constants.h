#pragma once
namespace ThroughScope
{
	// Default values
	constexpr float DEFAULT_FOV = 20.0f;
	constexpr float DEFAULT_ADJUSTMENT_SPEED = 0.1f;

	// Adjustment targets
	enum class AdjustmentTarget
	{
		POSITION = 0,
		ROTATION = 1,
		SCALE = 2
	};

	// Render targets for scope rendering
	constexpr int SCOPE_RENDER_TARGET_MAIN = 100;
	constexpr int SCOPE_RENDER_TARGET_TEMP = 101;

	// Texture dimensions for scope rendering
	constexpr unsigned int SCOPE_TEXTURE_WIDTH = 1024;
	constexpr unsigned int SCOPE_TEXTURE_HEIGHT = 1024;

	// Performance settings
	constexpr bool ENABLE_REAL_TIME_ADJUSTMENT = true;
	constexpr bool ENABLE_DEBUG_LOGGING = true;

	// Scale limits
	constexpr float MIN_SCALE_VALUE = 0.01f;
	constexpr float MAX_SCALE_VALUE = 10.0f;
	constexpr float DEFAULT_SCALE_VALUE = 1.0f;

	// Position limits
	constexpr float MIN_POSITION_VALUE = -100.0f;
	constexpr float MAX_POSITION_VALUE = 100.0f;

	// Rotation limits (in degrees)
	constexpr float MIN_ROTATION_VALUE = -180.0f;
	constexpr float MAX_ROTATION_VALUE = 180.0f;

	// FOV limits
	constexpr float MIN_FOV_VALUE = 5.0f;
	constexpr float MAX_FOV_VALUE = 100.0f;

	// Fine adjustment increments
	constexpr float FINE_POSITION_INCREMENT = 0.1f;
	constexpr float FINE_ROTATION_INCREMENT = 1.0f;  // degrees
	constexpr float FINE_SCALE_INCREMENT = 0.1f;
	constexpr float FINE_FOV_INCREMENT = 1.0f;
}
