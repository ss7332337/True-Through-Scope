#include "RenderOptimization.h"
#include <algorithm>

namespace ThroughScope
{
    RenderOptimization* RenderOptimization::GetSingleton()
    {
        static RenderOptimization instance;
        return &instance;
    }

    void RenderOptimization::SetSettings(const OptimizationSettings& settings)
    {
        m_settings = settings;
    }

    void RenderOptimization::SetQualityLevel(QualityLevel level)
    {
        m_settings.qualityLevel = level;
        ApplyPreset(level);
    }

    void RenderOptimization::ApplyPreset(QualityLevel level)
    {
        SetQualityPreset(level);
    }

    void RenderOptimization::SetQualityPreset(QualityLevel level)
    {
        m_settings.qualityLevel = level;

        switch (level) {
            case QualityLevel::Ultra:
                // 极致画质 - 无优化
                m_settings.enableLightLimiting = false;
                m_settings.maxScopeLights = 32;
                m_settings.skipOcclusionMap = false;
                m_settings.skipDecals = false;
                m_settings.skipDistantObjects = false;
                m_settings.skipShadows = false;
                m_settings.skipReflections = false;
                m_settings.skipAO = false;
                m_settings.skipVolumetrics = false;
                m_settings.skipPostProcessing = false;
                m_settings.optimizeGBufferClear = false;
                m_settings.enableFrameSkip = false;
                m_settings.frameSkipInterval = 1;
                break;

            case QualityLevel::High:
                // 高画质 - 轻度优化
                m_settings.enableLightLimiting = true;
                m_settings.maxScopeLights = 16;
                m_settings.skipOcclusionMap = true;
                m_settings.skipDecals = false;
                m_settings.skipDistantObjects = true;
                m_settings.skipShadows = false;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = false;
                m_settings.optimizeGBufferClear = true;
                m_settings.enableFrameSkip = false;
                m_settings.frameSkipInterval = 1;
                break;

            case QualityLevel::Medium:
                // 中画质 - 中度优化（推荐）
                m_settings.enableLightLimiting = true;
                m_settings.maxScopeLights = 12;
                m_settings.skipOcclusionMap = true;
                m_settings.skipDecals = true;
                m_settings.skipDistantObjects = true;
                m_settings.skipShadows = true;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = true;
                m_settings.optimizeGBufferClear = true;
                m_settings.enableFrameSkip = false;
                m_settings.frameSkipInterval = 1;
                break;

            case QualityLevel::Low:
                // 低画质 - 重度优化
                m_settings.enableLightLimiting = true;
                m_settings.maxScopeLights = 8;
                m_settings.skipOcclusionMap = true;
                m_settings.skipDecals = true;
                m_settings.skipDistantObjects = true;
                m_settings.skipShadows = true;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = true;
                m_settings.optimizeGBufferClear = true;
                m_settings.enableFrameSkip = false;
                m_settings.frameSkipInterval = 1;
                break;

            case QualityLevel::Performance:
                // 性能模式 - 极限优化
                m_settings.enableLightLimiting = true;
                m_settings.maxScopeLights = 4;
                m_settings.skipOcclusionMap = true;
                m_settings.skipDecals = true;
                m_settings.skipDistantObjects = true;
                m_settings.skipShadows = true;
                m_settings.skipReflections = true;
                m_settings.skipAO = true;
                m_settings.skipVolumetrics = true;
                m_settings.skipPostProcessing = true;
                m_settings.optimizeGBufferClear = true;
                m_settings.enableFrameSkip = true;
                m_settings.frameSkipInterval = 2;  // 每2帧更新一次
                break;
        }
    }

    bool RenderOptimization::ShouldRenderThisFrame()
    {
        if (!m_settings.enableFrameSkip) {
            return true;
        }

        m_frameCounter++;
        if (m_frameCounter >= m_settings.frameSkipInterval) {
            m_frameCounter = 0;
            m_stats.renderedFrameCount++;
            return true;
        }

        m_stats.skippedFrameCount++;
        return false;
    }

    void RenderOptimization::UpdateDynamicQuality(float frameTime)
    {
        if (!m_settings.enableDynamicQuality) {
            return;
        }

        const float targetDiff = frameTime - m_settings.targetFrameTime;
        const float adjustThreshold = 2.0f; // ms

        if (std::abs(targetDiff) > adjustThreshold) {
            if (targetDiff > 0) {
                // 帧时间过长，降低质量
                m_currentQualityScale = std::max(0.5f, m_currentQualityScale - 0.05f);

                // 考虑自动降低光源数量
                if (m_settings.enableLightLimiting) {
					m_settings.maxScopeLights = std::max((size_t)4u,
                        static_cast<size_t>(m_settings.maxScopeLights * 0.9f));
                }
            } else {
                // 帧时间充裕，提高质量
                m_currentQualityScale = std::min(1.0f, m_currentQualityScale + 0.05f);

                // 考虑自动增加光源数量
                if (m_settings.enableLightLimiting) {
					m_settings.maxScopeLights = std::min((size_t)16u,
                        static_cast<size_t>(m_settings.maxScopeLights * 1.1f));
                }
            }
        }
    }

    void RenderOptimization::SaveCurrentAsCustomPreset()
    {
        m_customPreset = m_settings;
        m_hasCustomPreset = true;
    }

    void RenderOptimization::LoadCustomPreset()
    {
        if (m_hasCustomPreset) {
            m_settings = m_customPreset;
        }
    }

    void RenderOptimization::UpdateStats(float frameTime, float scopeRenderTime, int lightCount)
    {
        // 简单的移动平均
        const float alpha = 0.1f;
        m_stats.avgFrameTime = m_stats.avgFrameTime * (1.0f - alpha) + frameTime * alpha;
        m_stats.avgScopeRenderTime = m_stats.avgScopeRenderTime * (1.0f - alpha) + scopeRenderTime * alpha;
        m_stats.activeLightCount = lightCount;
    }

    void RenderOptimization::ResetStats()
    {
        m_stats = PerformanceStats{};
        m_frameCounter = 0;
    }

    const char* RenderOptimization::GetQualityLevelName(QualityLevel level)
    {
        switch (level) {
            case QualityLevel::Ultra:       return "Ultra";
            case QualityLevel::High:        return "High";
            case QualityLevel::Medium:      return "Medium";
            case QualityLevel::Low:         return "Low";
            case QualityLevel::Performance: return "Performance";
            default:                        return "Unknown";
        }
    }

    const char* RenderOptimization::GetQualityLevelDescription(QualityLevel level)
    {
        switch (level) {
            case QualityLevel::Ultra:
                return "Extreme quality - No optimizations, full rendering";
            case QualityLevel::High:
                return "High quality - Light optimizations";
            case QualityLevel::Medium:
                return "Medium quality - Balanced (Recommended)";
            case QualityLevel::Low:
                return "Low quality - Heavy optimizations";
            case QualityLevel::Performance:
                return "Performance mode - Maximum optimizations";
            default:
                return "";
        }
    }
}
