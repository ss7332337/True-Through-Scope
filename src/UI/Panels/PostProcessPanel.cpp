#include "PostProcessPanel.h"
#include "ScopeHDR.h"
#include "BloomPass.h"

namespace ThroughScope
{
    PostProcessPanel::PostProcessPanel(PanelManagerInterface* manager) :
        m_Manager(manager)
    {
    }

    void PostProcessPanel::Render()
    {
        RenderPresetButtons();
        ImGui::Spacing();

        RenderHDRSection();
        ImGui::Spacing();

        RenderBloomSection();
        ImGui::Spacing();

        RenderQuickActions();
    }

    void PostProcessPanel::Update()
    {
        // 可以在这里添加实时更新逻辑
    }

    void PostProcessPanel::RenderHDRSection()
    {
        auto hdr = ScopeHDR::GetSingleton();
        if (!hdr || !hdr->IsInitialized()) {
            ImGui::TextColored(m_WarningColor, "HDR system not initialized");
            return;
        }

        ImGui::SetNextItemOpen(m_ShowHDRSection, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("HDR / Tonemapping")) {
            m_ShowHDRSection = true;

            auto& constants = hdr->GetConstants();

            // 基础曝光控制
            RenderSectionHeader("Exposure Control");

            if (SliderFloatWithReset("Fixed Exposure", &constants.FixedExposure, 0.0f, 5.0f, 0.7f, "%.2f")) {
                MarkChanged();
            }
            RenderHelpTooltip("Fixed exposure value. Set to 0 for auto exposure.");

            if (SliderFloatWithReset("Exposure Multiplier", &constants.ExposureMultiplier, 0.1f, 5.0f, 1.5f, "%.2f")) {
                MarkChanged();
            }
            RenderHelpTooltip("Multiplier for auto exposure calculation.");

            if (SliderFloatWithReset("Min Exposure", &constants.MinExposure, 0.01f, 2.0f, 0.5f, "%.2f")) {
                MarkChanged();
            }

            if (SliderFloatWithReset("Max Exposure", &constants.MaxExposure, 1.0f, 30.0f, 15.0f, "%.1f")) {
                MarkChanged();
            }

            // Tonemapping 参数
            ImGui::Spacing();
            RenderSectionHeader("Tonemapping");

            if (SliderFloatWithReset("Middle Gray", &constants.MiddleGray, 0.05f, 0.5f, 0.18f, "%.3f")) {
                MarkChanged();
            }
            RenderHelpTooltip("Target middle gray value for exposure calculation.");

            if (SliderFloatWithReset("White Point", &constants.WhitePoint, 0.001f, 0.2f, 0.03f, "%.3f")) {
                MarkChanged();
            }
            RenderHelpTooltip("White point for Reinhard tonemapping.");

            bool skipTonemap = constants.SkipHDRTonemapping != 0;
            if (ImGui::Checkbox("Skip Tonemapping", &skipTonemap)) {
                constants.SkipHDRTonemapping = skipTonemap ? 1 : 0;
                MarkChanged();
            }
            RenderHelpTooltip("Bypass HDR tonemapping (for debugging).");

            // 颜色调整
            ImGui::Spacing();
            RenderSectionHeader("Color Adjustment");

            if (SliderFloatWithReset("Saturation", &constants.Saturation, 0.0f, 3.0f, 1.2f, "%.2f")) {
                MarkChanged();
            }

            if (SliderFloatWithReset("Contrast", &constants.Contrast, 0.5f, 3.0f, 1.3f, "%.2f")) {
                MarkChanged();
            }

            if (SliderFloatWithReset("Bloom Strength", &constants.BloomStrength, 0.0f, 5.0f, 1.0f, "%.2f")) {
                MarkChanged();
            }

            // 高级选项 - Color Tint
            ImGui::Spacing();
            if (ImGui::TreeNode("Color Tint (Advanced)")) {
                float tint[4] = { constants.ColorTintR, constants.ColorTintG, constants.ColorTintB, constants.ColorTintW };
                if (ImGui::ColorEdit3("Tint Color", tint)) {
                    constants.ColorTintR = tint[0];
                    constants.ColorTintG = tint[1];
                    constants.ColorTintB = tint[2];
                    MarkChanged();
                }

                if (SliderFloatWithReset("Tint Weight", &constants.ColorTintW, 0.0f, 1.0f, 0.0f, "%.2f")) {
                    MarkChanged();
                }

                ImGui::TreePop();
            }

            // Bloom UV 缩放
            if (ImGui::TreeNode("Bloom UV Scale (Advanced)")) {
                if (SliderFloatWithReset("Bloom UV Scale X", &constants.BloomUVScaleX, 0.1f, 2.0f, 1.0f, "%.2f")) {
                    MarkChanged();
                }
                if (SliderFloatWithReset("Bloom UV Scale Y", &constants.BloomUVScaleY, 0.1f, 2.0f, 1.0f, "%.2f")) {
                    MarkChanged();
                }
                ImGui::TreePop();
            }
        } else {
            m_ShowHDRSection = false;
        }
    }

