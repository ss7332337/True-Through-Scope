#pragma once

#include "PCH.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "LightBackupSystem.h"
#include "RenderStateManager.h"


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
        
        // ========== 分阶段渲染 (fo4test 兼容) ==========
        /**
         * 执行瞄具场景渲染（应在 DrawWorld_Forward 之前调用）
         * 这确保瞄具内容被包含在运动矢量生成中
         * @return true 如果场景渲染成功完成
         */
        bool ExecuteSceneRendering();
        
        /**
         * 执行后处理效果（应在 TAA 之后调用）
         * 包括 HDR、热成像等效果
         * @return true 如果后处理成功完成
         */
        bool ExecutePostProcessing();
        
        /**
         * 检查场景渲染是否已完成（用于后处理阶段检查）
         */
        bool IsSceneRenderingComplete() const { return m_sceneRenderingComplete; }

    public:



    private:
        bool BackupFirstPassTextures();
        bool UpdateScopeCamera();
        void ClearRenderTargets();
        bool SyncLighting();
        bool BackupDepthBuffer();  // 备份深度缓冲区用于 MV Mask 深度比较

        /*
         * 执行第二次渲染,生成瞄具内容
         */
        void DrawScopeContent();


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
        
        // ========== 深度备份（用于 MV Mask 深度比较）==========
        ID3D11Texture2D* m_depthBackupTex = nullptr;
        ID3D11ShaderResourceView* m_depthBackupSRV = nullptr;
        bool m_depthBackupCreated = false;

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

        bool m_sceneRenderingComplete = false;  // fo4test 兼容：场景渲染阶段是否完成



        

        


        // ========== 错误处理 ==========
        mutable std::string m_lastError;

		// ========== Motion Vector Mask (fo4test 兼容) ==========
		bool InitializeMotionVectorMask();
		void ShutdownMotionVectorMask();

	public:
		void ApplyMotionVectorMask();  // 在scope区域清零motion vector
		void ApplyGBufferMask();       // 合并第一次和第二次渲染的 GBuffer (RT_20, RT_22)
		
		// Write white pixels to fo4test's MV region override mask texture
		// This marks the scope region so Frame Generation will NOT interpolate those pixels
		void WriteToMVRegionOverrideMask(ID3D11RenderTargetView* maskRTV);

		// MV/GBuffer 调试可视化 - 在屏幕角落显示纹理内容
		static void RenderMVDebugOverlay();
		static bool s_ShowMVDebug;  // 是否显示调试 overlay
		static int s_DebugGBufferIndex;  // 要显示的 GBuffer 索引 (20=Normal, 22=Albedo, 23=Emissive, 24=Material, 29=MotionVector)

	private:
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_mvMaskVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_mvMaskPS;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_mvMaskConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_mvMaskSampler;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_mvMaskBlendState;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_mvMaskDepthState;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_mvMaskRasterizerState;
		bool m_mvMaskInitialized = false;

		// Motion vector mask常量缓冲区结构
		struct MVMaskConstants
		{
			float scopeCenterX;     // 屏幕中心X (0.5)
			float scopeCenterY;     // 屏幕中心Y (0.5)
			float scopeRadius;      // scope半径 (normalized)
			float aspectRatio;      // 宽高比
		};
    };
}
