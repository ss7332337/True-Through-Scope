#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <atomic>
#include <wrl/client.h>

namespace ThroughScope
{
namespace FGCompatibility
{
    // ============================================================================
    // FG (Frame Generation) 兼容层 - 直接 Hook vanilla fo4test (AAAFrameGeneration.dll)
    //
    // 问题：fo4test 在 DrawWorld_Forward::thunk 中复制 RT_29 (Motion Vectors) 和 Depth 到共享缓冲区，
    //       但此时 TrueThroughScope 尚未渲染 Scope 和修改 RT_29。
    //       导致 fo4test 使用的是未修改的 MV，造成 Scope 区域的鬼影。
    //
    // 解决方案（侵入式 Hook + 自定义遮罩处理）：
    // 1. 修改 DrawWorld_Forward::thunk 中的 jz 指令为 jmp，跳过整个 MV/Depth 复制块
    // 2. 创建我们自己的 interpolation mask 纹理（R8_UNORM）
    // 3. 使用我们自己的 ApplyMaskToMVCS 计算着色器处理遮罩
    // 4. 在遮罩区域内将 MV 设为 (0,0)，让 FG 从源帧复制而非插值
    //
    // 关键地址（fo4test 1.3.3 版本）：
    // - DrawWorld_Forward::thunk: RVA 0x6250
    // - d3d12Interop 检查: RVA 0x6274 (cmp byte ptr [rax+3], 0)
    // - jz 跳转指令: RVA 0x6278 (我们把 jz 改成 jmp 跳过整个复制块)
    // - MV CopyResource: RVA 0x6306
    // - Depth CS Dispatch: RVA 0x63DA
    // - Upscaling singleton: RVA 0xcd520
    // - DX12SwapChain singleton: RVA 0xcd440
    // ============================================================================

    // fo4test 版本特定的 RVA 地址
    constexpr uintptr_t RVA_UPSCALING_SINGLETON = 0xcd520;   // Upscaling singleton
    constexpr uintptr_t RVA_DX12SWAPCHAIN_SINGLETON = 0xcd440; // DX12SwapChain singleton

    // Upscaling 类偏移 (IDA 反编译验证)
    constexpr uintptr_t OFFSET_D3D12_INTEROP = 0x03;          // d3d12Interop (bool)
    constexpr uintptr_t OFFSET_DEPTH_BUFFER_SHARED = 0x20;    // depthBufferShared[2] (Texture2D*[2])
    constexpr uintptr_t OFFSET_MOTION_VECTOR_SHARED = 0x30;   // motionVectorBufferShared[2] (Texture2D*[2])
    constexpr uintptr_t OFFSET_COPY_DEPTH_CS = 0x70;          // copyDepthToSharedBufferCS (ID3D11ComputeShader*)
    constexpr uintptr_t OFFSET_SETUP_BUFFERS = 0x80;          // setupBuffers (bool)

    // DX12SwapChain 类偏移
    constexpr uintptr_t OFFSET_SWAPCHAIN_WIDTH = 0x38;        // swapChainDesc.Width
    constexpr uintptr_t OFFSET_SWAPCHAIN_HEIGHT = 0x3C;       // swapChainDesc.Height
    constexpr uintptr_t OFFSET_FRAME_INDEX = 0xB8;            // frameIndex

    // Texture2D 类偏移
    constexpr uintptr_t OFFSET_TEXTURE2D_RESOURCE = 0x30;     // resource.m_ptr (ID3D11Texture2D*)
    constexpr uintptr_t OFFSET_TEXTURE2D_UAV = 0x40;          // uav.m_ptr (ID3D11UnorderedAccessView*)

    // 状态标志
    inline std::atomic<bool> g_Initialized = false;
    inline std::atomic<bool> g_Fo4testDetected = false;
    inline std::atomic<bool> g_HookInstalled = false;
    inline std::atomic<bool> g_MaskResourcesCreated = false;

    // fo4test 模块信息
    inline HMODULE g_Fo4testModule = nullptr;
    inline uintptr_t g_Fo4testBase = 0;

