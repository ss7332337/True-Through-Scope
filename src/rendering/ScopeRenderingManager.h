#pragma once

#include <memory>
#include "rendering/SecondPassRenderer.h"

namespace ThroughScope
{
    /**
     * 全局瞄具渲染管理器
     * 
     * 用于协调分阶段渲染（fo4test 兼容模式）：
     * - 场景渲染阶段在 DrawWorld_Forward 之前执行
     * - 后处理阶段在 TAA 之后执行
     * 
     * 这确保瞄具内容被包含在 fo4test 的运动矢量捕获中
     */
    class ScopeRenderingManager
    {
    public:
        static ScopeRenderingManager* GetSingleton()
        {
            static ScopeRenderingManager instance;
            return &instance;
        }

        /**
         * 检查是否启用了 fo4test 兼容模式
         * 当检测到 fo4test 相关模块加载时自动启用
         */
        bool IsFO4TestCompatibilityEnabled() const { return m_fo4testCompatEnabled; }
        
        /**
         * 初始化管理器，检测 fo4test 模块
         */
        void Initialize()
        {
            // 检测 fo4test 相关模块是否加载
            // 注意：fo4test 可能以多个 DLL 形式存在
            bool detected = 
                GetModuleHandleA("AAAFrameGeneration.dll") != nullptr ||
                GetModuleHandleA("FrameGeneration.dll") != nullptr ||
                GetModuleHandleA("Upscaling.dll") != nullptr ||
                GetModuleHandleA("MotionVectorFixes.dll") != nullptr;
                
            if (detected) {
                logger::info("[ScopeRenderingManager] FO4Test mods detected");
                // 使用 PreUI Hook 进行渲染（而非 Forward 阶段）
                // PreUI 发生在 DLSS/FSR3 之前，所以瞄具内容会被包含在 upscaling 中
                logger::info("[ScopeRenderingManager] Enabling compatibility mode (PreUI rendering)");
                m_fo4testCompatEnabled = true;
            } else {
                logger::info("[ScopeRenderingManager] No FO4Test mods detected, using standard TAA hook");
                m_fo4testCompatEnabled = false;
            }
        }
        
        /**
         * 执行场景渲染阶段（在 Forward 之前调用）
         * @return true 如果成功渲染
         */
        bool ExecuteSceneRenderingPhase();
        
        /**
         * 执行后处理阶段（在 TAA 之后调用）
         * @return true 如果成功处理
         */
        bool ExecutePostProcessingPhase();
        
        /**
         * 检查场景渲染是否已完成（本帧）
         */
        bool IsSceneRenderingCompleteThisFrame() const { return m_sceneRenderingCompleteThisFrame; }
        
        /**
         * 帧结束时重置状态
         */
        void OnFrameEnd()
        {
            m_sceneRenderingCompleteThisFrame = false;
            m_currentRenderer.reset();
        }
        
        /**
         * 设置当前帧要使用的渲染器资源
         */
        void SetCurrentFrameResources(ID3D11DeviceContext* context, ID3D11Device* device, D3DHooks* d3dHooks);

    private:
        ScopeRenderingManager() = default;
        ~ScopeRenderingManager() = default;
        ScopeRenderingManager(const ScopeRenderingManager&) = delete;
        ScopeRenderingManager& operator=(const ScopeRenderingManager&) = delete;
        
        bool m_fo4testCompatEnabled = false;
        bool m_sceneRenderingCompleteThisFrame = false;
        
        // 当前帧的渲染器实例
        std::unique_ptr<SecondPassRenderer> m_currentRenderer;
        
        // 缓存的 D3D 资源指针
        ID3D11DeviceContext* m_cachedContext = nullptr;
        ID3D11Device* m_cachedDevice = nullptr;
        D3DHooks* m_cachedD3DHooks = nullptr;
    };
}
