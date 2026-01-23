#pragma once

/*
 * ENB DOF 崩溃问题隔离测试框架
 * 
 * 使用方法：
 * 1. 设置 TTS_ISOLATE_LEVEL 为 0-6 之间的值
 * 2. 编译并测试
 * 3. 根据测试结果调整级别，逐步定位问题
 * 
 * 隔离级别说明：
 * 0 = 完全禁用TTS（基线测试）
 * 1 = 仅执行纹理备份（无渲染、无恢复）
 * 2 = 备份 + 清理渲染目标（无渲染、无恢复）
 * 3 = 备份 + 相机切换（无渲染、无恢复）
 * 4 = 完整流程但跳过实际游戏渲染（g_RenderPreUIOriginal）
 * 5 = 完整流程但跳过纹理恢复（RestoreFirstPass中的SafeCopyTexture）
 * 6 = 完整流程但跳过SetScopeTexture（Stencil写入和DrawIndexed）
 * 
 * 默认值：-1（正常模式，不启用隔离）
 */

#ifndef TTS_ISOLATE_LEVEL
#define TTS_ISOLATE_LEVEL -1  // -1 = 正常模式，0-6 = 隔离测试级别
#endif

// 辅助宏：检查是否启用隔离测试
#define TTS_ISOLATION_ENABLED (TTS_ISOLATE_LEVEL >= 0)

// 辅助宏：检查是否达到指定隔离级别
#define TTS_ISOLATE_LEVEL_EQ(level) (TTS_ISOLATE_LEVEL == (level))
#define TTS_ISOLATE_LEVEL_GE(level) (TTS_ISOLATE_LEVEL >= (level))

// ===== SetScopeTexture 细粒度隔离（LEVEL=6 时使用）=====
// 说明：以下开关用于定位 SetScopeTexture 内部的具体崩溃点
// 建议一次只启用一个开关进行测试

#ifndef TTS_SCOPE_SKIP_COPY
#define TTS_SCOPE_SKIP_COPY 0  // 1 = 跳过 Resolve/Copy 到 staging 纹理
#endif

#ifndef TTS_SCOPE_DISABLE_STENCIL
#define TTS_SCOPE_DISABLE_STENCIL 1  // 1 = 禁用 stencil 写入（StencilEnable=false）
#endif

#ifndef TTS_SCOPE_NULL_DSV
#define TTS_SCOPE_NULL_DSV 0  // 1 = 绑定 nullptr DSV（不使用深度/模板）
#endif

#ifndef TTS_SCOPE_SKIP_DRAW
#define TTS_SCOPE_SKIP_DRAW 0  // 1 = 跳过 DrawIndexed（仅设置状态）
#endif

// ===== SetScopeTexture 分段提前返回（定位崩溃位置）=====
// 设置为 0~6 中的某个值，将在对应步骤后直接返回
// -1 = 不启用
#ifndef TTS_SCOPE_EARLY_RETURN_STEP
#define TTS_SCOPE_EARLY_RETURN_STEP -1
#endif

// ===== DrawScopeContent/ExecuteSecondPass 分段提前返回（Level 4 时定位崩溃）=====
// 设置为 0~30 中的某个值，将在对应步骤后直接返回
// -1 = 不启用（正常执行）
// 
// DrawScopeContent 内部：
// 0 = UpdateSceneGraph 后返回
// 1 = SetCameraData 后返回
// 2 = 绑定 RTV/DSV 后返回（在 g_RenderPreUIOriginal 之前）
// 3 = PosAdjust 恢复后返回（跳过 GetAndResetCullingStats）[崩溃]
// 4 = 函数末尾返回（GetAndResetCullingStats 之后，析构之前）
// 
// ExecuteSecondPass（DrawScopeContent 和 RestoreFirstPass 之间）：
// 10 = RestoreFirstPass 入口立即返回（不崩溃）
// 20 = DrawScopeContent 返回后立即返回
// 21 = m_renderExecuted 赋值后返回
// 22 = RestoreScopeAimingAfterRender 后返回
// 
// RestoreFirstPass 内部：
// 11 = RestoreFirstPass 光照恢复后返回
// 12 = RestoreFirstPass Flush 前返回
// 13 = RestoreFirstPass Flush 后返回
#ifndef TTS_DRAW_EARLY_RETURN_STEP
#define TTS_DRAW_EARLY_RETURN_STEP -1
#endif
