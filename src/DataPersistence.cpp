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
					if (objectInstanceExtra->values == NULL)
						continue;
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

			// Helper function to get value or default
			auto getValue = [](const nlohmann::json& json, const char* key, auto defaultValue) {
				return json.value(key, defaultValue);
			};

			// Load weapon config with validation
			if (configJson.contains("weapon")) {
				const auto& weaponJson = configJson["weapon"];
				std::string formIDStr = getValue(weaponJson, "localFormID", "00000000");
				config.weaponConfig.localFormID = WeaponConfig::ParseFormID(formIDStr);
				config.weaponConfig.modFileName = getValue(weaponJson, "modFileName", "");
			} else {
				logger::warn("Config file missing weapon section: {}", filePath);
				config.weaponConfig.localFormID = 0;
				config.weaponConfig.modFileName = "";
			}

			// Load camera adjustments with defaults
			const auto& cameraJson = configJson.value("camera", nlohmann::json::object());
			config.cameraAdjustments.deltaPosX = getValue(cameraJson, "deltaPosX", 0.0f);
			config.cameraAdjustments.deltaPosY = getValue(cameraJson, "deltaPosY", 0.0f);
			config.cameraAdjustments.deltaPosZ = getValue(cameraJson, "deltaPosZ", 5.0f);

			// Handle rotation array
			if (cameraJson.contains("deltaRot") && cameraJson["deltaRot"].is_array() && cameraJson["deltaRot"].size() == 3) {
				config.cameraAdjustments.deltaRot[0] = cameraJson["deltaRot"][0];
				config.cameraAdjustments.deltaRot[1] = cameraJson["deltaRot"][1];
				config.cameraAdjustments.deltaRot[2] = cameraJson["deltaRot"][2];
			} else {
				config.cameraAdjustments.deltaRot[0] = 0.0f;
				config.cameraAdjustments.deltaRot[1] = 0.0f;
				config.cameraAdjustments.deltaRot[2] = 0.0f;
			}

			config.cameraAdjustments.deltaScale = getValue(cameraJson, "deltaScale", 1.0f);

			// Load parallax settings with defaults
			const auto& parallaxJson = configJson.value("parallax", nlohmann::json::object());
			config.parallaxSettings.parallaxStrength = getValue(parallaxJson, "parallaxStrength", 0.05f);
			config.parallaxSettings.parallaxSmoothing = getValue(parallaxJson, "parallaxSmoothing", 0.5f);
			config.parallaxSettings.exitPupilRadius = getValue(parallaxJson, "exitPupilRadius", 0.45f);
			config.parallaxSettings.exitPupilSoftness = getValue(parallaxJson, "exitPupilSoftness", 0.15f);
			config.parallaxSettings.vignetteStrength = getValue(parallaxJson, "vignetteStrength", 0.3f);
			config.parallaxSettings.vignetteRadius = getValue(parallaxJson, "vignetteRadius", 0.7f);
			config.parallaxSettings.vignetteSoftness = getValue(parallaxJson, "vignetteSoftness", 0.3f);
			config.parallaxSettings.eyeReliefDistance = getValue(parallaxJson, "eyeReliefDistance", 0.5f);
			config.parallaxSettings.enableParallax = getValue(parallaxJson, "enableParallax", true);

			// 高级视差参数
			config.parallaxSettings.parallaxFogRadius = getValue(parallaxJson, "parallaxFogRadius", 1.0f);
			config.parallaxSettings.parallaxMaxTravel = getValue(parallaxJson, "parallaxMaxTravel", 1.5f);
			config.parallaxSettings.reticleParallaxStrength = getValue(parallaxJson, "reticleParallaxStrength", 0.5f);

			// Load scope settings with defaults
			const auto& scopeJson = configJson.value("scopeSettings", nlohmann::json::object());
			config.scopeSettings.minMagnification = getValue(scopeJson, "minMagnification", 1.0f);
			config.scopeSettings.maxMagnification = getValue(scopeJson, "maxMagnification", 6.0f);
			config.scopeSettings.nightVision = getValue(scopeJson, "nightVision", false);


			// Load reticle settings with defaults
			const auto& reticleJson = configJson.value("reticle", nlohmann::json::object());
			config.reticleSettings.customReticlePath = getValue(reticleJson, "customPath", "");
			config.reticleSettings.offsetX = getValue(reticleJson, "offsetX", 0.0f);
			config.reticleSettings.offsetY = getValue(reticleJson, "offsetY", 0.0f);
			config.reticleSettings.scale = getValue(reticleJson, "scale", 1.0f);
			config.reticleSettings.scaleReticleWithZoom = getValue(reticleJson, "scaleWithZoom", false);

			// Load zoom data settings with defaults
			const auto& zoomDataJson = configJson.value("zoomData", nlohmann::json::object());
			config.zoomDataSettings.fovMult = getValue(zoomDataJson, "fovMult", 1.0f);
			config.zoomDataSettings.offsetX = getValue(zoomDataJson, "offsetX", 0.0f);
			config.zoomDataSettings.offsetY = getValue(zoomDataJson, "offsetY", 0.0f);
			config.zoomDataSettings.offsetZ = getValue(zoomDataJson, "offsetZ", 0.0f);

			// Model name
			config.modelName = getValue(configJson, "modelName", "");

			// 加载夜视效果参数
			config.scopeSettings.nightVisionIntensity = getValue(scopeJson, "nightVisionIntensity", 1.0f);
			config.scopeSettings.nightVisionNoiseScale = getValue(scopeJson, "nightVisionNoiseScale", 0.05f);
			config.scopeSettings.nightVisionNoiseAmount = getValue(scopeJson, "nightVisionNoiseAmount", 0.05f);
			config.scopeSettings.nightVisionGreenTint = getValue(scopeJson, "nightVisionGreenTint", 1.2f);



			// 加载球形畸变效果参数
			config.scopeSettings.enableSphericalDistortion = getValue(scopeJson, "enableSphericalDistortion", false);
			config.scopeSettings.enableChromaticAberration = getValue(scopeJson, "enableChromaticAberration", false);
			config.scopeSettings.sphericalDistortionStrength = getValue(scopeJson, "sphericalDistortionStrength", 0.0f);
			config.scopeSettings.sphericalDistortionRadius = getValue(scopeJson, "sphericalDistortionRadius", 0.8f);
			config.scopeSettings.sphericalDistortionCenterX = getValue(scopeJson, "sphericalDistortionCenterX", 0.0f);
			config.scopeSettings.sphericalDistortionCenterY = getValue(scopeJson, "sphericalDistortionCenterY", 0.0f);

			// Add to multimap
			m_Configurations.emplace(config.weaponConfig.GetKey(), config);

			if (configJson.size() < 7) {  // Arbitrary threshold for detecting incomplete configs
				logger::info("Fixed incomplete config file: {}", filePath);
				SaveConfig(config);
			}

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
				{ "parallaxStrength", config.parallaxSettings.parallaxStrength },
				{ "parallaxSmoothing", config.parallaxSettings.parallaxSmoothing },
				{ "exitPupilRadius", config.parallaxSettings.exitPupilRadius },
				{ "exitPupilSoftness", config.parallaxSettings.exitPupilSoftness },
				{ "vignetteStrength", config.parallaxSettings.vignetteStrength },
				{ "vignetteRadius", config.parallaxSettings.vignetteRadius },
				{ "vignetteSoftness", config.parallaxSettings.vignetteSoftness },
				{ "eyeReliefDistance", config.parallaxSettings.eyeReliefDistance },
				{ "enableParallax", config.parallaxSettings.enableParallax },
				// 高级视差参数
				{ "parallaxFogRadius", config.parallaxSettings.parallaxFogRadius },
				{ "parallaxMaxTravel", config.parallaxSettings.parallaxMaxTravel },
				{ "reticleParallaxStrength", config.parallaxSettings.reticleParallaxStrength }
			};

			// Scope settings
			configJson["scopeSettings"] = {
				{ "minMagnification", config.scopeSettings.minMagnification },
				{ "maxMagnification", config.scopeSettings.maxMagnification },
				{ "nightVision", config.scopeSettings.nightVision },
				// 保存夜视效果参数
				{ "nightVisionIntensity", config.scopeSettings.nightVisionIntensity },
				{ "nightVisionNoiseScale", config.scopeSettings.nightVisionNoiseScale },
				{ "nightVisionNoiseAmount", config.scopeSettings.nightVisionNoiseAmount },
				{ "nightVisionGreenTint", config.scopeSettings.nightVisionGreenTint },

				// 保存球形畸变效果参数
				{ "enableSphericalDistortion", config.scopeSettings.enableSphericalDistortion },
				{ "enableChromaticAberration", config.scopeSettings.enableChromaticAberration },
				{ "sphericalDistortionStrength", config.scopeSettings.sphericalDistortionStrength },
				{ "sphericalDistortionRadius", config.scopeSettings.sphericalDistortionRadius },
				{ "sphericalDistortionCenterX", config.scopeSettings.sphericalDistortionCenterX },
				{ "sphericalDistortionCenterY", config.scopeSettings.sphericalDistortionCenterY }
			};

			// Reticle settings
			configJson["reticle"] = {
				{ "customPath", config.reticleSettings.customReticlePath },
				{ "offsetX", config.reticleSettings.offsetX },
				{ "offsetY", config.reticleSettings.offsetY },
				{ "scale", config.reticleSettings.scale },
				{ "scaleWithZoom", config.reticleSettings.scaleReticleWithZoom }
			};


			configJson["zoomData"] = {
				{ "fovMult", config.zoomDataSettings.fovMult },
				{ "offsetX", config.zoomDataSettings.offsetX },
				{ "offsetY", config.zoomDataSettings.offsetY },
				{ "offsetZ", config.zoomDataSettings.offsetZ }
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
			0.0f, 0.0f, 5.0f,      // Position
			{ 0.0f, 0.0f, 0.0f },  // Rotation
			1.0f                   // Scale
		};

		// Set default parallax settings
		presetConfig.parallaxSettings = {
			0.05f,   // parallaxStrength
			0.5f,    // parallaxSmoothing
			0.45f,   // exitPupilRadius
			0.15f,   // exitPupilSoftness
			0.3f,    // vignetteStrength
			0.7f,    // vignetteRadius
			0.3f,    // vignetteSoftness
			0.5f,    // eyeReliefDistance
			true,    // enableParallax
			// 高级视差参数
			1.0f,    // parallaxFogRadius
			1.5f,    // parallaxMaxTravel
			0.5f     // reticleParallaxStrength
		};

		// Set default scope settings
		presetConfig.scopeSettings.minMagnification = 1.0f;
		presetConfig.scopeSettings.maxMagnification = 6.0f;
		presetConfig.scopeSettings.nightVision = false;

		// 设置夜视效果默认参数
		presetConfig.scopeSettings.nightVisionIntensity = 1.0f;
		presetConfig.scopeSettings.nightVisionNoiseScale = 0.05f;
		presetConfig.scopeSettings.nightVisionNoiseAmount = 0.05f;
		presetConfig.scopeSettings.nightVisionGreenTint = 1.2f;



		// 设置球形畸变效果默认参数
		presetConfig.scopeSettings.enableSphericalDistortion = false;
		presetConfig.scopeSettings.enableChromaticAberration = false;
		presetConfig.scopeSettings.sphericalDistortionStrength = 0.0f;
		presetConfig.scopeSettings.sphericalDistortionRadius = 0.8f;
		presetConfig.scopeSettings.sphericalDistortionCenterX = 0.0f;
		presetConfig.scopeSettings.sphericalDistortionCenterY = 0.0f;

		// Default reticle settings
		presetConfig.reticleSettings.customReticlePath = "";
		presetConfig.reticleSettings.offsetX = 0.0f;
		presetConfig.reticleSettings.offsetY = 0.0f;
		presetConfig.reticleSettings.scale = 1.0f;
		presetConfig.reticleSettings.scaleReticleWithZoom = false;

		presetConfig.zoomDataSettings.fovMult = 1.0f;
		presetConfig.zoomDataSettings.offsetX = 0.0f;
		presetConfig.zoomDataSettings.offsetY = 0.0f;
		presetConfig.zoomDataSettings.offsetZ = 0.0f;

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

				{ "selectedLanguage", m_GlobalSettings.selectedLanguage }
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

				m_GlobalSettings.menuKeyBindings = { 113, 0, 0 };          // F2
				m_GlobalSettings.nightVisionKeyBindings = { 88, 16, 0 };    // LShift + X

				m_GlobalSettings.selectedLanguage = 0;  // Default to English

				return SaveGlobalConfig();
			}

			nlohmann::json globalJson;
			configFile >> globalJson;

			// Load key bindings with fallback to defaults if not found
			if (globalJson.contains("menuKeyBindings")) {
				m_GlobalSettings.menuKeyBindings = globalJson["menuKeyBindings"].get<std::array<int, 3>>();
			} else {
				m_GlobalSettings.menuKeyBindings = { 113, 0, 0 }; 
			}

			if (globalJson.contains("nightVisionKeyBindings")) {
				m_GlobalSettings.nightVisionKeyBindings = globalJson["nightVisionKeyBindings"].get<std::array<int, 3>>();
			} else {
				m_GlobalSettings.nightVisionKeyBindings = { 88, 16, 0 };
			}



			// 加载语言设置
			if (globalJson.contains("selectedLanguage")) {
				m_GlobalSettings.selectedLanguage = globalJson["selectedLanguage"].get<int>();
			} else {
				m_GlobalSettings.selectedLanguage = 0;  // Default to English
			}

			return true;
		} catch (const std::exception& e) {
			logger::error("Failed to load global config: {}. Creating default configuration.", e.what());

			// Set default key bindings
			m_GlobalSettings.menuKeyBindings = { 113, 0, 0 };
			m_GlobalSettings.nightVisionKeyBindings = { 88, 16, 0 };

			m_GlobalSettings.selectedLanguage = 0;  // Default to English

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
