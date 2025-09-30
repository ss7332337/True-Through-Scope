#include "PerformancePanel.h"
#include <imgui.h>

namespace ThroughScope
{
    PerformancePanel::PerformancePanel(PanelManagerInterface* manager)
        : m_Manager(manager)
        , m_RenderOpt(RenderOptimization::GetSingleton())
    {
        LoadCurrentSettings();
    }

    void PerformancePanel::Render()
    {
        if (!ImGui::BeginTabBar("PerformanceTabBar")) {
            return;
        }

        // 质量预设选项卡
        if (ImGui::BeginTabItem("Quality Presets")) {
            RenderQualityPresets();
            ImGui::EndTabItem();
        }

        // 光照设置选项卡
        if (ImGui::BeginTabItem("Lighting")) {
            RenderLightingSettings();
            ImGui::EndTabItem();
        }

        // 渲染阶段选项卡
        if (ImGui::BeginTabItem("Render Stages")) {
            RenderRenderStageSettings();
            ImGui::EndTabItem();
        }

        // 高级设置选项卡
        if (ImGui::BeginTabItem("Advanced")) {
            RenderAdvancedSettings();
            ImGui::EndTabItem();
        }

        // 性能统计选项卡
        if (ImGui::BeginTabItem("Statistics")) {
            RenderPerformanceStats();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();

        // 底部操作按钮
        RenderActionButtons();
    }

    void PerformancePanel::Update()
    {
        // 定期更新当前设置（如果没有未保存的更改）
        if (!m_settingsChanged) {
            LoadCurrentSettings();
        }
    }

    void PerformancePanel::RenderQualityPresets()
    {
        RenderSectionHeader("Quality Presets");

        // 全局优化开关 - 用于对比测试
        bool optimizationsEnabled = m_currentSettings.enableOptimizations;
        if (ImGui::Checkbox("Enable All Optimizations", &optimizationsEnabled)) {
            m_currentSettings.enableOptimizations = optimizationsEnabled;
            m_RenderOpt->SetEnableOptimizations(optimizationsEnabled);
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Master switch for all optimizations\n"
                         "Disable to see unoptimized scope rendering for comparison");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!optimizationsEnabled) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
            ImGui::TextWrapped("All optimizations are disabled. Scope will render at full quality.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        ImGui::Text("Select a quality preset for scope rendering:");
        ImGui::Spacing();

        const char* presetNames[] = { "Ultra", "High", "Medium", "Low", "Performance" };
        const char* presetDescriptions[] = {
            "Extreme quality - No optimizations, full rendering",
            "High quality - Light optimizations",
            "Medium quality - Balanced (Recommended)",
            "Low quality - Heavy optimizations",
            "Performance mode - Maximum optimizations"
        };

        int currentLevel = static_cast<int>(m_currentSettings.qualityLevel);

        for (int i = 0; i < 5; i++) {
            bool isSelected = (currentLevel == i);

            if (ImGui::RadioButton(presetNames[i], isSelected)) {
                m_selectedQualityLevel = i;
                m_RenderOpt->SetQualityLevel(static_cast<RenderOptimization::QualityLevel>(i));
                LoadCurrentSettings();
                MarkSettingsChanged();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", presetDescriptions[i]);
            }

            ImGui::Spacing();
        }

        ImGui::Separator();

        // 自定义预设管理
        ImGui::Text("Custom Preset:");
        if (ImGui::Button("Save Current as Custom")) {
            m_RenderOpt->SaveCurrentAsCustomPreset();
        }
        RenderHelpTooltip("Save your current settings as a custom preset");

        ImGui::SameLine();
        if (ImGui::Button("Load Custom Preset")) {
            m_RenderOpt->LoadCustomPreset();
            LoadCurrentSettings();
            MarkSettingsChanged();
        }
        if (!m_RenderOpt->HasCustomPreset()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(No custom preset saved)");
        }
    }

    void PerformancePanel::RenderLightingSettings()
    {
        RenderSectionHeader("Lighting Optimization");

        ImGui::Checkbox("Enable Light Limiting", &m_currentSettings.enableLightLimiting);
        RenderHelpTooltip("Limit the number of lights rendered in the scope scene");

        if (m_currentSettings.enableLightLimiting) {
            ImGui::Indent();

            int maxLights = static_cast<int>(m_currentSettings.maxScopeLights);
            ImGui::SliderInt("Max Scope Lights", &maxLights, 4, 32);
            m_currentSettings.maxScopeLights = static_cast<size_t>(maxLights);
            RenderHelpTooltip("Maximum number of lights in the scope scene\n"
                             "Lower = Better performance\n"
                             "Higher = Better quality");

            ImGui::Text("Light Priority Factors:");
            ImGui::BulletText("Distance to camera");
            ImGui::BulletText("Light intensity");
            ImGui::BulletText("Dynamic lights (higher priority)");
            ImGui::BulletText("Frustum visibility");

            ImGui::Unindent();
        }

        if (ImGui::Checkbox("Skip Shadows", &m_currentSettings.skipShadows)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip shadow rendering in scope scene\n"
                         "Significant performance boost");
    }

    void PerformancePanel::RenderRenderStageSettings()
    {
        RenderSectionHeader("Render Stage Skipping");

        ImGui::Text("Skip these rendering stages in the scope scene:");
        ImGui::Spacing();

        if (ImGui::Checkbox("Skip Occlusion Map", &m_currentSettings.skipOcclusionMap)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip occlusion culling map generation\n+5-10% performance");

        if (ImGui::Checkbox("Skip Decals", &m_currentSettings.skipDecals)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip decal rendering (blood, bullet holes, etc.)\n+5-8% performance");

        if (ImGui::Checkbox("Skip Distant Objects", &m_currentSettings.skipDistantObjects)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip distant object rendering\n+8-12% performance");

        if (ImGui::Checkbox("Skip Reflections", &m_currentSettings.skipReflections)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip reflection rendering\n+10-15% performance");

        if (ImGui::Checkbox("Skip Ambient Occlusion", &m_currentSettings.skipAO)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip AO calculation\n+8-12% performance");

        if (ImGui::Checkbox("Skip Volumetrics", &m_currentSettings.skipVolumetrics)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip volumetric effects (fog, god rays)\n+10-15% performance");

        if (ImGui::Checkbox("Skip Post Processing", &m_currentSettings.skipPostProcessing)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Skip post-processing effects\n+5-10% performance");

        ImGui::Separator();

        if (ImGui::Checkbox("Optimize G-Buffer Clear", &m_currentSettings.optimizeGBufferClear)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Only clear essential render buffers\n+5-8% performance");
    }

    void PerformancePanel::RenderAdvancedSettings()
    {
        RenderSectionHeader("Advanced Options");

        // 帧跳过设置
        if (ImGui::Checkbox("Enable Frame Skip", &m_currentSettings.enableFrameSkip)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Update scope scene every N frames instead of every frame\n"
                         "Can cause visible stuttering but gives huge performance boost");

        if (m_currentSettings.enableFrameSkip) {
            ImGui::Indent();

            int frameSkip = m_currentSettings.frameSkipInterval;
            ImGui::SliderInt("Frame Skip Interval", &frameSkip, 1, 4);
            m_currentSettings.frameSkipInterval = frameSkip;
            RenderHelpTooltip("1 = Every frame (no skip)\n"
                             "2 = Every 2nd frame (30fps scope)\n"
                             "3 = Every 3rd frame (20fps scope)\n"
                             "4 = Every 4th frame (15fps scope)");

            float effectiveFPS = 60.0f / frameSkip;
            ImGui::Text("Effective Scope FPS: %.1f", effectiveFPS);

            ImGui::Unindent();
        }

        ImGui::Separator();

        // 动态质量调整
        if (ImGui::Checkbox("Enable Dynamic Quality", &m_currentSettings.enableDynamicQuality)) {
            MarkSettingsChanged();
        }
        RenderHelpTooltip("Automatically adjust quality based on frame time\n"
                         "Experimental feature");

        if (m_currentSettings.enableDynamicQuality) {
            ImGui::Indent();

            ImGui::SliderFloat("Target Frame Time (ms)", &m_currentSettings.targetFrameTime, 8.33f, 33.33f);
            RenderHelpTooltip("Target frame time in milliseconds\n"
                             "16.67ms = 60fps\n"
                             "33.33ms = 30fps");

            ImGui::Unindent();
        }
    }

    void PerformancePanel::RenderPerformanceStats()
    {
        RenderSectionHeader("Performance Statistics");

        const auto& stats = m_RenderOpt->GetStats();

        ImGui::Text("Average Frame Time: %.2f ms", stats.avgFrameTime);
        ImGui::Text("Average Scope Render Time: %.2f ms", stats.avgScopeRenderTime);
        ImGui::Text("Active Light Count: %d", stats.activeLightCount);
        ImGui::Text("Rendered Frames: %d", stats.renderedFrameCount);
        ImGui::Text("Skipped Frames: %d", stats.skippedFrameCount);

        ImGui::Separator();

        // 性能影响估算
        ImGui::Text("Estimated Performance Impact:");

        float perfGain = 0.0f;

        if (m_currentSettings.enableLightLimiting) {
            float lightReduction = 1.0f - (m_currentSettings.maxScopeLights / 32.0f);
            perfGain += lightReduction * 25.0f;
        }

        if (m_currentSettings.skipShadows) perfGain += 20.0f;
        if (m_currentSettings.skipOcclusionMap) perfGain += 7.0f;
        if (m_currentSettings.skipDecals) perfGain += 6.0f;
        if (m_currentSettings.skipDistantObjects) perfGain += 10.0f;
        if (m_currentSettings.skipReflections) perfGain += 12.0f;
        if (m_currentSettings.skipAO) perfGain += 10.0f;
        if (m_currentSettings.skipVolumetrics) perfGain += 12.0f;
        if (m_currentSettings.skipPostProcessing) perfGain += 7.0f;
        if (m_currentSettings.optimizeGBufferClear) perfGain += 5.0f;

        if (m_currentSettings.enableFrameSkip) {
            perfGain += 50.0f / m_currentSettings.frameSkipInterval;
        }

        ImGui::TextColored(m_SuccessColor, "Estimated Performance Gain: +%.1f%%", perfGain);

        ImGui::Spacing();

        if (ImGui::Button("Reset Statistics")) {
            m_RenderOpt->ResetStats();
        }
    }

    void PerformancePanel::RenderActionButtons()
    {
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Apply Settings")) {
            ApplySettings();
        }
        RenderHelpTooltip("Apply current settings to the renderer");

        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults")) {
            ResetToDefaults();
        }
        RenderHelpTooltip("Reset all settings to default values (Medium quality)");

        ImGui::SameLine();
        if (m_settingsChanged) {
            ImGui::TextColored(m_WarningColor, "Unsaved Changes");
        } else {
            ImGui::TextColored(m_SuccessColor, "Saved");
        }
    }

    void PerformancePanel::LoadCurrentSettings()
    {
        m_currentSettings = m_RenderOpt->GetSettings();
        m_selectedQualityLevel = static_cast<int>(m_currentSettings.qualityLevel);
    }

    void PerformancePanel::ApplySettings()
    {
        m_RenderOpt->SetSettings(m_currentSettings);
        m_settingsChanged = false;
        m_isSaved = true;
    }

    void PerformancePanel::ResetToDefaults()
    {
        m_RenderOpt->SetQualityLevel(RenderOptimization::QualityLevel::Medium);
        LoadCurrentSettings();
        ApplySettings();
    }

    void PerformancePanel::MarkSettingsChanged()
    {
        m_settingsChanged = true;
        m_isSaved = false;
        m_Manager->MarkUnsavedChanges();
    }
}