#include "DataPersistence.h"
#include <Utilities.h>
#include <filesystem>
#include <fstream>

namespace ThroughScope
{

	DataPersistence* DataPersistence::GetSingleton()
	{
		static DataPersistence instance;

		// Load configurations on first access
		static std::once_flag initFlag;
		std::call_once(initFlag, [&]() {
			instance.LoadAllConfigs(instance.m_ConfigDirectory);
			instance.LoadGlobalConfig();
		});

		return &instance;
	}

	ThroughScope::DataPersistence::WeaponInfo DataPersistence::GetCurrentWeaponInfo()
	{
		using namespace RE;
		WeaponInfo weaponInfo;
		auto dataPersistence = DataPersistence::GetSingleton();

		// Get current equipped weapon
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->currentProcess) {
			return weaponInfo;  // Return empty struct
		}

		auto& eventEquipped = player->currentProcess->middleHigh->equippedItems;

		// Check if we have a gun equipped
		if (!(eventEquipped.size() > 0 &&
				eventEquipped[0].item.instanceData &&
				((TESObjectWEAP::InstanceData*)eventEquipped[0].item.instanceData.get())->type == WEAPON_TYPE::kGun)) {
			return weaponInfo;  // Return empty struct
		}

		weaponInfo.weapon = ((TESObjectWEAP*)eventEquipped[0].item.object);
		weaponInfo.instanceData = (TESObjectWEAP::InstanceData*)eventEquipped[0].item.instanceData.get();

		if (!weaponInfo.weapon || !weaponInfo.instanceData) {
			return weaponInfo;  // Return empty struct
		}

		// Get weapon form info
		auto weaponForm = TESForm::GetFormByID(weaponInfo.weapon->formID);
		auto weaponFile = weaponForm ? weaponForm->GetFile() : nullptr;
		weaponInfo.weaponModName = weaponFile ? weaponFile->filename : "";
		weaponInfo.weaponFormID = weaponForm ? weaponForm->GetLocalFormID() : 0;

		// Find available modifications and check for configs
		auto invItems = player->inventoryList;
		for (size_t i = 0; i < invItems->data.size(); i++) {
			auto& item = invItems->data[i];
			if (!item.object || item.object != weaponForm) {
				continue;
			}

			if (item.stackData && item.stackData->extra) {
				auto extraDataList = item.stackData->extra.get();
				auto objectInstanceExtra = extraDataList->GetByType<RE::BGSObjectInstanceExtra>();

				if (objectInstanceExtra) {
					auto indexData = objectInstanceExtra->GetIndexData();
					if (!indexData.empty()) {
						// Collect available modifications
						for (auto& modData : indexData) {
							if (auto modForm = RE::TESForm::GetFormByID(modData.objectID)) {
								weaponInfo.availableMods.push_back(modForm);
							}
						}

						// Check for modification configs (iterate in reverse order for priority)
						for (auto it = indexData.rbegin(); it != indexData.rend(); ++it) {
							auto& modData = *it;
							auto modForm = RE::TESForm::GetFormByID(modData.objectID);
							if (!modForm)
								continue;

							auto modFile = modForm->GetFile();
							std::string modName = modFile ? modFile->filename : "";
							uint32_t modFormID = modForm->GetLocalFormID();

							// Try to find config by FormID and mod name (numeric comparison)
							if (auto config = dataPersistence->GetConfigByFormIDAndMod(modFormID, modName)) {
								weaponInfo.currentConfig = config;
								weaponInfo.selectedModForm = modForm;
								weaponInfo.configSource = "Modification";
								break;
							}
						}
					}
				}
			}
			break;
		}

		// If no modification config found, try weapon itself
		if (!weaponInfo.currentConfig) {
			if (auto config = dataPersistence->GetConfigByFormIDAndMod(weaponInfo.weaponFormID, weaponInfo.weaponModName)) {
				weaponInfo.currentConfig = config;
				weaponInfo.configSource = "Weapon";
			}
		}

