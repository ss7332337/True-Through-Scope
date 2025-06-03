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
        { "简体中文", "zh_CN" },
        { "繁體中文", "zh_TW" },
        { "日本語", "ja" },
        { "한국어", "ko" },
        { "Deutsch", "de" },
        { "Français", "fr" },
        { "Español", "es" },
        { "Русский", "ru" }
    };

    LocalizationManager* LocalizationManager::GetSingleton() 
    {
        static LocalizationManager instance;
        return &instance;
    }

    bool LocalizationManager::Initialize() 
    {
        if (m_Initialized) {
            return true;
        }

        // 创建语言文件目录（如果不存在）
        std::filesystem::path langDir = "Data/F4SE/Plugins/TrueThroughScope/Languages";
        
        if (!std::filesystem::exists(langDir)) {
            std::filesystem::create_directories(langDir);
        }

        // 加载所有语言文件
        bool success = true;
        for (int i = 0; i < static_cast<int>(Language::COUNT); ++i) {
            Language lang = static_cast<Language>(i);
            if (!LoadLanguageFile(lang)) {
                success = false;
                // 如果是英语加载失败，这是严重错误
                if (lang == Language::English) {
                    return false;
                }
            }
        }


        m_Initialized = true;
        return success;
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
            std::ifstream file(filePath);
            if (!file.is_open()) {
                // 如果文件不存在，为英语创建默认文件
                if (language == Language::English) {
                    CreateDefaultEnglishFile(filePath);
                    file.open(filePath);
                }
                if (!file.is_open()) {
                    return false;
                }
            }

            nlohmann::json json;
            file >> json;
            file.close();

            // 解析JSON并存储翻译
            auto& translations = m_Translations[language];
            for (auto& [key, value] : json.items()) {
                if (value.is_string()) {
                    translations[key] = value.get<std::string>();
                }
            }

            return true;
        }
        catch (const std::exception& e) {
            // 记录错误（如果有日志系统的话）
            return false;
        }
    }

    void LocalizationManager::CreateDefaultEnglishFile(const std::string& filePath) 
    {
        nlohmann::json defaultTranslations = {
            // 通用UI文本
            {"ui.menu.settings", "Settings"},
            {"ui.menu.camera", "Camera Adjustment"},
            {"ui.menu.reticle", "Reticle"},
            {"ui.menu.debug", "Debug"},
            {"ui.menu.models", "Model Switcher"},
            {"ui.menu.zoom", "Zoom Data"},
            
            // 设置面板
            {"settings.tab.interface", "Interface"},
            {"settings.tab.performance", "Performance"},
            {"settings.tab.keybindings", "Key Bindings"},
            {"settings.tab.advanced", "Advanced"},
            
            {"settings.ui.language", "Language / 语言:"},
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
            
            // 按钮
            {"button.save", "Save"},
            {"button.reset", "Reset to Defaults"},
            {"button.cancel", "Cancel"},
            {"button.apply", "Apply"},
            {"button.create", "Create Configuration"},
            {"button.rescan", "Rescan for NIF Files"},
            {"button.clear", "Clear"},
            {"button.refresh", "Refresh"},
            
            // 状态
            {"status.unsaved_changes", "● Unsaved changes"},
            {"status.all_saved", "○ All saved"},
            {"status.night_vision_on", "Night Vision: ON"},
            {"status.night_vision_off", "Night Vision: OFF"},
            
            // 相机调整面板
            {"camera.config.target", "Configuration Target:"},
            {"camera.config.scope_shape", "Scope Shape (Model File):"},
            {"camera.config.no_config", "No configuration found for this weapon"},
            {"camera.config.create_desc", "Create a new configuration to start customizing your scope settings."},
            {"camera.config.no_nif_files", "No NIF files found in Meshes/TTS/ScopeShape/"},
            {"camera.config.base_weapon", "Base Weapon"},
            {"camera.config.modification", "Modification #{} - {}"},
            {"camera.config.select_model", "Select a model..."},
            
            // 错误和警告
            {"error.reset_confirm", "Are you sure you want to reset ALL settings to defaults?"},
            {"error.cannot_undo", "This action cannot be undone."},
            {"warning.advanced_users", "Warning: These settings are for advanced users only!"},
            
            // 工具提示
            {"tooltip.base_weapon", "Create configuration for the base weapon"},
            {"tooltip.modification", "Create configuration specific to this modification"},
            
            // 相机调整面板详细设置
            {"camera.position", "Position"},
            {"camera.rotation", "Rotation"},
            {"camera.scale", "Scale"},
            {"camera.parallax", "Parallax Settings"},
            {"camera.night_vision", "Night Vision"},
            {"camera.thermal_vision", "Thermal Vision"},
            {"camera.reset_all", "Reset All Adjustments"},
            {"camera.apply_settings", "Apply Settings"},
            
            // 准星面板
            {"reticle.current", "Current Reticle"},
            {"reticle.texture_selection", "Texture Selection"},
            {"reticle.adjustments", "Adjustments"},
            {"reticle.scale", "Scale:"},
            {"reticle.offset_x", "Offset X:"},
            {"reticle.offset_y", "Offset Y:"},
            {"reticle.preview", "Preview"},
            {"reticle.reset_defaults", "Reset to Defaults"},
            
            // 调试面板
            {"debug.camera_info", "Camera Information"},
            {"debug.tts_node_info", "TTSNode Information"},
            {"debug.rendering_status", "Rendering Status"},
            {"debug.performance_info", "Performance Information"},
            {"debug.print_hierarchy", "Print Node Hierarchy"},
            {"debug.refresh_node", "Refresh TTSNode"},
            {"debug.copy_values", "Copy Values to Clipboard"},
            
            // 模型切换器面板
            {"models.current_model", "Current Model"},
            {"models.available_models", "Available Models"},
            {"models.switch_model", "Switch Model"},
            {"models.reload_current", "Reload Current Model"},
            {"models.remove_current", "Remove Current Model"},
            
            // 变焦数据面板
            {"zoom.fov_multiplier", "FOV Multiplier:"},
            {"zoom.offset_x", "Offset X:"},
            {"zoom.offset_y", "Offset Y:"},
            {"zoom.offset_z", "Offset Z:"},
            {"zoom.reset_settings", "Reset Settings"},
            
            // 通用
            {"common.none", "None"},
            {"common.enabled", "Enabled"},
            {"common.disabled", "Disabled"},
            {"common.loading", "Loading..."},
            {"common.error", "Error"},
            {"common.warning", "Warning"},
            {"common.info", "Information"},
            {"common.yes", "Yes"},
            {"common.no", "No"},
            
            // Debug panel
            {"debug.scope_camera_info", "Scope Camera Information"},
            {"debug.camera_not_available", "Camera not available"},
            {"debug.local_position", "Local Position"},
            {"debug.world_position", "World Position"},
            {"debug.local_rotation", "Local Rotation"},
            {"debug.world_rotation", "World Rotation"},
            {"debug.current_fov", "Current FOV"},
            {"debug.tts_node_info", "TTSNode Information"},
            {"debug.tts_node_not_found", "TTSNode not found"},
            {"debug.local_scale", "Local Scale"},
            {"debug.rendering_status", "Rendering Status"},
            {"debug.rendering_enabled", "Rendering Enabled"},
            {"debug.is_forward_stage", "Is Forward Stage"},
            {"debug.rendering_for_scope", "Rendering for Scope"},
            {"debug.node_status", "Node Status"},
            {"debug.advanced_debug_info", "Advanced Debug Information"},
            {"debug.weapon_information", "Weapon Information"},
            {"debug.form_id", "Form ID: %08X"},
            {"debug.mod_name", "Mod Name: %s"},
            {"debug.has_config", "Has Config: %s"},
            {"debug.model_name", "Model: %s"},
            {"debug.config_source", "Config Source: %s"},
            {"debug.no_weapon_equipped", "No weapon equipped"},
            {"debug.actions", "Debug Actions"},
            {"debug.print_hierarchy", "Print Node Hierarchy"},
            {"debug.refresh_node", "Refresh TTSNode"},
            {"debug.copy_values", "Copy Values"},
            {"debug.force_update", "Force Update Debug Info"},
            {"debug.clear_log", "Clear Debug Log"},
            {"debug.auto_refresh", "Auto Refresh"},
            {"debug.show_advanced", "Show Advanced Info"},
            {"debug.info_updated", "Debug information updated"},
            {"debug.hierarchy_printed", "Node hierarchy printed to log file"},
            {"debug.weapon_node_not_found", "Weapon node not found"},
            {"debug.player_not_available", "Player character or 3D not available"},
            {"debug.error_printing_hierarchy", "Error printing hierarchy: {}"},
            {"debug.tts_found_updated", "TTSNode found and information updated"},
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
            {"debug.clipboard_failed", "Failed to access clipboard!"},
            {"debug.error_copying", "Error copying to clipboard: {}"},
            
            // Debug tooltips
            {"tooltip.print_hierarchy", "Print the weapon node hierarchy to the log file"},
            {"tooltip.refresh_node", "Force refresh TTSNode information"},
            {"tooltip.copy_values", "Copy current debug values to clipboard"},
            {"tooltip.auto_refresh", "Automatically refresh debug information"},
            {"tooltip.show_advanced_debug", "Show additional debug information and performance data"},
            {"tooltip.refresh_interval", "Debug information refresh interval"},
            
            // 更多相机调整面板文本
            {"camera.weapon_info", "Current Weapon Information"},
            {"camera.no_weapon", "No valid weapon equipped"},
            {"camera.equip_weapon", "Please equip a weapon with scope capabilities to configure settings."},
            {"camera.weapon_label", "Weapon: [%08X] %s"},
            {"camera.config_source", "Config Source: [%08X] %s (%s)"},
            {"camera.current_model", "Current Model: %s"},
            {"camera.reload_tts", "Reload TTSNode"},
            {"camera.reload_tooltip", "Click to recreate the TTSNode from the saved model file"},
            {"camera.position_controls", "Position Controls"},
            {"camera.rotation_controls", "Rotation Controls"},
            {"camera.scale_controls", "Scale Controls"},
            {"camera.precise_adjustment", "Precise Adjustment:"},
            {"camera.fine_tuning", "Fine Tuning:"},
            {"camera.scope_settings", "Scope Settings"},
            {"camera.min_fov", "Minimum FOV"},
            {"camera.max_fov", "Maximum FOV"},
            {"camera.night_vision_settings", "Night Vision Settings"},
            {"camera.thermal_vision_settings", "Thermal Vision Settings"},
            {"camera.enable_night_vision", "Enable Night Vision"},
            {"camera.enable_thermal_vision", "Enable Thermal Vision"},
            {"camera.night_vision_Tips", "(Click the Save button to take effect)"},
            {"camera.intensity", "Intensity"},
            {"camera.noise_scale", "Noise Scale"},
            {"camera.noise_amount", "Noise Amount"},
            {"camera.green_tint", "Green Tint"},
            {"camera.threshold", "Threshold"},
            {"camera.contrast", "Contrast"},
            {"camera.reset_adjustments", "Reset Adjustments"},
            {"camera.reset_confirm_title", "Reset Confirmation"},
            {"camera.reset_confirm_text", "Are you sure you want to reset all adjustments?"},
            {"camera.reset_confirm_desc", "This will restore default position, rotation, and scale values."},
            {"camera.yes_reset", "Yes, Reset"},
            
            // 视差设置
            {"camera.relative_fog_radius", "Relative Fog Radius"},
            {"camera.scope_sway_amount", "Scope Sway Amount"},
            {"camera.max_travel", "Max Travel"},
            {"camera.parallax_radius", "Radius"},
            
            // 准星面板详细文本
            {"reticle.no_config", "No reticle configuration available"},
            {"reticle.equip_scope", "Please equip a weapon with a scope to configure reticle settings."},
            {"reticle.current_texture", "Current Texture: %s"},
            {"reticle.no_texture", "No texture loaded"},
            {"reticle.search_placeholder", "Search textures..."},
            {"reticle.no_textures_found", "No texture files found"},
            {"reticle.scan_textures", "Scan for Textures"},
            {"reticle.apply_settings", "Apply Settings"},
            {"reticle.settings_applied", "Reticle settings applied successfully!"},
            {"reticle.failed_apply", "Failed to apply reticle settings!"},
            {"reticle.click_save", "  (Click 'Save Settings' to persist)"},
            
            // 调试面板详细文本
            {"debug.local_pos", "Local Position: [%.3f, %.3f, %.3f]"},
            {"debug.world_pos", "World Position: [%.3f, %.3f, %.3f]"},
            {"debug.local_rot", "Local Rotation: [%.1f°, %.1f°, %.1f°]"},
            {"debug.world_rot", "World Rotation: [%.1f°, %.1f°, %.1f°]"},
            {"debug.current_fov", "Current FOV: %.1f"},
            {"debug.local_scale", "Local Scale: %.3f"},
            {"debug.node_exists", "Node Exists: %s"},
            {"debug.rendering_enabled", "Rendering Enabled: %s"},
            {"debug.forward_stage", "Forward Stage: %s"},
            {"debug.rendering_for_scope", "Rendering for Scope: %s"},
            {"debug.frame_time", "Frame Time: %.3f ms"},
            {"debug.frame_count", "Frame Count: %d"},
            
            // 模型切换器面板详细文本
            {"models.no_config", "No model configuration available"},
            {"models.equip_scope", "Please equip a weapon with a scope to switch models."},
            {"models.current_model_label", "Current Model: %s"},
            {"models.no_model", "No model loaded"},
            {"models.search_placeholder", "Search models..."},
            {"models.no_models_found", "No model files found"},
            {"models.scan_models", "Scan for Models"},
            {"models.switch_success", "Model switched successfully!"},
            {"models.switch_failed", "Failed to switch model!"},
            {"models.reload_success", "Model reloaded successfully!"},
            {"models.reload_failed", "Failed to reload model!"},
            {"models.remove_success", "Model removed successfully!"},
            {"models.remove_failed", "Failed to remove model!"},
            
            // ModelSwitcherPanel详细文本
            {"models.current_model_info", "Current Model Information"},
            {"models.no_config_available", "No configuration available"},
            {"models.status_loaded", "Status: ✓ Loaded and Active\nPosition: [%.2f, %.2f, %.2f]"},
            {"models.status_not_loaded", "Status: ⚠ Not Loaded"},
            {"models.auto_load_config", "Auto-Load from Config"},
            {"models.auto_load_success", "Model auto-loaded from configuration"},
            {"models.auto_load_failed", "Failed to auto-load model from configuration"},
            {"models.no_model_assigned", "No model assigned to this configuration"},
            {"models.select_model", "Select Model..."},
            {"models.switch_error_title", "Model Switch Error"},
            {"models.switch_error_desc", "Failed to switch to the selected model. Please check if the file is valid."},
            {"models.quick_actions", "Quick Actions"},
            {"models.reload_error_title", "Reload Error"},
            {"models.reload_error_desc", "Failed to reload the current model."},
            {"models.no_model_to_remove", "No model to remove"},
            {"models.directory_not_found", "ScopeShape directory not found!"},
            {"models.files_found", "Found {} NIF files"},
            {"models.current_suffix", "(Current)"},
            {"models.error_switching", "Error switching model: {}"},
            {"models.error_previewing", "Error previewing model: {}"},
            {"models.error_removing", "Error removing model: {}"},
            {"models.error_scanning", "Error scanning NIF files: {}"},
            
            // ModelSwitcherPanel工具提示
            {"tooltip.auto_load_config", "Load the model specified in the current configuration"},
            {"tooltip.click_to_switch", "Click to switch to this model\nFile: %s"},
            {"tooltip.reload_current", "Reload the current model from the configuration"},
            {"tooltip.remove_current", "Remove the currently loaded TTSNode"},
            
            // 变焦数据面板详细文本
            {"zoom.weapon_info", "Current Weapon Information"},
            {"zoom.no_config", "No zoom configuration available"},
            {"zoom.equip_scope", "Please equip a weapon with a scope to configure zoom settings."},
            {"zoom.current_values", "Current Values"},
            {"zoom.fov_mult_desc", "Field of view multiplier for zoom levels"},
            {"zoom.offset_desc", "Position offset adjustments"},
            {"zoom.settings_saved", "Zoom settings saved successfully!"},
            {"zoom.settings_failed", "Failed to save zoom settings!"},
            
            // ZoomDataPanel详细文本
            {"zoom.no_weapon", "No valid weapon equipped"},
            {"zoom.weapon_label", "Weapon: [%08X] %s"},
            {"zoom.config_source_mod", "Config Source: [%08X] %s (%s)"},
            {"zoom.config_source_weapon", "Config Source: Weapon (%s)"},
            {"zoom.settings_title", "Zoom Data Settings"},
            {"zoom.position_offsets", "Position Offsets"},
            {"zoom.precise_adjustment", "Precise Adjustment:"},
            {"zoom.fine_tuning", "Fine Tuning:"},
            {"zoom.settings_reset", "Zoom data settings reset to defaults!"},
            {"zoom.no_config_loaded", "No configuration loaded to apply zoom data settings."},
            
            // ReticlePanel详细文本
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
            {"reticle.scan_textures", "Scan for Textures"},
            {"reticle.search_placeholder", "Search textures..."},
            {"reticle.select_texture", "Select Texture..."},
            {"reticle.texture_selected", "Texture selected: {}"},
            {"reticle.scale", "Scale"},
            {"reticle.horizontal_offset", "Horizontal Offset"},
            {"reticle.vertical_offset", "Vertical Offset"},
            {"reticle.reset_to_center", "Reset to Center"},
            {"reticle.texture_preview", "Texture Preview"},
            {"reticle.dimensions", "Dimensions: %dx%d"},
            {"reticle.aspect_ratio", "Aspect Ratio: %.3f"},
            {"reticle.no_preview", "No preview available"},
            {"reticle.load_preview", "Load Preview"},
            {"reticle.quick_actions", "Quick Actions"},
            {"reticle.reload_texture", "Reload Texture"},
            {"reticle.reload_success", "Reticle texture reloaded successfully"},
            {"reticle.reload_error_title", "Reload Error"},
            {"reticle.reload_error_desc", "Failed to reload reticle texture."},
            {"reticle.settings_saved", "Reticle settings saved"},
            {"reticle.reset_defaults", "Reset to Defaults"},
            {"reticle.reset_success", "Settings reset to defaults"},
            {"reticle.reset_instruction", "Hold Ctrl and click to reset to defaults"},
            {"reticle.realtime_changes", "● Changes applied in real-time"},
            {"reticle.save_instruction", "  (Click 'Save Settings' to persist)"},
            {"reticle.all_saved", "✓ All settings saved"},
            {"reticle.show_preview", "Show Texture Preview"},
            {"reticle.directory_not_found", "TTS texture directory not found!"},
            {"reticle.textures_found", "Found {} texture files"},
            {"reticle.error_scanning", "Error scanning texture files: {}"},
            {"reticle.current_suffix", "(Current)"},
            
            // ZoomDataPanel和ReticlePanel工具提示
            {"tooltip.fov_multiplier", "Multiplier applied to the field of view when zooming"},
            {"tooltip.reticle_scale", "Adjust the size of the reticle (0.1 = very small, 1 = very large)"},
            {"tooltip.horizontal_offset", "Horizontal position of the reticle (0.0 = left, 0.5 = center, 1.0 = right)"},
            {"tooltip.vertical_offset", "Vertical position of the reticle (0.0 = top, 0.5 = center, 1.0 = bottom)"},
            {"tooltip.reset_to_center", "Reset position to screen center"},
            {"tooltip.click_to_select_texture", "Click to select this texture\nFile: %s"},
            {"tooltip.reload_texture", "Reload the current texture file"},
            {"tooltip.save_settings", "Save current settings to configuration file"},
            {"tooltip.reset_defaults", "Hold Ctrl and click to reset all settings to defaults"},
            {"tooltip.show_preview", "Show texture preview and information"},
            
            // 设置面板更多文本
            {"settings.show_tooltips", "Show Help Tooltips"},
            {"settings.auto_save", "Auto-Save Changes"},
            {"settings.confirm_reset", "Confirm Before Reset"},
            {"settings.realtime_adjust", "Real-time Adjustment"},
            {"settings.auto_save_interval", "Auto-save interval (seconds)"},
            {"settings.enable_vsync", "Enable V-Sync"},
            {"settings.optimize_performance", "Optimize for Performance"},
            {"settings.reduced_animations", "Reduced Animations"},
            {"settings.show_advanced", "Show Advanced Settings"},
            {"settings.open_config", "Open Config Folder"},
            {"settings.reset_all", "Reset All Settings"},
            {"settings.yes_reset_all", "Yes, Reset Everything"},
            
            // 工具提示文本
            {"tooltip.show_tooltips", "Show helpful tooltips when hovering over UI elements"},
            {"tooltip.auto_save", "Automatically save changes periodically"},
            {"tooltip.auto_save_interval", "How often to auto-save (in seconds)"},
            {"tooltip.confirm_reset", "Show confirmation dialog before resetting adjustments"},
            {"tooltip.realtime_adjust", "Apply changes immediately as you adjust sliders"},
            {"tooltip.refresh_rate", "Lower refresh rates can improve performance"},
            {"tooltip.advanced_settings", "Show advanced configuration options"},
            {"tooltip.vsync", "Synchronize frame rate with display refresh rate"},
            {"tooltip.optimize_performance", "Reduce visual quality for better performance"},
            {"tooltip.max_fps", "Maximum frame rate limit (0 = unlimited)"},
            {"tooltip.reduced_animations", "Reduce UI animations to improve performance"},
            {"tooltip.select_language", "Select your preferred language"},
            
            // 键位绑定工具提示
            {"tooltip.key_none", "Select 'None' to disable a key binding"},
            {"tooltip.key_combination", "For combination keys, choose both modifier and main key"},
            {"tooltip.key_immediate", "Changes are applied immediately"},
            
            // 状态消息
            {"status.settings_saved", "Settings saved successfully!"},
            {"status.settings_failed", "Failed to save settings!"},
            {"status.changes_cancelled", "Changes cancelled"},
            {"status.settings_applied", "Settings applied"},
            {"status.all_reset", "All settings reset to defaults"},
            {"status.config_created", "Configuration created successfully! Model: %s"},
            {"status.config_failed", "Failed to create configuration!"},
            {"status.tts_reloaded", "TTSNode reloaded from: %s"},
            {"status.tts_reload_failed", "Failed to reload TTSNode from: %s"},
            {"status.nif_files_found", "Found %d NIF files"},
            {"status.nif_scan_error", "Error scanning NIF files: %s"},
            {"status.adjustments_reset", "All adjustments reset!"},
            {"status.font_rebuilt", "Font rebuilt for language support"},
            
            // 状态栏显示文本
            {"status.WeaponLoaded", "✓ Weapon Loaded"},
            {"status.WeaponNotLoaded", "⚠ No Weapon"},
            {"status.TTSNodeReady", "✓ TTSNode Ready"},
            {"status.TTSNodeNotReady", "⚠ No TTSNode"},
            
            // 备选状态文本（如果符号无法显示）
            {"status.WeaponLoaded_alt", "[OK] Weapon Loaded"},
            {"status.WeaponNotLoaded_alt", "[!] No Weapon"},
            {"status.TTSNodeReady_alt", "[OK] TTSNode Ready"},
            {"status.TTSNodeNotReady_alt", "[!] No TTSNode"},

			{ "camera.relative_fog_radius", "Relative Fog Radius" },
			{ "camera.scope_sway_amount", "Scope Sway Amount" },
			{ "camera.max_travel", "Max Travel" },
			{ "camera.parallax_radius", "Radius" },
        };

        std::ofstream file(filePath);
        if (file.is_open()) {
            file << defaultTranslations.dump(4);
            file.close();
        }
    }
} 
