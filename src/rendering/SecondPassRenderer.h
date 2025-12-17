#pragma once

#include "PCH.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "LightBackupSystem.h"
#include "RenderStateManager.h"
#include "ScopeHDR.h"
#include "ScopePostProcess.h"
#include "LuminancePass.h"

namespace ThroughScope
{
    /*
     * 负责执行瞄具的第二次渲染过程，包括：
     * - 备份第一次渲染的纹理
     * - 更新瞄具相机配置
     * - 清理渲染目标
     * - 同步光照状态
     * - 执行第二次渲染
     * - 恢复原始状态
     */
    class SecondPassRenderer
    {
    public:
        SecondPassRenderer(ID3D11DeviceContext* context, ID3D11Device* device, D3DHooks* d3dHooks);
        ~SecondPassRenderer();

        bool ExecuteSecondPass(); //主要的入口点
        bool CanExecuteSecondPass() const;

    public:
        // 热成像模式控制
        void SetThermalVisionEnabled(bool enabled) { m_thermalVisionEnabled = enabled; }
        bool IsThermalVisionEnabled() const { return m_thermalVisionEnabled; }


    private:
        bool BackupFirstPassTextures();
        bool UpdateScopeCamera();
        void ClearRenderTargets();
        bool SyncLighting();

        /*
         * 执行第二次渲染,生成瞄具内容
         */
        void DrawScopeContent();
        void ApplyEngineHDREffect();  // 应用引擎的 HDR 后处理（包括 Color Grading）- 已弃用
        void ApplyCustomHDREffect();  // 应用自定义 HDR 后处理（推荐）
        void ApplyThermalVisionEffect(); // 应用热成像效果
        void RestoreFirstPass();
        void CleanupResources();
        bool ValidateD3DResources() const;
        bool CreateTemporaryBackBuffer();
        void ConfigureScopeFrustum(RE::NiCamera* scopeCamera, RE::NiCamera* originalCamera);
        void SyncAccumulatorEyePosition(RE::NiCamera* scopeCamera);

        // ========== D3D资源 ==========
        ID3D11DeviceContext* m_context;
        ID3D11Device* m_device;
        D3DHooks* m_d3dHooks;

        // ========== 渲染资源 ==========
        ID3D11Texture2D* m_tempBackBufferTex = nullptr;
        ID3D11ShaderResourceView* m_tempBackBufferSRV = nullptr;
        ID3D11Texture2D* m_rtTexture2D = nullptr;
        ID3D11RenderTargetView* m_savedRTVs[2] = { nullptr };

        // ========== 渲染目标信息 ==========
        ID3D11RenderTargetView* m_mainRTV = nullptr;
        ID3D11DepthStencilView* m_mainDSV = nullptr;
        ID3D11Texture2D* m_mainRTTexture = nullptr;
        ID3D11Texture2D* m_mainDSTexture = nullptr;

        // ========== 相机状态 ==========
        RE::NiCamera* m_playerCamera = nullptr;
        RE::NiCamera* m_scopeCamera = nullptr;
        RE::NiCamera* m_originalCamera = nullptr;

        // ========== 管理器引用 ==========
        LightBackupSystem* m_lightBackup;
        RenderStateManager* m_renderStateMgr;

        // ========== 状态标志 ==========
        bool m_texturesBackedUp = false;
        bool m_cameraUpdated = false;
        bool m_lightingSynced = false;
        bool m_renderExecuted = false;
        bool m_thermalVisionEnabled = false;


        // ========== 热成像资源 ==========
        class ThermalVision* m_thermalVision = nullptr;
        ID3D11Texture2D* m_thermalRenderTarget = nullptr;
        ID3D11RenderTargetView* m_thermalRTV = nullptr;
        ID3D11ShaderResourceView* m_thermalSRV = nullptr;
        
        // ========== HDR 资源 ==========
        ScopeHDR* m_scopeHDR = nullptr;                   // 自定义 HDR 效果（旧版）
        ScopePostProcess* m_postProcess = nullptr;        // 新版后处理管线
        LuminancePass* m_luminancePass = nullptr;         // 独立亮度计算
        ID3D11Texture2D* m_hdrTempTexture = nullptr;      // t1 替换：Scope 场景纹理
        ID3D11ShaderResourceView* m_hdrTempSRV = nullptr;
        
        // HDR 替换纹理（用于替换 t0, t2, t3）
        ID3D11Texture2D* m_hdrBloomTexture = nullptr;     // t0 替换：黑色 bloom 纹理 (480x270)
        ID3D11ShaderResourceView* m_hdrBloomSRV = nullptr;
        ID3D11Texture2D* m_hdrLuminanceTexture = nullptr; // t2 替换：固定亮度纹理 (1x1)
        ID3D11ShaderResourceView* m_hdrLuminanceSRV = nullptr;
        ID3D11Texture2D* m_hdrMaskTexture = nullptr;      // t3 替换：全零材质遮罩 (1920x1080)
        ID3D11ShaderResourceView* m_hdrMaskSRV = nullptr;
        bool m_hdrDefaultTexturesCreated = false;

        // ========== 错误处理 ==========
        mutable std::string m_lastError;
    };
}
