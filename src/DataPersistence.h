#pragma once

#include "Utilities.h"
#include <array>  // For key bindings
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ThroughScope
{
	class DataPersistence
	{
	public:
		struct WeaponConfig
		{
			uint32_t localFormID;
			std::string modFileName;

			// Generate key for multimap
			std::string GetKey() const
			{
				return fmt::format("{:08X}:{}", localFormID, modFileName);  // Format as 8-digit hex
			}

			// Helper to parse hex string to uint32
			static uint32_t ParseFormID(const std::string& hexStr)
			{
				return static_cast<uint32_t>(std::stoul(hexStr, nullptr, 16));
			}
		};

		
		struct CameraAdjustments
		{
			float deltaPosX = 0.0f;
			float deltaPosY = 0.0f;
			float deltaPosZ = 7.0f;
			float deltaRot[3] = { 0.0f, 0.0f, 0.0f };  // Pitch, Yaw, Roll
			float deltaScale = 1.25f;
		};

		struct ParallaxSettings
		{
			float parallaxStrength = 0.05f;        // 视差偏移强度
			float parallaxSmoothing = 0.5f;        // 时域平滑
			float exitPupilRadius = 0.45f;         // 出瞳半径
			float exitPupilSoftness = 0.15f;       // 出瞳边缘柔和度
			float vignetteStrength = 0.3f;         // 晕影强度
			float vignetteRadius = 0.7f;           // 晕影起始半径
			float vignetteSoftness = 0.3f;         // 晕影柔和度
			float eyeReliefDistance = 0.5f;        // 眼距
			bool  enableParallax = true;           // 启用视差

			// 高级视差参数
			float parallaxFogRadius = 1.0f;            // 边缘渐变半径
			float parallaxMaxTravel = 1.5f;            // 最大移动距离
			float reticleParallaxStrength = 0.5f;      // 准星偏移强度
		};


		struct ScopeSettings
		{
			float minMagnification = 1.0f;   // 最小倍率 (1x = 无放大)
			float maxMagnification = 6.0f;   // 最大倍率 (6x = 6倍放大)
			bool nightVision = false;

			// 夜视效果参数
			float nightVisionIntensity = 1.0f;
			float nightVisionNoiseScale = 0.05f;
			float nightVisionNoiseAmount = 0.05f;
			float nightVisionGreenTint = 1.2f;



			// 球形畸变效果参数
			bool enableSphericalDistortion = false;
			bool enableChromaticAberration = false;
			float sphericalDistortionStrength = 0.0f;   // 畸变强度 (-0.5 到 0.5)
			float sphericalDistortionRadius = 0.8f;     // 畸变半径 (0.1 到 1.0)
			float sphericalDistortionCenterX = 0.0f;    // X轴中心偏移 (-0.5 到 0.5)
			float sphericalDistortionCenterY = 0.0f;    // Y轴中心偏移 (-0.5 到 0.5)
		};

		struct ReticleSettings
		{
			std::string customReticlePath;
			float scale = 1.0f;    // 瞄准镜缩放 (0.1 - 32.0)
			float offsetX = 0.5f;
			float offsetY = 0.5f;
			bool scaleReticleWithZoom = false;  // 准星随瞄具放大
		};

		struct ZoomDataSettings
		{
			float fovMult = 1.0f; 
			float offsetX = 0.0f;
			float offsetY = 0.0f;
			float offsetZ = 0.0f;
		};

		
		struct ScopeConfig
		{
			WeaponConfig weaponConfig;
			CameraAdjustments cameraAdjustments;
			ParallaxSettings parallaxSettings;
			ScopeSettings scopeSettings;
			ReticleSettings reticleSettings;
			ZoomDataSettings zoomDataSettings;

			std::string modelName;
			std::string nifFileName;
		};


		struct GlobalSettings
		{
			static constexpr std::array<int, 3> DEFAULT_MENU_KEYS = { 113, 0, 0 };          // 2
			static constexpr std::array<int, 3> DEFAULT_NIGHTVISION_KEYS = { 78, 0, 0 };    // N


			std::array<int, 3> menuKeyBindings = DEFAULT_MENU_KEYS;
			std::array<int, 3> nightVisionKeyBindings = DEFAULT_NIGHTVISION_KEYS;

			
			// 语言设置
			int selectedLanguage = 0;  // 0 = English (Language::English)
		};

		struct WeaponInfo
		{
			RE::TESObjectWEAP* weapon = nullptr;
			RE::TESObjectWEAP::InstanceData* instanceData = nullptr;
			std::string weaponModName;
			uint32_t weaponFormID = 0;
			std::vector<RE::TESForm*> availableMods;
			RE::TESForm* selectedModForm = nullptr;
			std::string configSource;
			const ScopeConfig* currentConfig = nullptr;
		};


		static DataPersistence* GetSingleton();

		// Load/Save operations
		bool LoadAllConfigs(const std::string& directoryPath = "");
		bool SaveConfig(const ScopeConfig& config);

		// Config management
		bool GeneratePresetConfig(uint32_t localFormID, const std::string& modFileName);
		bool GeneratePresetConfig(uint32_t localFormID, const std::string& modFileName, const std::string& nifFileName);
		const ScopeConfig* GetConfig(const std::string& key) const;
		std::vector<const ScopeConfig*> GetAllConfigs() const;
		bool RemoveConfig(const std::string& key);

		// Global settings (stored in a separate file)
		void SetGlobalSettings(const GlobalSettings& settings);
		const GlobalSettings& GetGlobalSettings() const;
		const ScopeConfig* GetConfigByFormIDAndMod(uint32_t formID, const std::string& modName) const;

		static WeaponInfo GetCurrentWeaponInfo();
	private:
		DataPersistence() = default;
		~DataPersistence() = default;

		mutable std::mutex m_DataMutex;
		std::string m_ConfigDirectory = "Data/F4SE/Plugins/TrueThroughScope/WeaponConfigs/";
		std::string m_GlobalConfigPath = "Data/F4SE/Plugins/TrueThroughScope/global_config.json";

		// Multimap to store all configurations
		std::unordered_multimap<std::string, ScopeConfig> m_Configurations;

		// Global settings
		GlobalSettings m_GlobalSettings;

		// Helper methods
		bool LoadConfig(const std::string& filePath);
		bool SaveGlobalConfig();
		bool LoadGlobalConfig();
		std::string GetConfigFilePath(uint32_t localFormID, const std::string& modFileName) const;
		uint32_t ParseFormIDFromKey(const std::string& key) const;
	};
}