    void PostProcessPanel::RenderBloomSection()
    {
        auto bloom = BloomPass::GetSingleton();
        if (!bloom || !bloom->IsInitialized()) {
            ImGui::TextColored(m_WarningColor, "Bloom system not initialized");
            return;
        }

        ImGui::SetNextItemOpen(m_ShowBloomSection, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Bloom")) {
            m_ShowBloomSection = true;

            auto& accConstants = bloom->GetAccumulateConstants();
            auto& blurConstants = bloom->GetBlurConstants();

            // Bloom 累加参数
            RenderSectionHeader("Bloom Accumulate");

            if (SliderFloatWithReset("Bloom Multiplier", &accConstants.BloomMultiplier, 0.0f, 5.0f, 1.0f, "%.2f")) {
                MarkChanged();
            }
            RenderHelpTooltip("Overall bloom intensity multiplier.");

            if (SliderFloatWithReset("Bloom UV Scale X", &accConstants.BloomUVScaleX, 0.1f, 2.0f, 1.0f, "%.2f")) {
                MarkChanged();
            }

            if (SliderFloatWithReset("Bloom UV Scale Y", &accConstants.BloomUVScaleY, 0.1f, 2.0f, 1.0f, "%.2f")) {
                MarkChanged();
            }

            // Bloom Offset Weights
            if (ImGui::TreeNode("Bloom Offset Weights (Advanced)")) {
                ImGui::Text("4-tap bloom sampling offsets and weights:");
                ImGui::Spacing();

                for (int i = 0; i < 4; ++i) {
                    ImGui::PushID(i);
                    char label[32];
                    snprintf(label, sizeof(label), "Sample %d", i);

                    if (ImGui::TreeNode(label)) {
                        bool changed = false;
                        changed |= ImGui::SliderFloat("Offset X", &accConstants.BloomOffsetWeight[i].x, -3.0f, 3.0f, "%.2f");
                        changed |= ImGui::SliderFloat("Offset Y", &accConstants.BloomOffsetWeight[i].y, -3.0f, 3.0f, "%.2f");
                        changed |= ImGui::SliderFloat("Weight", &accConstants.BloomOffsetWeight[i].z, 0.0f, 1.0f, "%.3f");

                        if (changed) {
                            MarkChanged();
                        }

                        // 重置按钮
                        if (ImGui::Button("Reset")) {
                            float defaultOffsets[4][2] = { {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f} };
                            accConstants.BloomOffsetWeight[i].x = defaultOffsets[i][0];
                            accConstants.BloomOffsetWeight[i].y = defaultOffsets[i][1];
                            accConstants.BloomOffsetWeight[i].z = 0.25f;
                            MarkChanged();
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }

            // 高斯模糊参数
            ImGui::Spacing();
            RenderSectionHeader("Gaussian Blur");

            if (ImGui::TreeNode("Blur Weights (Advanced)")) {
                ImGui::Text("7-tap Gaussian blur weights:");
                ImGui::Text("Sum should be close to 1.0 for proper normalization.");
                ImGui::Spacing();

                float weightSum = 0.0f;
                for (int i = 0; i < 7; ++i) {
                    weightSum += blurConstants.BlurOffsetWeight[i].z;
                }
                ImGui::Text("Current sum: %.4f", weightSum);

                if (weightSum < 0.99f || weightSum > 1.01f) {
                    ImGui::SameLine();
                    ImGui::TextColored(m_WarningColor, "(!)");
                }

                ImGui::Spacing();

                for (int i = 0; i < 7; ++i) {
                    ImGui::PushID(100 + i);
                    char label[32];
                    snprintf(label, sizeof(label), "Tap %d Weight", i - 3);  // -3 to +3

                    if (ImGui::SliderFloat(label, &blurConstants.BlurOffsetWeight[i].z, 0.0f, 0.5f, "%.4f")) {
                        MarkChanged();
                    }
                    ImGui::PopID();
                }

                // 重置为默认高斯权重
                if (ImGui::Button("Reset to Default Gaussian")) {
                    float defaultWeights[7] = { 0.03663f, 0.11128f, 0.21675f, 0.27068f, 0.21675f, 0.11128f, 0.03663f };
                    for (int i = 0; i < 7; ++i) {
                        blurConstants.BlurOffsetWeight[i].z = defaultWeights[i];
                    }
                    MarkChanged();
                }

                ImGui::TreePop();
            }
        } else {
            m_ShowBloomSection = false;
        }
    }

    void PostProcessPanel::RenderPresetButtons()
    {
        RenderSectionHeader("HDR Presets");

        if (ImGui::Button("Default")) {
            ApplyHDRPresetDefault();
        }
        RenderHelpTooltip("Reset to default values from code.");

        ImGui::SameLine();
        if (ImGui::Button("Bright")) {
            ApplyHDRPresetBright();
        }
        RenderHelpTooltip("Increased brightness for dark scopes.");

        ImGui::SameLine();
        if (ImGui::Button("Cinematic")) {
            ApplyHDRPresetCinematic();
        }
        RenderHelpTooltip("More contrast and saturation for cinematic look.");

        ImGui::SameLine();
        if (ImGui::Button("High Contrast")) {
            ApplyHDRPresetHighContrast();
        }
        RenderHelpTooltip("High contrast for visibility.");
    }

    void PostProcessPanel::RenderQuickActions()
    {
        RenderSectionHeader("Quick Actions");

        auto hdr = ScopeHDR::GetSingleton();
        auto bloom = BloomPass::GetSingleton();

        if (ImGui::Button("Reset All to Default")) {
            ApplyHDRPresetDefault();

            if (bloom && bloom->IsInitialized()) {
                bloom->SetAccumulateConstants(BloomAccumulateConstants());
                bloom->SetBlurConstants(GaussianBlurConstants());
            }

            MarkChanged();
        }
        RenderHelpTooltip("Reset all post-processing parameters to default values.");

        // 状态显示
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Status:");

        if (hdr && hdr->IsInitialized()) {
            ImGui::TextColored(m_SuccessColor, "  HDR: Initialized");
        } else {
            ImGui::TextColored(m_ErrorColor, "  HDR: Not Initialized");
        }

        if (bloom && bloom->IsInitialized()) {
            ImGui::TextColored(m_SuccessColor, "  Bloom: Initialized");
        } else {
            ImGui::TextColored(m_ErrorColor, "  Bloom: Not Initialized");
        }
    }

    void PostProcessPanel::ApplyHDRPresetDefault()
    {
        auto hdr = ScopeHDR::GetSingleton();
        if (hdr && hdr->IsInitialized()) {
            hdr->SetConstants(ScopeHDRConstants());
            MarkChanged();
        }
    }

    void PostProcessPanel::ApplyHDRPresetBright()
    {
        auto hdr = ScopeHDR::GetSingleton();
        if (hdr && hdr->IsInitialized()) {
            auto& c = hdr->GetConstants();
            c.FixedExposure = 1.2f;
            c.ExposureMultiplier = 2.0f;
            c.MinExposure = 0.8f;
            c.MaxExposure = 20.0f;
            c.Saturation = 1.1f;
            c.Contrast = 1.1f;
            c.BloomStrength = 0.8f;
            MarkChanged();
        }
    }

    void PostProcessPanel::ApplyHDRPresetCinematic()
    {
        auto hdr = ScopeHDR::GetSingleton();
        if (hdr && hdr->IsInitialized()) {
            auto& c = hdr->GetConstants();
            c.FixedExposure = 0.6f;
            c.ExposureMultiplier = 1.2f;
            c.MinExposure = 0.3f;
            c.MaxExposure = 12.0f;
            c.Saturation = 1.4f;
            c.Contrast = 1.5f;
            c.BloomStrength = 1.5f;
            c.MiddleGray = 0.15f;
            c.WhitePoint = 0.04f;
            MarkChanged();
        }
    }

    void PostProcessPanel::ApplyHDRPresetHighContrast()
    {
        auto hdr = ScopeHDR::GetSingleton();
        if (hdr && hdr->IsInitialized()) {
            auto& c = hdr->GetConstants();
            c.FixedExposure = 0.8f;
            c.ExposureMultiplier = 1.5f;
            c.MinExposure = 0.5f;
            c.MaxExposure = 15.0f;
            c.Saturation = 1.3f;
            c.Contrast = 1.8f;
            c.BloomStrength = 0.5f;
            c.MiddleGray = 0.2f;
            MarkChanged();
        }
    }

    bool PostProcessPanel::SliderFloatWithReset(const char* label, float* v, float v_min, float v_max, float reset_value, const char* format)
    {
        bool changed = false;

        ImGui::PushID(label);

        // 滑块
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
        changed = ImGui::SliderFloat("##slider", v, v_min, v_max, format);

        // 重置按钮
        ImGui::SameLine();
        if (ImGui::Button("R", ImVec2(20, 0))) {
            *v = reset_value;
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to %.2f", reset_value);
        }

        // 标签
        ImGui::SameLine();
        ImGui::Text("%s", label);

        ImGui::PopID();

        return changed;
    }

    bool PostProcessPanel::SliderIntWithReset(const char* label, int* v, int v_min, int v_max, int reset_value)
    {
        bool changed = false;

        ImGui::PushID(label);

        // 滑块
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
        changed = ImGui::SliderInt("##slider", v, v_min, v_max);

        // 重置按钮
        ImGui::SameLine();
        if (ImGui::Button("R", ImVec2(20, 0))) {
            *v = reset_value;
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to %d", reset_value);
        }

        // 标签
        ImGui::SameLine();
        ImGui::Text("%s", label);

        ImGui::PopID();

        return changed;
    }

    void PostProcessPanel::MarkChanged()
    {
        if (m_IsSaved) {
            m_IsSaved = false;
            m_Manager->MarkUnsavedChanges();
        }
    }
}