		return weaponInfo;
	}

	bool DataPersistence::LoadAllConfigs(const std::string& directoryPath)
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);

		if (!directoryPath.empty()) {
			m_ConfigDirectory = directoryPath;
		}

		// Create directory if it doesn't exist
		std::filesystem::create_directories(m_ConfigDirectory);

		m_Configurations.clear();

		try {
			for (const auto& entry : std::filesystem::directory_iterator(m_ConfigDirectory)) {
				if (entry.is_regular_file() && entry.path().extension() == ".json") {
					LoadConfig(entry.path().string());
				}
			}
			logger::info("Loaded {} weapon configurations from {}", m_Configurations.size(), m_ConfigDirectory);
			return true;
		} catch (const std::exception& e) {
			logger::error("Failed to load configs from {}: {}", m_ConfigDirectory, e.what());
			return false;
		}
	}

	bool DataPersistence::LoadConfig(const std::string& filePath)
	{
		try {
			std::ifstream configFile(filePath);
			if (!configFile.is_open()) {
				logger::warn("Could not open config file: {}", filePath);
				return false;
			}

			nlohmann::json configJson;
			configFile >> configJson;

			ScopeConfig config;

			// Load weapon config
			std::string formIDStr = configJson["weapon"]["localFormID"];
			config.weaponConfig.localFormID = WeaponConfig::ParseFormID(formIDStr);
			config.weaponConfig.modFileName = configJson["weapon"]["modFileName"];

			// Load camera adjustments
			const auto& cameraJson = configJson["camera"];
			config.cameraAdjustments.deltaPosX = cameraJson["deltaPosX"];
			config.cameraAdjustments.deltaPosY = cameraJson["deltaPosY"];
			config.cameraAdjustments.deltaPosZ = cameraJson["deltaPosZ"];
			config.cameraAdjustments.deltaRot[0] = cameraJson["deltaRot"][0];
			config.cameraAdjustments.deltaRot[1] = cameraJson["deltaRot"][1];
			config.cameraAdjustments.deltaRot[2] = cameraJson["deltaRot"][2];
			config.cameraAdjustments.deltaScale = cameraJson["deltaScale"];

			// Load parallax settings
			const auto& parallaxJson = configJson["parallax"];
			config.parallaxSettings.relativeFogRadius = parallaxJson["relativeFogRadius"];
			config.parallaxSettings.scopeSwayAmount = parallaxJson["scopeSwayAmount"];
			config.parallaxSettings.maxTravel = parallaxJson["maxTravel"];
			config.parallaxSettings.radius = parallaxJson["radius"];

			// Load scope settings
			if (configJson.contains("scopeSettings")) {
				const auto& scopeJson = configJson["scopeSettings"];
				config.scopeSettings.minFOV = scopeJson["minFOV"];
				config.scopeSettings.maxFOV = scopeJson["maxFOV"];
				config.scopeSettings.nightVision = scopeJson["nightVision"];
				config.scopeSettings.thermalVision = scopeJson["thermalVision"];
			}

			// Load reticle settings
			const auto& reticleJson = configJson["reticle"];
			config.reticleIndex = reticleJson["index"];
			config.customReticlePath = reticleJson.value("customPath", "");

			config.modelName = configJson.value("modelName", "");

			// Add to multimap
			m_Configurations.emplace(config.weaponConfig.GetKey(), config);

			return true;
		} catch (const std::exception& e) {
			logger::error("Failed to parse config file {}: {}", filePath, e.what());
			return false;
		}
	}


	bool DataPersistence::SaveConfig(const ScopeConfig& config)
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);

		try {
			nlohmann::json configJson;

			// Weapon info
			configJson["weapon"] = {
				{ "localFormID", fmt::format("{:08X}", config.weaponConfig.localFormID) },
				{ "modFileName", config.weaponConfig.modFileName }
			};

			// Camera adjustments
			configJson["camera"] = {
				{ "deltaPosX", config.cameraAdjustments.deltaPosX },
				{ "deltaPosY", config.cameraAdjustments.deltaPosY },
				{ "deltaPosZ", config.cameraAdjustments.deltaPosZ },
				{ "deltaRot", { config.cameraAdjustments.deltaRot[0],
								  config.cameraAdjustments.deltaRot[1],
								  config.cameraAdjustments.deltaRot[2] } },
				{ "deltaScale", config.cameraAdjustments.deltaScale }
			};

			// Parallax settings
			configJson["parallax"] = {
				{ "relativeFogRadius", config.parallaxSettings.relativeFogRadius },
				{ "scopeSwayAmount", config.parallaxSettings.scopeSwayAmount },
				{ "maxTravel", config.parallaxSettings.maxTravel },
				{ "radius", config.parallaxSettings.radius }
			};

			// Scope settings
			configJson["scopeSettings"] = {
				{ "minFOV", config.scopeSettings.minFOV },
				{ "maxFOV", config.scopeSettings.maxFOV },
				{ "nightVision", config.scopeSettings.nightVision },
				{ "thermalVision", config.scopeSettings.thermalVision }
			};

			// Reticle settings
			configJson["reticle"] = {
				{ "index", config.reticleIndex },
				{ "customPath", config.customReticlePath }
			};

			configJson["modelName"] = config.modelName;

			// Create directory if it doesn't exist
			std::filesystem::create_directories(m_ConfigDirectory);

			// Save to file
			std::string filePath = GetConfigFilePath(config.weaponConfig.localFormID, config.weaponConfig.modFileName);
			std::ofstream configFile(filePath);
			if (!configFile.is_open()) {
				logger::error("Failed to open config file for writing: {}", filePath);
				return false;
			}

			configFile << configJson.dump(4);
			logger::info("Saved weapon configuration to {}", filePath);
			return true;
		} catch (const std::exception& e) {
			logger::error("Failed to save config: {}", e.what());
			return false;
		}
	}

	bool DataPersistence::GeneratePresetConfig(uint32_t localFormID, const std::string& modFileName, const std::string& nifFileName)
	{
		ScopeConfig presetConfig;
		presetConfig.weaponConfig.localFormID = localFormID;
		presetConfig.weaponConfig.modFileName = modFileName;
		presetConfig.modelName = nifFileName;

		// Set default camera adjustments
		presetConfig.cameraAdjustments = {
			0.0f, 0.0f, 0.0f,      // Position
			{ 0.0f, 0.0f, 0.0f },  // Rotation
			1.0f                   // Scale
		};

		// Set default parallax settings
		presetConfig.parallaxSettings = {
			0.5f,   // relativeFogRadius
			0.1f,   // scopeSwayAmount
			0.05f,  // maxTravel
			0.3f    // radius
		};

		// Set default scope settings
		presetConfig.scopeSettings = {
			5,      // minFOV
			90,     // maxFOV
			false,  // nightVision
			false   // thermalVision
		};

		// Default reticle settings
		presetConfig.reticleIndex = 0;
		presetConfig.customReticlePath = "";

		return SaveConfig(presetConfig);
	}

	bool DataPersistence::GeneratePresetConfig(uint32_t localFormID, const std::string& modFileName)
	{
		return GeneratePresetConfig(localFormID, modFileName, "");  // 默认空字符串
	}

	void DataPersistence::SetGlobalSettings(const GlobalSettings& settings)
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);
		m_GlobalSettings = settings;
		SaveGlobalConfig();
	}

	const DataPersistence::GlobalSettings& DataPersistence::GetGlobalSettings() const
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);
		return m_GlobalSettings;
	}

	bool DataPersistence::SaveGlobalConfig()
	{
		try {
			nlohmann::json globalJson = {
				{ "menuKeyBindings", m_GlobalSettings.menuKeyBindings },
				{ "nightVisionKeyBindings", m_GlobalSettings.nightVisionKeyBindings },
				{ "thermalVisionKeyBindings", m_GlobalSettings.thermalVisionKeyBindings }
			};

			// Create directory if it doesn't exist
			std::filesystem::path configPath(m_GlobalConfigPath);
			std::filesystem::create_directories(configPath.parent_path());

			std::ofstream configFile(m_GlobalConfigPath);
			if (!configFile.is_open()) {
				logger::error("Failed to open global config file for writing: {}", m_GlobalConfigPath);
				return false;
			}

			configFile << globalJson.dump(4);
			return true;
		} catch (const std::exception& e) {
			logger::error("Failed to save global config: {}", e.what());
			return false;
		}
	}

	bool DataPersistence::LoadGlobalConfig()
	{
		try {
			std::ifstream configFile(m_GlobalConfigPath);
			if (!configFile.is_open()) {
				logger::warn("Global config file not found, creating default configuration");

				// Set default key bindings:
				// - Menu: F12 (VK_F12 = 123)
				// - Night Vision: N (VK_N = 78)
				// - Thermal Vision: T (VK_T = 84)
				m_GlobalSettings.menuKeyBindings = { 123, 0, 0 };          // F12
				m_GlobalSettings.nightVisionKeyBindings = { 78, 0, 0 };    // N
				m_GlobalSettings.thermalVisionKeyBindings = { 84, 0, 0 };  // T

				return SaveGlobalConfig();
			}

			nlohmann::json globalJson;
			configFile >> globalJson;

			// Load key bindings with fallback to defaults if not found
			if (globalJson.contains("menuKeyBindings")) {
				m_GlobalSettings.menuKeyBindings = globalJson["menuKeyBindings"].get<std::array<int, 3>>();
			} else {
				m_GlobalSettings.menuKeyBindings = { 123, 0, 0 };  // Default to F12
			}

			if (globalJson.contains("nightVisionKeyBindings")) {
				m_GlobalSettings.nightVisionKeyBindings = globalJson["nightVisionKeyBindings"].get<std::array<int, 3>>();
			} else {
				m_GlobalSettings.nightVisionKeyBindings = { 78, 0, 0 };  // Default to N
			}

			if (globalJson.contains("thermalVisionKeyBindings")) {
				m_GlobalSettings.thermalVisionKeyBindings = globalJson["thermalVisionKeyBindings"].get<std::array<int, 3>>();
			} else {
				m_GlobalSettings.thermalVisionKeyBindings = { 84, 0, 0 };  // Default to T
			}

			return true;
		} catch (const std::exception& e) {
			logger::error("Failed to load global config: {}. Creating default configuration.", e.what());

			// Set default key bindings
			m_GlobalSettings.menuKeyBindings = { 123, 0, 0 };          // F12
			m_GlobalSettings.nightVisionKeyBindings = { 78, 0, 0 };    // N
			m_GlobalSettings.thermalVisionKeyBindings = { 84, 0, 0 };  // T

			return SaveGlobalConfig();
		}
	}

	const DataPersistence::ScopeConfig* DataPersistence::GetConfigByFormIDAndMod(uint32_t formID, const std::string& modName) const
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);

		// 遍历所有配置，比较 FormID 数值而不是字符串
		for (const auto& [key, config] : m_Configurations) {
			// 解析键中的FormID和ModName
			size_t colonPos = key.find(':');
			if (colonPos == std::string::npos) {
				continue;
			}

			std::string formIDStr = key.substr(0, colonPos);
			std::string keyModName = key.substr(colonPos + 1);

			// 比较ModName（必须完全匹配）
			if (keyModName != modName) {
				continue;
			}

			// 将字符串FormID转换为数值进行比较
			uint32_t keyFormID = ParseFormIDFromKey(formIDStr);
			if (keyFormID == formID) {
				return &config;
			}
		}

		return nullptr;
	}

	uint32_t DataPersistence::ParseFormIDFromKey(const std::string& formIDStr) const
	{
		try {
			// 支持十六进制字符串转换，如 "F9D" 或 "0000F9D"
			return std::stoul(formIDStr, nullptr, 16);
		} catch (const std::exception&) {
			return 0;  // 解析失败返回0
		}
	}

	const DataPersistence::ScopeConfig* DataPersistence::GetConfig(const std::string& key) const
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);

		auto range = m_Configurations.equal_range(key);
		if (range.first != range.second) {
			// Return the first matching config
			return &range.first->second;
		}

		return nullptr;
	}

	std::vector<const DataPersistence::ScopeConfig*> DataPersistence::GetAllConfigs() const
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);

		std::vector<const ScopeConfig*> configs;
		for (const auto& pair : m_Configurations) {
			configs.push_back(&pair.second);
		}

		return configs;
	}

	bool DataPersistence::RemoveConfig(const std::string& key)
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);

		bool removed = false;
		auto range = m_Configurations.equal_range(key);
		for (auto it = range.first; it != range.second;) {
			// Delete the file first
			std::string filePath = GetConfigFilePath(it->second.weaponConfig.localFormID,
				it->second.weaponConfig.modFileName);
			if (std::filesystem::remove(filePath)) {
				it = m_Configurations.erase(it);
				removed = true;
			} else {
				++it;
			}
		}

		return removed;
	}


	std::string DataPersistence::GetConfigFilePath(uint32_t localFormID, const std::string& modFileName) const
	{
		// Sanitize modFileName to remove invalid characters for filenames
		std::string safeModName = modFileName;
		std::replace_if(safeModName.begin(), safeModName.end(), [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');

		return fmt::format("{}{:08X}_{}.json", m_ConfigDirectory, localFormID, safeModName);
	}

}  // namespace ThroughScope
