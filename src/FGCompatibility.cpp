#include "FGCompatibility.h"
#include "RE/Bethesda/BSGraphics.hpp"
#include <d3d9.h>  // for D3DPERF_BeginEvent / D3DPERF_EndEvent
#include <d3dcompiler.h>
#include "D3DHooks.h"
#include "rendering/D3DResourceManager.h"
#include <filesystem>

namespace ThroughScope
{
namespace FGCompatibility
{
    // ============================================================================
    // fo4test 版本特定的 RVA 地址 (版本 1.3.3)
    // 其他偏移定义在 FGCompatibility.h 中
    // ============================================================================

    // DrawWorld_Forward::thunk 中的关键位置
    // 原始代码:
    //   6274  cmp     byte ptr [rax+3], 0      ; 检查 d3d12Interop
    //   6278  jz      loc_6469                 ; 如果为 0，跳到退出
    //
    // 我们要把 jz 改成 jmp，这样无论 d3d12Interop 是什么值都会跳过 MV/Depth 复制
    constexpr uintptr_t RVA_D3D12INTEROP_CHECK = 0x6278;  // jz 指令位置

    // 验证签名：jz near (0F 84 xx xx xx xx)
    static const BYTE EXPECTED_SIGNATURE[] = { 0x0F, 0x84, 0xEB, 0x01, 0x00, 0x00 };

    // 新的指令：jmp near (E9 xx xx xx xx) + NOP (90)
    // jz 原偏移是 0x01EB (相对于下一条指令)
    // jmp 的偏移计算：原偏移 + 1 (因为 jmp 比 jz 少 1 字节)
    static const BYTE JMP_PATCH[] = { 0xE9, 0xEC, 0x01, 0x00, 0x00, 0x90 };

    // ============================================================================
    // 内部辅助函数
    // ============================================================================

    bool CompileApplyMaskShader(ID3D11Device* device)
    {
        if (!device) return false;
        if (g_ApplyMaskToMVCS) return true;  // 已编译

        const wchar_t* shaderPath = L"src\\HLSL\\ApplyMaskToMVCS.hlsl";
        const wchar_t* csoPath = L"Data\\Shaders\\XiFeiLi\\ApplyMaskToMVCS.cso";

        ID3DBlob* shaderBlob = nullptr;
        
        // 使用 D3DResourceManager::CreateShaderFromFile 自动处理 CSO 缓存
        HRESULT hr = D3DResourceManager::GetSingleton()->CreateShaderFromFile(
            csoPath,
            shaderPath,
            "main",
            "cs_5_0",
            &shaderBlob
        );
        
        if (FAILED(hr)) {
            logger::error("[FGCompat] Shader compilation/loading failed with HRESULT: {:X}", hr);
            return false;
        }
        
        hr = device->CreateComputeShader(
            shaderBlob->GetBufferPointer(),
            shaderBlob->GetBufferSize(),
            nullptr,
            g_ApplyMaskToMVCS.ReleaseAndGetAddressOf()
        );
        
        if (shaderBlob) {
            shaderBlob->Release();
        }
        
        if (FAILED(hr)) {
            logger::error("[FGCompat] Failed to create compute shader: {:X}", hr);
            return false;
        }
        
        logger::info("[FGCompat] ApplyMaskToMVCS shader loaded successfully");
        return true;
    }

    bool CreateMaskResources(ID3D11Device* device, uint32_t width, uint32_t height)
    {
        if (!device) return false;
        if (g_MaskResourcesCreated.load()) return true;

        logger::info("[FGCompat] Creating mask resources {}x{}", width, height);

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8_UNORM;  // 单通道遮罩
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8_UNORM;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        for (int i = 0; i < 2; ++i) {
            HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, g_InterpolationMask[i].ReleaseAndGetAddressOf());
            if (FAILED(hr)) {
                logger::error("[FGCompat] Failed to create mask texture[{}]: {:X}", i, hr);
                return false;
            }

            hr = device->CreateShaderResourceView(
                g_InterpolationMask[i].Get(), 
                &srvDesc, 
                g_InterpolationMaskSRV[i].ReleaseAndGetAddressOf()
            );
            if (FAILED(hr)) {
                logger::error("[FGCompat] Failed to create mask SRV[{}]: {:X}", i, hr);
                return false;
            }

            hr = device->CreateRenderTargetView(
                g_InterpolationMask[i].Get(), 
                &rtvDesc, 
                g_InterpolationMaskRTV[i].ReleaseAndGetAddressOf()
            );
            if (FAILED(hr)) {
                logger::error("[FGCompat] Failed to create mask RTV[{}]: {:X}", i, hr);
                return false;
            }
        }

