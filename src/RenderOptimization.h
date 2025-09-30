#pragma once

namespace ThroughScope
{
    /**
     * @brief 渲染优化管理器
     *
     * 统一管理瞄具场景渲染的各种性能优化设置
     * 包括质量级别、光源限制、渲染阶段跳过等
     */
    class RenderOptimization
    {
    public:
        /**
         * @brief 渲染质量级别
         */
        enum class QualityLevel
        {
            Ultra = 0,      // 极致画质 - 无优化，完整渲染
            High = 1,       // 高画质 - 轻度优化
            Medium = 2,     // 中画质 - 中度优化（推荐）
            Low = 3,        // 低画质 - 重度优化
            Performance = 4 // 性能模式 - 极限优化
        };

        /**
         * @brief 优化设置结构
         */
        struct OptimizationSettings
        {
            // 总开关
            bool enableOptimizations = true;        // 完全关闭/启用所有优化（用于对比测试）

            // 质量级别
            QualityLevel qualityLevel = QualityLevel::Medium;

            // 光源优化
            bool enableLightLimiting = true;        // 启用光源数量限制
            size_t maxScopeLights = 8;              // 瞄具场景最大光源数量

            // 渲染阶段跳过
            bool skipOcclusionMap = true;           // 跳过遮挡图渲染
            bool skipDecals = true;                 // 跳过贴花渲染
            bool skipDistantObjects = true;         // 跳过远景对象渲染
            bool skipShadows = true;                // 跳过阴影渲染
            bool skipReflections = true;            // 跳过反射
            bool skipAO = true;                     // 跳过环境光遮蔽
            bool skipVolumetrics = true;            // 跳过体积效果
            bool skipPostProcessing = true;         // 跳过后处理效果

            // G-Buffer清理优化
            bool optimizeGBufferClear = true;       // 优化G-Buffer清理（只清理必要的缓冲区）

            // 帧率控制
            bool enableFrameSkip = false;           // 启用帧跳过（每N帧更新一次瞄具场景）
            int frameSkipInterval = 1;              // 帧跳过间隔（1=每帧更新，2=每2帧更新一次）

            // 高级选项
            bool enableDynamicQuality = false;      // 启用动态质量调整（根据帧时间自动调整）
            float targetFrameTime = 16.67f;         // 目标帧时间（毫秒，60fps=16.67ms）
        };

        static RenderOptimization* GetSingleton();

        // 设置管理
        const OptimizationSettings& GetSettings() const { return m_settings; }
        void SetSettings(const OptimizationSettings& settings);

        // 总开关控制
        void SetEnableOptimizations(bool enable) { m_settings.enableOptimizations = enable; }
        bool IsOptimizationsEnabled() const { return m_settings.enableOptimizations; }

        // 质量级别快速设置
        void SetQualityLevel(QualityLevel level);
        QualityLevel GetQualityLevel() const { return m_settings.qualityLevel; }

        // 光源优化控制
        void SetEnableLightLimiting(bool enable) { m_settings.enableLightLimiting = enable; }
        void SetMaxScopeLights(size_t maxLights) { m_settings.maxScopeLights = maxLights; }
        bool IsLightLimitingEnabled() const { return m_settings.enableOptimizations && m_settings.enableLightLimiting; }
        size_t GetMaxScopeLights() const { return m_settings.maxScopeLights; }

        // 渲染阶段查询（用于Hook中判断是否跳过）
        // 注意：所有查询都会检查总开关 enableOptimizations
        bool ShouldSkipOcclusionMap() const { return m_settings.enableOptimizations && m_settings.skipOcclusionMap; }
        bool ShouldSkipDecals() const { return m_settings.enableOptimizations && m_settings.skipDecals; }
        bool ShouldSkipDistantObjects() const { return m_settings.enableOptimizations && m_settings.skipDistantObjects; }
        bool ShouldSkipShadows() const { return m_settings.enableOptimizations && m_settings.skipShadows; }
        bool ShouldSkipReflections() const { return m_settings.enableOptimizations && m_settings.skipReflections; }
        bool ShouldSkipAO() const { return m_settings.enableOptimizations && m_settings.skipAO; }
        bool ShouldSkipVolumetrics() const { return m_settings.enableOptimizations && m_settings.skipVolumetrics; }
        bool ShouldSkipPostProcessing() const { return m_settings.enableOptimizations && m_settings.skipPostProcessing; }
        bool ShouldOptimizeGBufferClear() const { return m_settings.enableOptimizations && m_settings.optimizeGBufferClear; }

        // 帧跳过控制
        bool IsFrameSkipEnabled() const { return m_settings.enableOptimizations && m_settings.enableFrameSkip; }
        int GetFrameSkipInterval() const { return m_settings.frameSkipInterval; }
        bool ShouldRenderThisFrame();  // 根据帧计数器判断是否应该渲染

        // 动态质量调整
        void UpdateDynamicQuality(float frameTime);

        // 预设管理
        void ApplyPreset(QualityLevel level);
        void SaveCurrentAsCustomPreset();
        void LoadCustomPreset();
        bool HasCustomPreset() const { return m_hasCustomPreset; }

        // 性能统计
        struct PerformanceStats
        {
            float avgFrameTime = 0.0f;          // 平均帧时间
            float avgScopeRenderTime = 0.0f;    // 平均瞄具渲染时间
            int activeLightCount = 0;            // 当前激活光源数量
            int renderedFrameCount = 0;          // 已渲染帧数
            int skippedFrameCount = 0;           // 跳过帧数
        };

        const PerformanceStats& GetStats() const { return m_stats; }
        void UpdateStats(float frameTime, float scopeRenderTime, int lightCount);
        void ResetStats();

        // 质量级别名称（用于UI显示）
        static const char* GetQualityLevelName(QualityLevel level);
        static const char* GetQualityLevelDescription(QualityLevel level);

    private:
        RenderOptimization() = default;
        ~RenderOptimization() = default;
        RenderOptimization(const RenderOptimization&) = delete;
        RenderOptimization& operator=(const RenderOptimization&) = delete;

        OptimizationSettings m_settings;
        OptimizationSettings m_customPreset;
        bool m_hasCustomPreset = false;

        // 帧跳过计数器
        int m_frameCounter = 0;

        // 性能统计
        PerformanceStats m_stats;

        // 动态质量调整
        float m_currentQualityScale = 1.0f;

        // 预设配置
        void SetQualityPreset(QualityLevel level);
    };
}