    // Patch 参数 (jz -> jmp)
    constexpr size_t PATCH_SIZE = 6;  // jz near 是 6 字节

    // 保存的原始字节（用于恢复）
    inline BYTE g_OriginalBytes[PATCH_SIZE] = { 0 };

    // 缓存的指针
    inline void* g_UpscalingSingleton = nullptr;

    // 警告输出控制（只输出一次）
    inline bool g_WarnedMVBufferNull = false;
    inline bool g_WarnedSharedBufferNull = false;
    inline bool g_WarnedException = false;

    // ========== 遮罩纹理资源 (我们自己创建，不依赖 fo4test) ==========
    // 双缓冲遮罩纹理，用于双遮罩方法减少边缘伪影
    inline Microsoft::WRL::ComPtr<ID3D11Texture2D> g_InterpolationMask[2];
    inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> g_InterpolationMaskSRV[2];
    inline Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_InterpolationMaskRTV[2];
    
    // ApplyMaskToMVCS 计算着色器（我们自己编译）
    inline Microsoft::WRL::ComPtr<ID3D11ComputeShader> g_ApplyMaskToMVCS;

    // ========== 公共接口 ==========

    /**
     * @brief 初始化 FG 兼容层
     * @param context D3D11 设备上下文
     * @return 初始化是否成功
     *
     * 检测 fo4test，安装 Hook，创建遮罩资源
     */
    bool Initialize(ID3D11DeviceContext* context);

    /**
     * @brief 关闭 FG 兼容层
     *
     * 恢复被 Patch 的字节，释放资源
     */
    void Shutdown();

    /**
     * @brief 创建遮罩纹理资源
     * @param device D3D11 设备
     * @param width 纹理宽度
     * @param height 纹理高度
     * @return 创建是否成功
     */
    bool CreateMaskResources(ID3D11Device* device, uint32_t width, uint32_t height);

    /**
     * @brief 获取当前帧的遮罩 RTV
     * @return 遮罩 RTV，外部渲染白色 (1.0) 标记 scope 区域
     *
     * 返回基于当前帧索引的遮罩 RTV。
     * 调用者应在 scope 区域渲染白色，然后调用 ExecuteMVCopyWithMask。
     */
    ID3D11RenderTargetView* GetMaskRTV();

    /**
     * @brief 清除当前帧遮罩
     * @param context D3D11 设备上下文
     *
     * 每帧开始时调用以清除遮罩
     */
    void ClearMask(ID3D11DeviceContext* context);

    /**
     * @brief 使用遮罩执行 MV 复制 (增强版)
     * @param context D3D11 设备上下文
     *
     * 使用双遮罩方法：
     * 1. 结合当前帧和上一帧遮罩减少边缘伪影
     * 2. 遮罩区域内的 MV 设为 (0,0)，让 FG 从源帧复制
     * 3. 复制处理后的 MV 到 fo4test 的共享缓冲区
     */
    void ExecuteMVCopyWithMask(ID3D11DeviceContext* context);

    /**
     * @brief 手动执行 fo4test 的 MV 复制 (简单版，无遮罩)
     * @param context D3D11 设备上下文
     *
     * 直接复制 RT_29 到共享缓冲区，不处理遮罩。
     * 仅在遮罩不可用时作为回退。
     */
    void ExecuteMVCopy(ID3D11DeviceContext* context);

    /**
     * @brief 检查 FG 兼容层是否活跃
     */
    bool IsActive();

    /**
     * @brief 检查 fo4test 是否已检测
     */
    bool IsFo4testDetected();

    /**
     * @brief 检查遮罩 API 是否可用
     */
    bool IsMaskAPIAvailable();

    // ========== 内部函数 ==========

    /**
     * @brief 获取 DX12SwapChain 的 frameIndex
     */
    int GetFrameIndex();

    /**
     * @brief 编译 ApplyMaskToMVCS 着色器
     * @param device D3D11 设备
     * @return 编译是否成功
     */
    bool CompileApplyMaskShader(ID3D11Device* device);
}
}