        // 编译 compute shader
        if (!CompileApplyMaskShader(device)) {
            logger::warn("[FGCompat] Failed to compile mask shader, will use simple copy fallback");
        }

        g_MaskResourcesCreated = true;
        logger::info("[FGCompat] Mask resources created successfully");
        return true;
    }

    bool Initialize(ID3D11DeviceContext* context)
    {
        if (g_Initialized.load()) return true;

        // 检查 fo4test 是否加载
        g_Fo4testModule = GetModuleHandleA("AAAFrameGeneration.dll");
        if (!g_Fo4testModule) {
            logger::info("[FGCompat] AAAFrameGeneration.dll not loaded, FG compatibility disabled");
            g_Fo4testDetected = false;
            return false;
        }

        g_Fo4testDetected = true;
        g_Fo4testBase = (uintptr_t)g_Fo4testModule;
        logger::info("[FGCompat] AAAFrameGeneration.dll detected at base {:X}", g_Fo4testBase);

        // 获取 Upscaling singleton 地址
        uintptr_t upscalingSingletonAddr = g_Fo4testBase + RVA_UPSCALING_SINGLETON;
        g_UpscalingSingleton = (void*)upscalingSingletonAddr;
        logger::info("[FGCompat] Upscaling singleton at {:X}", upscalingSingletonAddr);

        // 计算 patch 位置
        uintptr_t patchAddr = g_Fo4testBase + RVA_D3D12INTEROP_CHECK;
        logger::info("[FGCompat] Patch location at {:X}", patchAddr);

        // 验证签名
        bool signatureMatch = true;
        for (size_t i = 0; i < sizeof(EXPECTED_SIGNATURE); i++) {
            if (((BYTE*)patchAddr)[i] != EXPECTED_SIGNATURE[i]) {
                signatureMatch = false;
                break;
            }
        }

        if (!signatureMatch) {
            logger::error("[FGCompat] Signature mismatch at patch location!");
            logger::error("[FGCompat] Expected: 0F 84 EB 01 00 00 (jz near)");
            logger::error("[FGCompat] Found: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
                ((BYTE*)patchAddr)[0],
                ((BYTE*)patchAddr)[1],
                ((BYTE*)patchAddr)[2],
                ((BYTE*)patchAddr)[3],
                ((BYTE*)patchAddr)[4],
                ((BYTE*)patchAddr)[5]);
            logger::error("[FGCompat] This version of fo4test may not be compatible");
            return false;
        }

        logger::info("[FGCompat] Signature verified, installing patch...");

        // 保存原始字节
        memcpy(g_OriginalBytes, (void*)patchAddr, PATCH_SIZE);

        // 修改内存保护
        DWORD oldProtect;
        if (!VirtualProtect((void*)patchAddr, PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            logger::error("[FGCompat] VirtualProtect failed: {}", GetLastError());
            return false;
        }

        // 应用 patch：把 jz 改成 jmp，跳过整个 MV/Depth 复制块
        memcpy((void*)patchAddr, JMP_PATCH, PATCH_SIZE);

        // 恢复内存保护
        DWORD tempProtect;
        VirtualProtect((void*)patchAddr, PATCH_SIZE, oldProtect, &tempProtect);

        g_HookInstalled = true;
        g_Initialized = true;

        logger::info("[FGCompat] Successfully patched fo4test DrawWorld_Forward::thunk");
        logger::info("[FGCompat] Changed: jz -> jmp (skip MV/Depth copy block)");
        logger::info("[FGCompat] TrueThroughScope will now control MV copy timing with mask support");

        // 创建遮罩资源（延迟到需要时创建，因为此时可能还没有尺寸信息）
        if (context) {
            ID3D11Device* device = nullptr;
            context->GetDevice(&device);
            if (device) {
                // 获取交换链尺寸
                uintptr_t dx12SwapChain = g_Fo4testBase + RVA_DX12SWAPCHAIN_SINGLETON;
                int width = *(int*)(dx12SwapChain + OFFSET_SWAPCHAIN_WIDTH);
                int height = *(int*)(dx12SwapChain + OFFSET_SWAPCHAIN_HEIGHT);
                
                if (width > 0 && height > 0) {
                    CreateMaskResources(device, width, height);
                }
                device->Release();
            }
        }

        return true;
    }

    void Shutdown()
    {
        if (!g_Initialized.load()) return;

        if (g_HookInstalled.load() && g_Fo4testBase != 0) {
            uintptr_t patchAddr = g_Fo4testBase + RVA_D3D12INTEROP_CHECK;

            DWORD oldProtect;
            if (VirtualProtect((void*)patchAddr, PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                // 恢复原始字节
                memcpy((void*)patchAddr, g_OriginalBytes, PATCH_SIZE);

                DWORD tempProtect;
                VirtualProtect((void*)patchAddr, PATCH_SIZE, oldProtect, &tempProtect);

                logger::info("[FGCompat] Restored original fo4test code");
            }
        }

        // 释放遮罩资源
        for (int i = 0; i < 2; ++i) {
            g_InterpolationMask[i].Reset();
            g_InterpolationMaskSRV[i].Reset();
            g_InterpolationMaskRTV[i].Reset();
        }
        g_ApplyMaskToMVCS.Reset();
        g_MaskResourcesCreated = false;

        g_Initialized = false;
        g_HookInstalled = false;
        g_Fo4testModule = nullptr;
        g_Fo4testBase = 0;
        g_UpscalingSingleton = nullptr;

        logger::info("[FGCompat] Shutdown complete");
    }

    int GetFrameIndex()
    {
        if (!g_Fo4testBase) return 0;

        // 获取 DX12SwapChain singleton
        uintptr_t dx12SwapChainSingletonAddr = g_Fo4testBase + RVA_DX12SWAPCHAIN_SINGLETON;

        // DX12SwapChain singleton 是一个静态对象，不是指针
        // frameIndex 在偏移 0xB8
        int frameIndex = *(int*)(dx12SwapChainSingletonAddr + OFFSET_FRAME_INDEX);

        // frameIndex 应该是 0 或 1（双缓冲）
        if (frameIndex < 0 || frameIndex > 1) {
            frameIndex = 0;
        }

        return frameIndex;
    }

    ID3D11RenderTargetView* GetMaskRTV()
    {
        if (!g_MaskResourcesCreated.load()) return nullptr;

        int frameIndex = GetFrameIndex();
        return g_InterpolationMaskRTV[frameIndex].Get();
    }

    void ClearMask(ID3D11DeviceContext* context)
    {
        if (!context) return;
        if (!g_MaskResourcesCreated.load()) return;

        int frameIndex = GetFrameIndex();
        
        FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->ClearRenderTargetView(g_InterpolationMaskRTV[frameIndex].Get(), clearColor);
    }

    void ExecuteMVCopyWithMask(ID3D11DeviceContext* context)
    {
        if (!context) return;
        if (!g_Initialized.load()) return;
        if (!g_HookInstalled.load()) return;
        if (!g_UpscalingSingleton) return;

        D3DPERF_BeginEvent(0xFF00FF00, L"[FGCompat] Execute MV Copy With Mask");

        __try {
            // 读取 Upscaling singleton
            uintptr_t upscaling = (uintptr_t)g_UpscalingSingleton;

            // 检查 d3d12Interop 是否启用
            bool d3d12Interop = *(bool*)(upscaling + OFFSET_D3D12_INTEROP);
            if (!d3d12Interop) {
                D3DPERF_EndEvent();
                return;
            }

            // 检查 setupBuffers 是否完成
            bool setupBuffers = *(bool*)(upscaling + OFFSET_SETUP_BUFFERS);
            if (!setupBuffers) {
                D3DPERF_EndEvent();
                return;
            }

            // 获取 frameIndex
            int frameIndex = GetFrameIndex();
            int prevFrameIndex = 1 - frameIndex;

            // 获取 motionVectorBufferShared[frameIndex]
            uintptr_t* motionVectorBufferSharedArray = (uintptr_t*)(upscaling + OFFSET_MOTION_VECTOR_SHARED);
            uintptr_t texture2DPtr = motionVectorBufferSharedArray[frameIndex];

            if (!texture2DPtr) {
                if (!g_WarnedMVBufferNull) {
                    logger::warn("[FGCompat] motionVectorBufferShared[{}] is null", frameIndex);
                    g_WarnedMVBufferNull = true;
                }
                D3DPERF_EndEvent();
                return;
            }

            // 从 fo4test 的 Texture2D 获取资源
            ID3D11Texture2D* sharedBuffer = *(ID3D11Texture2D**)(texture2DPtr + OFFSET_TEXTURE2D_RESOURCE);
            ID3D11UnorderedAccessView* sharedUAV = *(ID3D11UnorderedAccessView**)(texture2DPtr + OFFSET_TEXTURE2D_UAV);

            if (!sharedBuffer) {
                if (!g_WarnedSharedBufferNull) {
                    logger::warn("[FGCompat] sharedBuffer->resource is null");
                    g_WarnedSharedBufferNull = true;
                }
                D3DPERF_EndEvent();
                return;
            }

            // 获取游戏的 RT_29 (Motion Vectors)
            auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
            if (!rendererData) {
                D3DPERF_EndEvent();
                return;
            }

            ID3D11ShaderResourceView* mvSRV = (ID3D11ShaderResourceView*)rendererData->renderTargets[29].srView;
            ID3D11Texture2D* mvTexture = (ID3D11Texture2D*)rendererData->renderTargets[29].texture;
            if (!mvTexture || !mvSRV) {
                D3DPERF_EndEvent();
                return;
            }

            context->OMSetRenderTargets(0, nullptr, nullptr);

            // ========== 使用遮罩处理 MV ==========
            bool useMaskProcessing = g_MaskResourcesCreated.load() && 
                                     g_ApplyMaskToMVCS && 
                                     sharedUAV;

            if (useMaskProcessing) {
                D3DPERF_BeginEvent(0xFFFF00FF, L"Apply Mask To MV");

                // 获取交换链尺寸
                uintptr_t dx12SwapChain = g_Fo4testBase + RVA_DX12SWAPCHAIN_SINGLETON;
                int width = *(int*)(dx12SwapChain + OFFSET_SWAPCHAIN_WIDTH);
                int height = *(int*)(dx12SwapChain + OFFSET_SWAPCHAIN_HEIGHT);

                uint32_t dispatchX = (uint32_t)std::ceil(float(width) / 8.0f);
                uint32_t dispatchY = (uint32_t)std::ceil(float(height) / 8.0f);

                // 设置 SRV: MV input, current mask, previous mask
                ID3D11ShaderResourceView* views[3] = {
                    mvSRV,
                    g_InterpolationMaskSRV[frameIndex].Get(),      // 当前帧遮罩
                    g_InterpolationMaskSRV[prevFrameIndex].Get()   // 上一帧遮罩
                };
                context->CSSetShaderResources(0, 3, views);

                // 设置 UAV: 共享 MV buffer
                ID3D11UnorderedAccessView* uavs[1] = { sharedUAV };
                context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

                // 执行 compute shader
                context->CSSetShader(g_ApplyMaskToMVCS.Get(), nullptr, 0);
                context->Dispatch(dispatchX, dispatchY, 1);

                // 清理
                ID3D11ShaderResourceView* nullViews[3] = { nullptr, nullptr, nullptr };
                context->CSSetShaderResources(0, 3, nullViews);
                ID3D11UnorderedAccessView* nullUavs[1] = { nullptr };
                context->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
                context->CSSetShader(nullptr, nullptr, 0);

                D3DPERF_EndEvent();
            } else {
                // 回退：简单复制
                ID3D11Device* device = nullptr;
                context->GetDevice(&device);
                if (device) {
                    RenderUtilities::SafeCopyTexture(context, device, sharedBuffer, mvTexture);
                    device->Release();
                } else {
                    context->CopyResource(sharedBuffer, mvTexture);
                }
            }

            // ========== 执行 Depth 复制 ==========
            uintptr_t* depthBufferSharedArray = (uintptr_t*)(upscaling + OFFSET_DEPTH_BUFFER_SHARED);
            uintptr_t depthTexture2DPtr = depthBufferSharedArray[frameIndex];

            if (depthTexture2DPtr) {
                ID3D11UnorderedAccessView* depthUAV = *(ID3D11UnorderedAccessView**)(depthTexture2DPtr + OFFSET_TEXTURE2D_UAV);

                if (depthUAV) {
                    ID3D11ComputeShader* copyDepthCS = *(ID3D11ComputeShader**)(upscaling + OFFSET_COPY_DEPTH_CS);

                    if (copyDepthCS) {
                        auto& depth = rendererData->depthStencilTargets[2];  // kMain = 2
                        ID3D11ShaderResourceView* depthSRV = (ID3D11ShaderResourceView*)depth.srViewDepth;

                        if (depthSRV) {
                            uintptr_t dx12SwapChain = g_Fo4testBase + RVA_DX12SWAPCHAIN_SINGLETON;
                            int width = *(int*)(dx12SwapChain + OFFSET_SWAPCHAIN_WIDTH);
                            int height = *(int*)(dx12SwapChain + OFFSET_SWAPCHAIN_HEIGHT);

                            uint32_t dispatchX = (uint32_t)std::ceil(float(width) / 8.0f);
                            uint32_t dispatchY = (uint32_t)std::ceil(float(height) / 8.0f);

                            ID3D11ShaderResourceView* views[1] = { depthSRV };
                            context->CSSetShaderResources(0, 1, views);

                            ID3D11UnorderedAccessView* uavs[1] = { depthUAV };
                            context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

                            context->CSSetShader(copyDepthCS, nullptr, 0);
                            context->Dispatch(dispatchX, dispatchY, 1);

                            ID3D11ShaderResourceView* nullViews[1] = { nullptr };
                            context->CSSetShaderResources(0, 1, nullViews);
                            ID3D11UnorderedAccessView* nullUavs[1] = { nullptr };
                            context->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
                            context->CSSetShader(nullptr, nullptr, 0);
                        }
                    }
                }
            }

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (!g_WarnedException) {
                logger::error("[FGCompat] Exception in ExecuteMVCopyWithMask");
                g_WarnedException = true;
            }
        }

        D3DPERF_EndEvent();
    }

    void ExecuteMVCopy(ID3D11DeviceContext* context)
    {
        // 简单版本（无遮罩），作为回退
        if (!context) return;
        if (!g_Initialized.load()) return;
        if (!g_HookInstalled.load()) return;
        if (!g_UpscalingSingleton) return;

        D3DPERF_BeginEvent(0xFF00FF00, L"[FGCompat] Execute MV Copy (Simple)");

        __try {
            uintptr_t upscaling = (uintptr_t)g_UpscalingSingleton;

            bool d3d12Interop = *(bool*)(upscaling + OFFSET_D3D12_INTEROP);
            if (!d3d12Interop) {
                D3DPERF_EndEvent();
                return;
            }

            bool setupBuffers = *(bool*)(upscaling + OFFSET_SETUP_BUFFERS);
            if (!setupBuffers) {
                D3DPERF_EndEvent();
                return;
            }

            int frameIndex = GetFrameIndex();

            uintptr_t* motionVectorBufferSharedArray = (uintptr_t*)(upscaling + OFFSET_MOTION_VECTOR_SHARED);
            uintptr_t texture2DPtr = motionVectorBufferSharedArray[frameIndex];

            if (!texture2DPtr) {
                D3DPERF_EndEvent();
                return;
            }

            ID3D11Texture2D* sharedBuffer = *(ID3D11Texture2D**)(texture2DPtr + OFFSET_TEXTURE2D_RESOURCE);
            if (!sharedBuffer) {
                D3DPERF_EndEvent();
                return;
            }

            auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
            if (!rendererData) {
                D3DPERF_EndEvent();
                return;
            }

            ID3D11Texture2D* mvTexture = (ID3D11Texture2D*)rendererData->renderTargets[29].texture;
            if (!mvTexture) {
                D3DPERF_EndEvent();
                return;
            }

            context->OMSetRenderTargets(0, nullptr, nullptr);
            
            ID3D11Device* device = nullptr;
            context->GetDevice(&device);
            if (device) {
                RenderUtilities::SafeCopyTexture(context, device, sharedBuffer, mvTexture);
                device->Release();
            } else {
                context->CopyResource(sharedBuffer, mvTexture);
            }

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            logger::error("[FGCompat] Exception in ExecuteMVCopy");
        }

        D3DPERF_EndEvent();
    }

    bool IsActive()
    {
        return g_Initialized.load() && g_HookInstalled.load();
    }

    bool IsFo4testDetected()
    {
        return g_Fo4testDetected.load();
    }

    bool IsMaskAPIAvailable()
    {
        return g_MaskResourcesCreated.load() && g_ApplyMaskToMVCS != nullptr;
    }
}
}
