#pragma once

#include "PCH.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "LightBackupSystem.h"
#include "RenderStateManager.h"
#include "RenderOptimization.h"

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

        // 性能优化控制（已集成到RenderOptimization）
        RenderOptimization* GetRenderOptimization() const { return m_renderOptimization; }

    private:
        bool BackupFirstPassTextures();
        bool UpdateScopeCamera();
        void ClearRenderTargets();
        bool SyncLighting();

        /*
         * 执行第二次渲染,生成瞄具内容
         */
        void DrawScopeContent();
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
        RenderOptimization* m_renderOptimization;

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

        // ========== 错误处理 ==========
        mutable std::string m_lastError;
    };
}
