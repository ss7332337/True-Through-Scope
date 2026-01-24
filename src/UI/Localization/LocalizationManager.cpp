#include "LocalizationManager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdarg>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace ThroughScope 
{
    // 静态语言信息
    const LocalizationManager::LanguageInfo LocalizationManager::s_LanguageInfo[] = {
        { "English", "en" },
        { "简体中文", "zh_SC" },
        { "繁體中文", "zh_TC" },
        { "日本語", "ja" },
        { "한국어", "ko" },
        { "Deutsch", "de" },
        { "Français", "fr" },
        { "Español", "es" },
        { "Русский", "ru" },
        { "Italiano", "it" },
        { "Português", "pt" },
        { "Polski", "pl" }
    };

    LocalizationManager* LocalizationManager::GetSingleton() 
    {
        static LocalizationManager instance;
        return &instance;
    }

    bool LocalizationManager::Initialize() 
    {
        if (m_Initialized) {
            logger::debug("LocalizationManager already initialized, skipping");
            return true;
        }

        logger::info("Initializing LocalizationManager...");

        // 创建语言文件目录（如果不存在）
        std::filesystem::path langDir = "Data/F4SE/Plugins/TrueThroughScope/Languages";
        
        try {
            if (!std::filesystem::exists(langDir)) {
                logger::info("Creating language directory: {}", langDir.string());
                std::filesystem::create_directories(langDir);
            }
        } catch (const std::exception& e) {
            logger::error("Failed to create language directory: {}", e.what());
            // 继续执行，因为目录可能已存在或稍后创建
        }

        // 加载所有语言文件
        // 注意：只有英语是必需的，其他语言文件是可选的
        bool englishLoaded = false;
        int loadedCount = 0;
        for (int i = 0; i < static_cast<int>(Language::COUNT); ++i) {
            Language lang = static_cast<Language>(i);
            if (LoadLanguageFile(lang)) {
                loadedCount++;
                if (lang == Language::English) {
                    englishLoaded = true;
                }
            } else {
                // 如果是英语加载失败，这是严重错误
                if (lang == Language::English) {
                    logger::error("Critical: Failed to load English language file!");
                    return false;
                }
                // 其他语言文件缺失只是调试信息，不是错误
            }
        }

        logger::info("LocalizationManager initialized: {} of {} language files loaded", loadedCount, static_cast<int>(Language::COUNT));
        m_Initialized = true;
        // 只要英语加载成功就返回true（其他语言是可选的）
        return englishLoaded;
    }

    void LocalizationManager::Shutdown() 
    {
        m_Translations.clear();
        m_Initialized = false;
    }

    void LocalizationManager::SetLanguage(Language language) 
    {
        if (language >= Language::COUNT) {
            return;
        }
        
        m_CurrentLanguage = language;
    }

    const char* LocalizationManager::GetText(const char* key) const 
    {
        if (!key || !m_Initialized) {
            return key ? key : "";
        }

        // 首先尝试当前语言
        auto langIt = m_Translations.find(m_CurrentLanguage);
        if (langIt != m_Translations.end()) {
            auto textIt = langIt->second.find(key);
            if (textIt != langIt->second.end()) {
                return textIt->second.c_str();
            }
        }

        // 如果当前语言没有，回退到英语
        if (m_CurrentLanguage != Language::English) {
            auto englishIt = m_Translations.find(Language::English);
            if (englishIt != m_Translations.end()) {
                auto textIt = englishIt->second.find(key);
                if (textIt != englishIt->second.end()) {
                    return textIt->second.c_str();
                }
            }
        }

        // 如果都没有找到，返回原始key
        return key;
    }

    const char* LocalizationManager::GetTextFormat(const char* key, ...) const 
    {
        const char* format = GetText(key);
        
        va_list args;
        va_start(args, key);
        vsnprintf(m_FormatBuffer, sizeof(m_FormatBuffer), format, args);
        va_end(args);
        
        return m_FormatBuffer;
    }

    const char* LocalizationManager::GetLanguageName(Language language) const 
    {
        int index = static_cast<int>(language);
        if (index >= 0 && index < static_cast<int>(Language::COUNT)) {
            return s_LanguageInfo[index].name;
        }
        return "Unknown";
    }

    const char* LocalizationManager::GetLanguageCode(Language language) const 
    {
        int index = static_cast<int>(language);
        if (index >= 0 && index < static_cast<int>(Language::COUNT)) {
            return s_LanguageInfo[index].code;
        }
        return "unknown";
    }

    bool LocalizationManager::ReloadLanguageFiles() 
    {
        m_Translations.clear();
        
        bool success = true;
        for (int i = 0; i < static_cast<int>(Language::COUNT); ++i) {
            Language lang = static_cast<Language>(i);
            if (!LoadLanguageFile(lang)) {
                success = false;
            }
        }
        
        return success;
    }

    bool LocalizationManager::LoadLanguageFile(Language language) 
    {
        const char* langCode = GetLanguageCode(language);
        std::string filePath = std::string("Data/F4SE/Plugins/TrueThroughScope/Languages/") + langCode + ".json";
        
        return LoadLanguageFromJSON(filePath, language);
    }

    bool LocalizationManager::LoadLanguageFromJSON(const std::string& filePath, Language language) 
    {
        try {
            // 如果是英语，先加载默认值
            bool isEnglish = (language == Language::English);
            
            if (isEnglish) {
                auto defaults = GetDefaultEnglishJSON();
                auto& translations = m_Translations[language];
                for (auto& [key, value] : defaults.items()) {
                    if (value.is_string()) {
                        translations[key] = value.get<std::string>();
                    }
                }
            }
            std::ifstream file(filePath);
            if (!file.is_open()) {
                // 如果文件不存在，为英语创建默认文件
                if (isEnglish) {
                    logger::info("Creating default English language file: {}", filePath);
                    CreateDefaultEnglishFile(filePath);
                    // 尝试重新打开
                    file.open(filePath);
                }
                
                if (!file.is_open()) {
                    // 如果是英语，返回true（因为我们已经加载了默认值）
                    if (isEnglish) {
                        logger::info("Using hardcoded English defaults (file creation may have failed)");
                        return true;
                    }
                    return false;
                }
            }

            nlohmann::json json;
            file >> json;
            file.close();

            // 解析JSON并存储翻译
            auto& translations = m_Translations[language];
            size_t keysLoaded = 0;
            for (auto& [key, value] : json.items()) {
                if (value.is_string()) {
                    translations[key] = value.get<std::string>();
                    keysLoaded++;
                }
            }
            logger::debug("Loaded {} keys from {}", keysLoaded, filePath);

            return true;
        }
        catch (const std::exception& e) {
            // 记录错误
            logger::error("Failed to load language file {}: {}", filePath.c_str(), e.what());
            
            // 如果是英语，即使加载失败也返回true，因为我们有硬编码的默认值
            if (language == Language::English) {
                logger::info("Using hardcoded English defaults due to file error");
                return true;
            }
            return false;
        }
    }

    void LocalizationManager::CreateDefaultEnglishFile(const std::string& filePath) 
    {
        nlohmann::json defaultTranslations = GetDefaultEnglishJSON();

        std::ofstream file(filePath);
        if (file.is_open()) {
            file << defaultTranslations.dump(4);
            file.close();
        }
    }

    nlohmann::json LocalizationManager::GetDefaultEnglishJSON()
    {
        return {
            {"ui.menu.settings", "Settings"},
            {"ui.menu.camera", "Camera Adjustment"},
            {"ui.menu.reticle", "Reticle"},
            {"ui.menu.debug", "Debug"},
            {"debug.culling_stats", "Culling Stats"},
            {"ui.menu.models", "Model Switcher"},
            {"ui.menu.zoom", "Zoom Data"},
            {"settings.tab.interface", "Interface"},
            {"settings.tab.performance", "Performance"},
            {"settings.tab.keybindings", "Key Bindings"},
            {"settings.tab.advanced", "Advanced"},
            {"settings.advanced", "Advanced Settings"},
            {"settings.ui.language", "Language / Language:"},
            {"settings.ui.refresh_rate", "UI Refresh Rate:"},
            {"settings.performance.max_fps", "Maximum FPS:"},
            {"settings.performance.memory", "Memory Usage:"},
            {"settings.performance.ui_memory", "UI Memory: ~%.1f MB"},
            {"settings.performance.current_fps", "Current FPS: %.1f"},
            {"settings.keys.description", "Select key bindings from the dropdown menus below."},
            {"settings.keys.menu_toggle", "Menu Toggle:"},
            {"settings.keys.night_vision", "Night Vision:"},
            {"settings.keys.thermal_vision", "Thermal Vision:"},
            {"settings.keys.modifier", "Modifier:"},
            {"settings.keys.key", "Key:"},
            {"settings.keys.tips", "Tips:"},
            {"settings.advanced.warning", "Warning: These settings are for advanced users only!"},
            {"settings.advanced.culling_margin", "Culling Safety Margin"},
            {"settings.advanced.culling_margin.desc", "Adjusts the frustum culling bound safety margin.\nPositive values ensure less culling (safer).\nNegative values cull more aggressively (riskier).\nDefault: 0.05"},
            {"settings.advanced.shadow_range", "Shadow Caster Range"},
            {"settings.advanced.shadow_range.desc", "Objects within this distance (game units) from the camera will NOT be culled.\nThis prevents shadow-casting objects from being incorrectly culled.\nIncrease if shadows are missing; decrease for better performance.\nDefault: 5500"},
            {"button.save", "Save"},
            {"button.reset", "Reset to Defaults"},
            {"button.cancel", "Cancel"},
            {"button.apply", "Apply"},
            {"button.create", "Create Configuration"},
            {"button.rescan", "Rescan for NIF Files"},
            {"button.clear", "Clear"},
            {"button.refresh", "Refresh"},
            {"status.unsaved_changes", "● Unsaved changes"},
            {"status.all_saved", "○ All saved"},
            {"status.night_vision_on", "Night Vision: ON"},
            {"status.night_vision_off", "Night Vision: OFF"},
            {"status.settings_saved", "Settings saved successfully!"},
            {"status.settings_failed", "Settings save failed!"},
            {"status.changes_cancelled", "Changes cancelled"},
            {"status.settings_applied", "Settings applied"},
            {"status.all_reset", "All settings reset to defaults"},
            {"status.config_created", "Configuration created successfully! Model: %s"},
            {"status.config_failed", "Configuration creation failed!"},
            {"status.tts_reloaded", "TTSNode reloaded from: %s"},
            {"status.tts_reload_failed", "TTSNode reload failed: %s"},
            {"status.nif_files_found", "Found %d NIF files"},
            {"status.nif_scan_error", "Error scanning NIF files: %s"},
            {"status.adjustments_reset", "All adjustments reset!"},
            {"status.WeaponLoaded", "✓ Weapon Loaded"},
            {"status.WeaponNotLoaded", "⚠ No Weapon"},
            {"status.TTSNodeReady", "✓ TTSNode Ready"},
            {"status.TTSNodeNotReady", "⚠ No TTSNode"},
            {"camera.config.target", "Configuration Target:"},
            {"camera.config.scope_shape", "Scope Shape (Model File):"},
            {"camera.config.no_config", "No configuration found for this weapon"},
            {"camera.config.create_desc", "Create a new configuration to start customizing your scope settings."},
            {"camera.config.no_nif_files", "No NIF files found in Meshes/TTS/ScopeShape/"},
            {"camera.config.base_weapon", "Base Weapon"},
            {"camera.config.modification", "Modification #{} - {}"},
            {"camera.config.select_model", "Select a model..."},
            {"error.reset_confirm", "Are you sure you want to reset all settings to default values?"},
            {"error.cannot_undo", "This action cannot be undone."},
            {"warning.advanced_users", "Warning: These settings are for advanced users only!"},
            {"tooltip.base_weapon", "Create configuration for the base weapon"},
            {"tooltip.modification", "Create configuration specific to this modification"},
            {"tooltip.fov_multiplier", "Multiplier applied to field of view when zooming"},
            {"tooltip.reticle_scale", "Adjust reticle size (0.1 = very small, 1 = very large)"},
            {"tooltip.horizontal_offset", "Reticle horizontal position (0.0 = left, 0.5 = center, 1.0 = right)"},
            {"tooltip.vertical_offset", "Reticle vertical position (0.0 = top, 0.5 = center, 1.0 = bottom)"},
            {"tooltip.reset_to_center", "Reset position to screen center"},
            {"tooltip.click_to_select_texture", "Click to select this texture\nFile: %s"},
            {"tooltip.click_to_switch", "Click to switch to this model\nFile: %s"},
            {"tooltip.reload_texture", "Reload current texture file"},
            {"tooltip.save_settings", "Save current settings to configuration file"},
            {"tooltip.reset_defaults", "Hold Ctrl and click to reset all settings to defaults"},
            {"tooltip.show_preview", "Show texture preview and information"},
            {"tooltip.auto_load_config", "Load model specified in current configuration"},
            {"tooltip.reload_current", "Reload current model from configuration"},
            {"tooltip.remove_current", "Remove currently loaded TTSNode"},
            {"tooltip.select_language", "Select your preferred language"},
            {"tooltip.key_none", "Select \"None\" to disable key binding"},
            {"tooltip.key_combination", "For combination keys, choose both modifier and primary key"},
            {"tooltip.key_immediate", "Changes take effect immediately"},
            {"tooltip.print_hierarchy", "Print weapon node hierarchy to log file"},
            {"tooltip.refresh_node", "Force refresh TTSNode information"},
            {"tooltip.copy_values", "Copy current debug values to clipboard"},
            {"tooltip.auto_refresh", "Automatically refresh debug information"},
            {"tooltip.show_advanced_debug", "Show additional debug information and performance data"},
            {"tooltip.refresh_interval", "Debug information refresh interval"},
            {"camera.position", "Position"},
            {"camera.rotation", "Rotation"},
            {"camera.scale", "Scale"},
            {"camera.parallax", "Parallax Settings"},
            {"camera.night_vision", "Night Vision"},
            {"camera.reset_all", "Reset All Adjustments"},
            {"camera.apply_settings", "Apply Settings"},
            {"camera.weapon_info", "Current Weapon Information"},
            {"camera.no_weapon", "No valid weapon equipped"},
            {"camera.equip_weapon", "Please equip a weapon with scope capabilities to configure settings."},
            {"camera.weapon_label", "Weapon: [%08X] %s"},
            {"camera.config_source", "Config Source: [%08X] %s (%s)"},
            {"camera.current_model", "Current Model: %s"},
            {"camera.reload_tts", "Reload TTSNode"},
            {"camera.reload_tooltip", "Click to recreate TTSNode from saved model file"},
            {"camera.position_controls", "Position Controls"},
            {"camera.rotation_controls", "Rotation Controls"},
            {"camera.scale_controls", "Scale Controls"},
            {"camera.precise_adjustment", "Precise Adjustment:"},
            {"camera.fine_tuning", "Fine Tuning:"},
            {"camera.scope_settings", "Scope Settings"},
            {"camera.min_fov", "Minimum FOV"},
            {"camera.max_fov", "Maximum FOV"},
            {"camera.night_vision_settings", "Night Vision Settings"},
            {"camera.enable_night_vision", "Enable Night Vision"},
            {"camera.intensity", "Intensity"},
            {"camera.noise_scale", "Noise Scale"},
            {"camera.noise_amount", "Noise Amount"},
            {"camera.green_tint", "Green Tint"},
            {"camera.reset_adjustments", "Reset Adjustments"},
            {"camera.reset_confirm_title", "Reset Confirmation"},
            {"camera.reset_confirm_text", "Are you sure you want to reset all adjustments?"},
            {"camera.reset_confirm_desc", "This will restore default position, rotation, and scale values."},
            {"camera.yes_reset", "Yes, Reset"},
            {"camera.enable_parallax", "Enable Parallax"},
            {"camera.parallax_offset", "Parallax Offset"},
            {"camera.parallax_strength", "Parallax Strength"},
            {"camera.parallax_smoothing", "Smoothing"},
            {"camera.eye_relief", "Eye Relief"},
            {"camera.exit_pupil", "Exit Pupil"},
            {"camera.exit_pupil_radius", "Exit Pupil Radius"},
            {"camera.exit_pupil_softness", "Exit Pupil Softness"},
            {"camera.vignette", "Vignette"},
            {"camera.vignette_strength", "Vignette Strength"},
            {"camera.vignette_radius", "Vignette Radius"},
            {"camera.vignette_softness", "Vignette Softness"},
            {"camera.night_vision_Tips", "(Recommended)"},
            {"camera.select_model_warning", "Please select a model file to create configuration"},
            {"camera.invalid_selection", "Invalid Selection"},
            {"camera.advanced_parallax", "Advanced Parallax Parameters"},
            {"camera.fog_radius", "Edge Fade Radius"},
            {"camera.fog_radius_tooltip", "Control fade effect at exit pupil edge"},
            {"camera.max_travel", "Max Travel"},
            {"camera.max_travel_tooltip", "Max eye offset distance, larger values mean more noticeable parallax movement"},
            {"camera.reticle_parallax", "Reticle Parallax"},
            {"camera.reticle_parallax_tooltip", "Reticle offset intensity when eye moves"},
            {"camera.distortion_settings", "Spherical Distortion Settings"},
            {"camera.enable_distortion", "Enable Spherical Distortion"},
            {"camera.distortion_tooltip", "(Simulates real lens optical distortion)"},
            {"camera.distortion_strength", "Distortion Strength"},
            {"camera.distortion_strength_desc", "Positive=Barrel, Negative=Pincushion"},
            {"camera.distortion_radius", "Distortion Radius"},
            {"camera.distortion_control_range", "Controls distortion effect range"},
            {"camera.center_offset", "Distortion Center Offset:"},
            {"camera.offset_x", "X Offset"},
            {"camera.offset_y", "Y Offset"},
            {"camera.enable_chromatic", "Enable Chromatic Aberration"},
            {"camera.chromatic_tooltip", "(High quality distortion, simulates RGB separation)"},
            {"camera.presets", "Quick Presets:"},
            {"camera.preset_barrel_slight", "Slight Barrel"},
            {"camera.preset_barrel_medium", "Medium Barrel"},
            {"camera.preset_barrel_strong", "Strong Barrel"},
            {"camera.preset_pincushion_slight", "Slight Pincushion"},
            {"camera.preset_pincushion_medium", "Medium Pincushion"},
            {"camera.reset_distortion", "Reset Distortion"},
            {"reticle.current", "Current Reticle"},
            {"reticle.texture_selection", "Texture Selection"},
            {"reticle.adjustments", "Adjustments"},
            {"reticle.scale", "Scale"},
            {"reticle.offset_x", "Offset X:"},
            {"reticle.offset_y", "Offset Y:"},
            {"reticle.preview", "Preview"},
            {"reticle.reset_defaults", "Reset to Defaults"},
            {"reticle.current_info", "Current Reticle Information"},
            {"reticle.no_config_available", "No configuration available"},
            {"reticle.current_texture", "Texture: %s"},
            {"reticle.current_scale", "Scale: %.2f"},
            {"reticle.current_offset", "Offset: [%.3f, %.3f]"},
            {"reticle.status_found", "Status: ✓ File Found"},
            {"reticle.file_size", "Size: %.2f KB"},
            {"reticle.size_unknown", "Size: Unknown"},
            {"reticle.status_not_found", "Status: ✗ File Not Found"},
            {"reticle.no_texture_selected", "No reticle texture selected"},
            {"reticle.no_textures_found", "No textures found in TTS directory"},
            {"reticle.scan_textures", "Scan Textures"},
            {"reticle.search_placeholder", "Search textures..."},
            {"reticle.select_texture", "Select Texture..."},
            {"reticle.texture_selected", "Texture Selected: {}"},
            {"reticle.horizontal_offset", "Horizontal Offset"},
            {"reticle.vertical_offset", "Vertical Offset"},
            {"reticle.reset_to_center", "Reset to Center"},
            {"reticle.texture_preview", "Texture Preview"},
            {"reticle.dimensions", "Dimensions: %dx%d"},
            {"reticle.aspect_ratio", "Aspect Ratio: %.3f"},
            {"reticle.no_preview", "No Preview Available"},
            {"reticle.load_preview", "Load Preview"},
            {"reticle.quick_actions", "Quick Actions"},
            {"reticle.reload_texture", "Reload Texture"},
            {"reticle.reload_success", "Reticle texture reload successful"},
            {"reticle.reload_error_title", "Reload Error"},
            {"reticle.reload_error_desc", "Reticle texture reload failed."},
            {"reticle.settings_saved", "Reticle settings saved"},
            {"reticle.reset_success", "Settings reset to defaults"},
            {"reticle.reset_instruction", "Hold Ctrl and click to reset to defaults"},
            {"reticle.realtime_changes", "● Changes applied in real-time"},
            {"reticle.save_instruction", " (Click 'Save Settings' to persist)"},
            {"reticle.all_saved", "✓ All Settings Saved"},
            {"reticle.show_preview", "Show Texture Preview"},
            {"reticle.directory_not_found", "TTS texture directory not found!"},
            {"reticle.textures_found", "Found {} texture files"},
            {"reticle.error_scanning", "Error scanning texture files: {}"},
            {"reticle.current_suffix", "(Current)"},
            {"debug.camera_info", "Camera Info"},
            {"debug.performance_info", "Performance Info"},
            {"debug.print_hierarchy", "Print Node Hierarchy"},
            {"debug.refresh_node", "Refresh TTSNode"},
            {"debug.scope_camera_info", "Scope Camera Info"},
            {"debug.camera_not_available", "Camera not available"},
            {"debug.local_position", "Local Position"},
            {"debug.world_position", "World Position"},
            {"debug.local_rotation", "Local Rotation"},
            {"debug.world_rotation", "World Rotation"},
            {"debug.current_fov", "Current FOV"},
            {"debug.tts_node_info", "TTSNode Info"},
            {"debug.tts_node_not_found", "TTSNode not found"},
            {"debug.local_scale", "Local Scale"},
            {"debug.rendering_status", "Rendering Status"},
            {"debug.rendering_enabled", "Rendering Enabled"},
            {"debug.is_forward_stage", "Is Forward Stage"},
            {"debug.rendering_for_scope", "Rendering for Scope"},
            {"debug.node_status", "Node Status"},
            {"debug.advanced_debug_info", "Advanced Debug Info"},
            {"debug.weapon_information", "Weapon Information"},
            {"debug.form_id", "Form ID: %08X"},
            {"debug.mod_name", "Mod Name: %s"},
            {"debug.has_config", "Has Config: %s"},
            {"debug.model_name", "Model: %s"},
            {"debug.config_source", "Config Source: %s"},
            {"debug.no_weapon_equipped", "No weapon equipped"},
            {"debug.actions", "Debug Actions"},
            {"debug.info_updated", "Debug info updated"},
            {"debug.hierarchy_printed", "Node hierarchy printed to log file"},
            {"debug.weapon_node_not_found", "Weapon node not found"},
            {"debug.player_not_available", "Player character or 3D not available"},
            {"debug.error_printing_hierarchy", "Error printing hierarchy: {}"},
            {"debug.tts_found_updated", "TTSNode found and info updated"},
            {"debug.tts_not_found_exclaim", "TTSNode not found!"},
            {"debug.clipboard_header", "TTS Debug Information"},
            {"debug.tts_position", "TTSNode Position"},
            {"debug.tts_rotation", "TTSNode Rotation"},
            {"debug.tts_scale", "TTSNode Scale"},
            {"debug.camera_position", "Camera Position"},
            {"debug.camera_fov", "Camera FOV"},
            {"debug.forward_stage", "Forward Stage"},
            {"debug.degrees", "degrees"},
            {"debug.enabled", "Enabled"},
            {"debug.disabled", "Disabled"},
            {"debug.not_found", "Not Found"},
            {"debug.load_model_first", "Please load a model first."},
            {"debug.values_copied", "Debug values copied to clipboard!"},
            {"debug.clipboard_failed", "Clipboard access failed!"},
            {"debug.error_copying", "Error copying to clipboard: {}"},
            {"debug.copy_values", "Copy Values"},
            {"debug.force_update", "Force Update Debug Info"},
            {"debug.clear_log", "Clear Debug Log"},
            {"debug.auto_refresh", "Auto Refresh"},
            {"debug.show_advanced", "Show Advanced Info"},
            {"models.current_model", "Current Model"},
            {"models.available_models", "Available Models"},
            {"models.switch_model", "Switch Model"},
            {"models.reload_current", "Reload Current Model"},
            {"models.remove_current", "Remove Current Model"},
            {"models.current_model_info", "Current Model Information"},
            {"models.no_config_available", "No configuration available"},
            {"models.current_model_label", "Model: %s"},
            {"models.status_loaded", "Status: ✓ Loaded and Active\nPosition: [%.2f, %.2f, %.2f]"},
            {"models.status_not_loaded", "Status: ⚠ Not Loaded"},
            {"models.auto_load_config", "Auto-Load from Config"},
            {"models.auto_load_success", "Model auto-loaded from config"},
            {"models.auto_load_failed", "Failed to auto-load model from config"},
            {"models.no_model_assigned", "No model assigned to this config"},
            {"models.no_models_found", "No models found in ScopeShape directory"},
            {"models.scan_models", "Scan Models"},
            {"models.search_placeholder", "Search models..."},
            {"models.select_model", "Select Model..."},
            {"models.switch_success", "Model switched to: {}"},
            {"models.switch_error_title", "Model Switch Error"},
            {"models.switch_error_desc", "Failed to switch to selected model. Please check file validity."},
            {"models.quick_actions", "Quick Actions"},
            {"models.reload_success", "Model reloaded: {}"},
            {"models.reload_error_title", "Reload Error"},
            {"models.reload_error_desc", "Failed to reload current model."},
            {"models.remove_success", "Current model removed"},
            {"models.no_model_to_remove", "No model to remove"},
            {"models.directory_not_found", "ScopeShape directory not found!"},
            {"models.files_found", "Found {} NIF files"},
            {"models.current_suffix", "(Current)"},
            {"models.error_switching", "Error switching model: {}"},
            {"models.error_previewing", "Error previewing model: {}"},
            {"models.error_removing", "Error removing model: {}"},
            {"models.error_scanning", "Error scanning NIF files: {}"},
            {"zoom.fov_multiplier", "FOV Multiplier:"},
            {"zoom.offset_x", "Offset X:"},
            {"zoom.offset_y", "Offset Y:"},
            {"zoom.offset_z", "Offset Z:"},
            {"zoom.reset_settings", "Reset Settings"},
            {"zoom.weapon_info", "Current Weapon Information"},
            {"zoom.no_weapon", "No valid weapon equipped"},
            {"zoom.weapon_label", "Weapon: [%08X] %s"},
            {"zoom.config_source_mod", "Config Source: [%08X] %s (%s)"},
            {"zoom.config_source_weapon", "Config Source: Weapon (%s)"},
            {"zoom.settings_title", "Zoom Data Settings"},
            {"zoom.position_offsets", "Position Offsets"},
            {"zoom.precise_adjustment", "Precise Adjustment:"},
            {"zoom.fine_tuning", "Fine Tuning:"},
            {"zoom.settings_saved", "Zoom data settings saved successfully!"},
            {"zoom.settings_failed", "Zoom data settings save failed!"},
            {"zoom.settings_reset", "Zoom data settings reset to defaults!"},
            {"zoom.no_config_loaded", "No configuration loaded to apply zoom data settings."},
            {"zoom.no_config_found", "No configuration found for this weapon"},
            {"common.none", "None"},
            {"common.enabled", "Enabled"},
            {"common.disabled", "Disabled"},
            {"common.loading", "Loading..."},
            {"common.error", "Error"},
            {"common.warning", "Warning"},
            {"common.info", "Info"},
            {"common.yes", "Yes"},
            {"common.no", "No"}
        };
    }

}
