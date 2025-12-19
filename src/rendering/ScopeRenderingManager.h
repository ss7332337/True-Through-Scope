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
         * 检查是否启用了 Upscaling (DLSS/FSR3)
         * 这些模块会完全替换 TAA，所以 TAA hook 不会被调用
         */
        bool IsUpscalingActive() const { return m_upscalingActive; }
        
        /**
         * 初始化管理器，检测 fo4test 模块
         */
        void Initialize()
        {
            // 分别检测不同模块
            // Upscaling 完全替换 TAA，需要特殊处理
            m_upscalingActive = 
                GetModuleHandleA("Upscaling.dll") != nullptr;
            
            // 其他 fo4test 模块不替换 TAA，可以使用 TAA hook
            bool otherFO4TestMods = 
                GetModuleHandleA("AAAFrameGeneration.dll") != nullptr ||
                GetModuleHandleA("FrameGeneration.dll") != nullptr ||
                GetModuleHandleA("MotionVectorFixes.dll") != nullptr;
                
            m_fo4testCompatEnabled = m_upscalingActive || otherFO4TestMods;
            
            if (m_upscalingActive) {
                logger::info("[ScopeRenderingManager] Upscaling.dll detected - TAA is replaced");
                
                // 诊断: 检查关键函数是否已被其他 mod hook
                DiagnoseHookStatus();
            } else if (otherFO4TestMods) {
                logger::info("[ScopeRenderingManager] FO4Test mods detected (no Upscaling) - using TAA hook");
            } else {
                logger::info("[ScopeRenderingManager] No FO4Test mods detected, using standard TAA hook");
            }
        }
        
        /**
         * 诊断关键函数是否已被其他 mod hook
         * 检查函数开头是否有 JMP 指令 (0xE9)
         */
        void DiagnoseHookStatus()
        {
            logger::info("[Hook Diagnostic] Checking if critical functions are hooked by fo4test...");
            
            // UI::BeginRender - REL::ID 1056045
            CheckFunctionHooked(REL::ID(1056045), "UI::BeginRender");
            
            // DrawWorld::Render_UI - REL::ID 1130087
            CheckFunctionHooked(REL::ID(1130087), "DrawWorld::Render_UI");
            
            // DrawWorld::Render_PreUI - REL::ID 984743
            CheckFunctionHooked(REL::ID(984743), "DrawWorld::Render_PreUI");
            
            // ImageSpaceManager::RenderEffectRange - REL::ID 459505
            CheckFunctionHooked(REL::ID(459505), "ImageSpaceManager::RenderEffectRange");
            
            // TAA - REL::ID 528052
            CheckFunctionHooked(REL::ID(528052), "ImageSpaceEffectTemporalAA::Render");
            
            // ImageSpaceEffectHDR::Render - 查找其 REL::ID
            // ImageSpaceEffectBokehDOF::Render
            
            logger::info("[Hook Diagnostic] Done.");
        }
        
        void CheckFunctionHooked(REL::ID relId, const char* funcName)
        {
            try {
                REL::Relocation<uintptr_t> func{ relId };
                uintptr_t addr = func.address();
                uint8_t* bytes = reinterpret_cast<uint8_t*>(addr);
                
                // 检查常见的 hook 模式
                bool hooked = false;
                const char* hookType = "NOT HOOKED";
                
                if (bytes[0] == 0xE9) {
                    hooked = true;
                    hookType = "JMP (0xE9) - MinHook style";
                } else if (bytes[0] == 0xFF && bytes[1] == 0x25) {
                    hooked = true;
                    hookType = "JMP [addr] (0xFF 0x25) - absolute jump";
                } else if (bytes[0] == 0x48 && bytes[1] == 0xB8) {
                    hooked = true;
                    hookType = "MOV RAX,imm64 (0x48 0xB8) - trampoline prep";
                }
                
                logger::info("[Hook Diagnostic] {} @ 0x{:X}: {} | First bytes: {:02X} {:02X} {:02X} {:02X} {:02X}",
                    funcName, addr, hookType,
                    bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
                    
            } catch (...) {
                logger::warn("[Hook Diagnostic] Failed to check {} - REL::ID may be invalid", funcName);
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
        bool m_upscalingActive = false;  // Upscaling.dll replaces TAA entirely
        bool m_sceneRenderingCompleteThisFrame = false;

        
        // 当前帧的渲染器实例
        std::unique_ptr<SecondPassRenderer> m_currentRenderer;
        
        // 缓存的 D3D 资源指针
        ID3D11DeviceContext* m_cachedContext = nullptr;
        ID3D11Device* m_cachedDevice = nullptr;
        D3DHooks* m_cachedD3DHooks = nullptr;
    };
}
