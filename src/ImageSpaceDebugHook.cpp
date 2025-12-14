#include "PCH.h"
#include <d3d9.h>  // for D3DPERF_BeginEvent / D3DPERF_EndEvent
#include "HookManager.h"
#include "HDRStateCache.h"
#include "ScopeCamera.h"
#include "D3DHooks.h"
#include "RE/Bethesda/ImageSpaceEffect.hpp"
#include "RE/Bethesda/ImageSpaceManager.hpp"

namespace ThroughScope
{
    // 原始 ImageSpaceEffectHDR::Render 函数指针
    typedef void (__fastcall *FnImageSpaceEffect_Render)(RE::ImageSpaceEffect* thisPtr, RE::BSTriShape* a_geometry, RE::ImageSpaceEffectParam* a_param);
    
    static FnImageSpaceEffect_Render g_HDR_Render_Original = nullptr;
    static FnImageSpaceEffect_Render g_TAA_Render_Original = nullptr;
    static FnImageSpaceEffect_Render g_DOF_Render_Original = nullptr;
    static FnImageSpaceEffect_Render g_MotionBlur_Render_Original = nullptr;
    static FnImageSpaceEffect_Render g_FullScreenBlur_Render_Original = nullptr;
    static FnImageSpaceEffect_Render g_PipboyScreen_Render_Original = nullptr;

    // Hook 函数 - HDR
    void __fastcall hkImageSpaceEffectHDR_Render(RE::ImageSpaceEffect* thisPtr, RE::BSTriShape* a_geometry, RE::ImageSpaceEffectParam* a_param)
    {
        D3DPERF_BeginEvent(0xFF00FF00, L"ImageSpaceEffect_HDR");

        bool shouldCapture = !ScopeCamera::IsRenderingForScope();

        // 调用原始函数
        g_HDR_Render_Original(thisPtr, a_geometry, a_param);

        // 重要修复：直接在 HDR::Render 完成后捕获状态
        // 之前依赖 DrawIndexed hook 捕获，但 HDR::Render 使用 Draw() 而非 DrawIndexed()
        // 所以从未成功捕获过 HDR 状态，导致闪烁问题
        if (shouldCapture) {
             // 获取 D3D Context
             ID3D11DeviceContext* context = D3DHooks::GetSington()->GetContext();
             if (context) {
                 // 直接捕获当前 HDR 状态（此时 HDR::Render 刚刚完成，所有状态已设置）
                 g_HDRStateCache.Capture(context);
                 // 更新帧标记
                 D3DHooks::s_HDRCapturedFrame = D3DHooks::GetFrameNumber();
             } else {
                 logger::warn("[HDR-DEBUG] hkHDR_Render: Context is null, cannot capture");
             }
        }

        D3DPERF_EndEvent();
    }

    // Hook 函数 - TAA (已在 TAAHook.cpp 中实现，这里只是备用)
    void __fastcall hkImageSpaceEffectTAA_Render_Debug(RE::ImageSpaceEffect* thisPtr, RE::BSTriShape* a_geometry, RE::ImageSpaceEffectParam* a_param)
    {
        D3DPERF_BeginEvent(0xFFFF0000, L"ImageSpaceEffect_TAA");
        g_TAA_Render_Original(thisPtr, a_geometry, a_param);
        D3DPERF_EndEvent();
    }

    // Hook 函数 - DOF
    void __fastcall hkImageSpaceEffectDOF_Render(RE::ImageSpaceEffect* thisPtr, RE::BSTriShape* a_geometry, RE::ImageSpaceEffectParam* a_param)
    {
        D3DPERF_BeginEvent(0xFF0000FF, L"ImageSpaceEffect_DOF");
        g_DOF_Render_Original(thisPtr, a_geometry, a_param);
        D3DPERF_EndEvent();
    }

    // Hook 函数 - MotionBlur
    void __fastcall hkImageSpaceEffectMotionBlur_Render(RE::ImageSpaceEffect* thisPtr, RE::BSTriShape* a_geometry, RE::ImageSpaceEffectParam* a_param)
    {
        D3DPERF_BeginEvent(0xFFFFFF00, L"ImageSpaceEffect_MotionBlur");
        g_MotionBlur_Render_Original(thisPtr, a_geometry, a_param);
        D3DPERF_EndEvent();
    }

    // Hook 函数 - FullScreenBlur
    void __fastcall hkImageSpaceEffectFullScreenBlur_Render(RE::ImageSpaceEffect* thisPtr, RE::BSTriShape* a_geometry, RE::ImageSpaceEffectParam* a_param)
    {
        D3DPERF_BeginEvent(0xFF00FFFF, L"ImageSpaceEffect_FullScreenBlur");
        g_FullScreenBlur_Render_Original(thisPtr, a_geometry, a_param);
        D3DPERF_EndEvent();
    }

    // 注册 ImageSpace 调试 Hook - 使用 VTABLE hook
    void RegisterImageSpaceDebugHooks()
    {
        logger::info("Registering ImageSpace debug hooks via VTABLE...");

        // Hook ImageSpaceEffectHDR::Render (VTABLE index 1)
        {
            REL::Relocation<std::uintptr_t> vtable_HDR(RE::ImageSpaceEffectHDR::VTABLE[0]);
            g_HDR_Render_Original = reinterpret_cast<FnImageSpaceEffect_Render>(vtable_HDR.write_vfunc(1, &hkImageSpaceEffectHDR_Render));
            logger::info("  - Hooked ImageSpaceEffectHDR::Render");
        }

        // Hook ImageSpaceEffectDepthOfField::Render  
        // Note: ImageSpaceEffectDepthOfField 可能没有重写 Render，检查 VTABLE
        // 暂时跳过

        // Hook ImageSpaceEffectFullScreenBlur::Render (VTABLE index 1)
        {
            REL::Relocation<std::uintptr_t> vtable_Blur(RE::ImageSpaceEffectFullScreenBlur::VTABLE[0]);
            g_FullScreenBlur_Render_Original = reinterpret_cast<FnImageSpaceEffect_Render>(vtable_Blur.write_vfunc(1, &hkImageSpaceEffectFullScreenBlur_Render));
            logger::info("  - Hooked ImageSpaceEffectFullScreenBlur::Render");
        }

        // Hook ImageSpaceEffectBokehDepthOfField::Render (VTABLE index 1)
        {
            REL::Relocation<std::uintptr_t> vtable_BokehDOF(RE::ImageSpaceEffectBokehDepthOfField::VTABLE[0]);
            g_DOF_Render_Original = reinterpret_cast<FnImageSpaceEffect_Render>(vtable_BokehDOF.write_vfunc(1, &hkImageSpaceEffectDOF_Render));
            logger::info("  - Hooked ImageSpaceEffectBokehDepthOfField::Render");
        }

        // ImageSpaceEffectTemporalAA::Render 已在 TAAHook.cpp 中 hook，添加调试标记
        // 这里不重复 hook

        logger::info("ImageSpace debug hooks registered successfully");
    }
}

