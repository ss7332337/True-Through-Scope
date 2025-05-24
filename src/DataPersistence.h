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
			float deltaPosZ = 0.0f;
			float deltaRot[3] = { 0.0f, 0.0f, 0.0f };  // Pitch, Yaw, Roll
			float deltaScale = 1.0f;
		};

		struct ParallaxSettings
		{
			float relativeFogRadius = 0.0f;
			float scopeSwayAmount = 0.0f;
			float maxTravel = 0.0f;
			float radius = 0.0f;
		};

		struct ScopeSettings
		{
			int minFOV = 5;
			int maxFOV = 90;
			bool nightVision = false;
			bool thermalVision = false;
		};

		struct ScopeConfig
		{
			WeaponConfig weaponConfig;
			CameraAdjustments cameraAdjustments;
			ParallaxSettings parallaxSettings;
			ScopeSettings scopeSettings;
			int reticleIndex = -1;
			std::string customReticlePath;
		};

		struct GlobalSettings
		{
			static constexpr std::array<int, 3> DEFAULT_MENU_KEYS = { 123, 0, 0 };          // F12
			static constexpr std::array<int, 3> DEFAULT_NIGHTVISION_KEYS = { 78, 0, 0 };    // N
			static constexpr std::array<int, 3> DEFAULT_THERMALVISION_KEYS = { 84, 0, 0 };  // T

			std::array<int, 3> menuKeyBindings = DEFAULT_MENU_KEYS;
			std::array<int, 3> nightVisionKeyBindings = DEFAULT_NIGHTVISION_KEYS;
			std::array<int, 3> thermalVisionKeyBindings = DEFAULT_THERMALVISION_KEYS;
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
