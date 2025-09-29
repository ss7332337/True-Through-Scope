#include "rendering/ThermalVisionControl.h"
#include "rendering/SecondPassRenderer.h"
#include "GlobalTypes.h"
#include "rendering/TemperatureDetection.h"
#include <Windows.h>


namespace ThroughScope
{
    ThermalVisionControl* ThermalVisionControl::s_instance = nullptr;

    ThermalVisionControl* ThermalVisionControl::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new ThermalVisionControl();
        }
        return s_instance;
    }

    void ThermalVisionControl::Initialize()
    {
        m_thermalVision = ThermalVision::GetSingleton();
        logger::info("ThermalVisionControl initialized");
    }

    void ThermalVisionControl::Update(float deltaTime)
    {
        UpdateHotkeys();

        if (m_enabled && m_thermalVision)
        {
            // 更新温度检测系统
            auto tempDetection = TemperatureDetection::GetSingleton();
            tempDetection->Update(deltaTime);
        }
    }

    void ThermalVisionControl::UpdateHotkeys()
    {
        // T键 - 切换热成像
        if (GetAsyncKeyState('T') & 0x8000)
        {
            if (!m_keyPressed)
            {
                ToggleThermalVision();
                m_keyPressed = true;
            }
        }
        else
        {
            m_keyPressed = false;
        }

        // Ctrl+T - 显示/隐藏UI
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState('T') & 0x8000))
        {
            m_showUI = !m_showUI;
        }

        // P键 - 切换调色板
        static bool pKeyPressed = false;
        if (GetAsyncKeyState('P') & 0x8000)
        {
            if (!pKeyPressed)
            {
                CyclePalette();
                pKeyPressed = true;
            }
        }
        else
        {
            pKeyPressed = false;
        }

        // +/- 调整灵敏度
        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000)
        {
            IncreaseSensitivity();
        }
        if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000)
        {
            DecreaseSensitivity();
        }
    }

    void ThermalVisionControl::RenderUI()
    {
        if (!m_showUI || !m_thermalVision)
            return;

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("热成像控制", &m_showUI))
        {
            ImGui::Text("热成像系统控制面板");
            ImGui::Separator();

            // 启用/禁用
            if (ImGui::Checkbox("启用热成像", &m_enabled))
            {
                SetEnabled(m_enabled);
            }

            if (m_enabled)
            {
                ImGui::Spacing();

                // 调色板选择
                auto& config = m_thermalVision->GetConfig();
                const char* paletteNames[] = {
                    "白热", "黑热", "铁弓", "高对比彩虹",
                    "北极", "熔岩", "等温线", "医疗"
                };
                int currentPalette = (int)config.palette;
                if (ImGui::Combo("调色板", &currentPalette, paletteNames, IM_ARRAYSIZE(paletteNames)))
                {
                    config.palette = (ThermalPalette)currentPalette;
                }

                ImGui::Spacing();

                // 温度范围
                float tempRange[2] = { config.minTemperature, config.maxTemperature };
                if (ImGui::DragFloat2("温度范围(°C)", tempRange, 1.0f, -50.0f, 500.0f))
                {
                    m_thermalVision->SetTemperatureRange(tempRange[0], tempRange[1]);
                }

                // 灵敏度(NETD)
                if (ImGui::SliderFloat("灵敏度(NETD)", &config.sensitivity, 0.01f, 0.1f, "%.3f K"))
                {
                    // 灵敏度已更新
                }

                // 发射率
                if (ImGui::SliderFloat("发射率", &config.emissivity, 0.1f, 1.0f))
                {
                    // 发射率已更新
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("图像调整");

                // 增益和电平
                ImGui::SliderFloat("增益", &config.gain, 0.1f, 5.0f);
                ImGui::SliderFloat("电平", &config.level, 0.0f, 1.0f);

                // 自动增益
                ImGui::Checkbox("自动增益控制(AGC)", &config.autoGain);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("效果设置");

                // 噪声
                ImGui::Checkbox("启用热噪声", &config.enableNoise);
                if (config.enableNoise)
                {
                    ImGui::SliderFloat("噪声强度", &config.noiseIntensity, 0.0f, 0.1f);
                }

                // 边缘检测
                ImGui::Checkbox("边缘增强", &config.enableEdgeDetection);
                if (config.enableEdgeDetection)
                {
                    ImGui::SliderFloat("边缘强度", &config.edgeStrength, 0.0f, 1.0f);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("统计信息");

                // 显示温度统计
                auto tempDetection = TemperatureDetection::GetSingleton();
                ImGui::Text("环境温度: %.1f°C", tempDetection->GetAmbientTemperature());

                // 快捷键提示
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("快捷键:");
                ImGui::BulletText("T - 切换热成像");
                ImGui::BulletText("P - 切换调色板");
                ImGui::BulletText("Ctrl+T - 显示/隐藏此界面");
                ImGui::BulletText("+/- - 调整灵敏度");
            }
        }
        ImGui::End();

        // 如果热成像启用，显示状态覆盖
        if (m_enabled)
        {
            DrawThermalOverlay();
        }
    }

    void ThermalVisionControl::DrawThermalOverlay()
    {
        // 在屏幕角落显示热成像状态
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 200, 10));
        ImGui::SetNextWindowBgAlpha(0.35f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::Begin("ThermalOverlay", nullptr, flags))
        {
            auto& config = m_thermalVision->GetConfig();

            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "热成像激活");

            const char* paletteNames[] = {
                "白热", "黑热", "铁弓", "彩虹",
                "北极", "熔岩", "等温线", "医疗"
            };
            ImGui::Text("模式: %s", paletteNames[(int)config.palette]);

            ImGui::Text("范围: %.0f-%.0f°C", config.minTemperature, config.maxTemperature);

            if (config.autoGain)
            {
                ImGui::Text("AGC: 开启");
            }
        }
        ImGui::End();
    }

    void ThermalVisionControl::ToggleThermalVision()
    {
        m_enabled = !m_enabled;
        SetEnabled(m_enabled);

        logger::info("Thermal vision {}", m_enabled ? "enabled" : "disabled");
    }

    void ThermalVisionControl::SetEnabled(bool enabled)
    {
        m_enabled = enabled;

        // 通知渲染器
        // 这里需要获取SecondPassRenderer实例并设置热成像状态
        // 暂时使用全局标志或通过其他方式传递
    }

    void ThermalVisionControl::CyclePalette()
    {
        if (!m_thermalVision)
            return;

        auto& config = m_thermalVision->GetConfig();
        int currentPalette = (int)config.palette;
        currentPalette = (currentPalette + 1) % 8;
        config.palette = (ThermalPalette)currentPalette;

        const char* paletteNames[] = {
            "白热", "黑热", "铁弓", "彩虹",
            "北极", "熔岩", "等温线", "医疗"
        };
        logger::info("Switched to {} palette", paletteNames[currentPalette]);
    }

    void ThermalVisionControl::IncreaseSensitivity()
    {
        if (!m_thermalVision)
            return;

        auto& config = m_thermalVision->GetConfig();
        config.sensitivity = std::min(config.sensitivity + 0.005f, 0.1f);
    }

    void ThermalVisionControl::DecreaseSensitivity()
    {
        if (!m_thermalVision)
            return;

        auto& config = m_thermalVision->GetConfig();
        config.sensitivity = std::max(config.sensitivity - 0.005f, 0.01f);
    }
}
