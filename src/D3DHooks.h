#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include "RenderUtilities.h"
#include <wrl/client.h>

namespace ThroughScope {
	using namespace DirectX;

	struct RSStateCache
	{
		// Rasterizer State
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

		void Clear()
		{
			rasterizerState.Reset();
		}
	};

	struct OMStateCache
	{
		// Render Targets
		static constexpr UINT MAX_RENDER_TARGETS = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViews[MAX_RENDER_TARGETS];
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
		UINT numRenderTargets;

		void Clear()
		{
			for (int i = 0; i < MAX_RENDER_TARGETS; ++i) {
				renderTargetViews[i].Reset();
			}
			depthStencilView.Reset();
			numRenderTargets = 0;
		}
	};

	struct IAStateCache
	{
		// Input Layout
		Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

		// Vertex Buffers
		static constexpr UINT MAX_VERTEX_BUFFERS = 16;
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffers[MAX_VERTEX_BUFFERS];
		UINT strides[MAX_VERTEX_BUFFERS];
		UINT offsets[MAX_VERTEX_BUFFERS];

		// Index Buffer
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
		DXGI_FORMAT indexFormat;
		UINT indexOffset;

		// Primitive Topology
		D3D11_PRIMITIVE_TOPOLOGY topology;

		void Clear()
		{
			inputLayout.Reset();
			for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i) {
				vertexBuffers[i].Reset();
				strides[i] = 0;
				offsets[i] = 0;
			}
			indexBuffer.Reset();
			indexFormat = DXGI_FORMAT_UNKNOWN;
			indexOffset = 0;
			topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		}
	};

	struct VSStateCache
	{
		// Vertex Shader
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;

		// Constant Buffers - 保存原始指针和拷贝的缓冲区
		static constexpr UINT MAX_CONSTANT_BUFFERS = 14;
		Microsoft::WRL::ComPtr<ID3D11Buffer> originalConstantBuffers[MAX_CONSTANT_BUFFERS];
		Microsoft::WRL::ComPtr<ID3D11Buffer> copiedConstantBuffers[MAX_CONSTANT_BUFFERS];

		// Shader Resources
		static constexpr UINT MAX_SHADER_RESOURCES = 128;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResources[MAX_SHADER_RESOURCES];

		// Samplers
		static constexpr UINT MAX_SAMPLERS = 16;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplers[MAX_SAMPLERS];

		void Clear()
		{
			vertexShader.Reset();
			for (int i = 0; i < MAX_CONSTANT_BUFFERS; ++i) {
				originalConstantBuffers[i].Reset();
				copiedConstantBuffers[i].Reset();
			}
			for (int i = 0; i < MAX_SHADER_RESOURCES; ++i) {
				shaderResources[i].Reset();
			}
			for (int i = 0; i < MAX_SAMPLERS; ++i) {
				samplers[i].Reset();
			}
		}
	};


    class D3DHooks 
	{
	private:
		 // 常量缓冲区数据结构
		struct ScopeConstantBuffer
		{
			float screenWidth;
			float screenHeight;
			int enableNightVision;
			int enableThermalVision;

			float cameraPosition[3];
			float padding2;  // 16字节对齐

			float scopePosition[3];
			float padding3;  // 16字节对齐

			float lastCameraPosition[3];
			float padding4;  // 16字节对齐

			float lastScopePosition[3];
			float padding5;  // 16字节对齐

			float parallax_relativeFogRadius;
			float parallax_scopeSwayAmount;
			float parallax_maxTravel;
			float parallax_Radius;

			float reticleScale;    // 瞄准镜缩放
			float reticleOffsetX;  // X轴偏移
			float reticleOffsetY;  // Y轴偏移
			float reticlePadding;  // 16字节对齐

			XMFLOAT4X4 CameraRotation = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

			float nightVisionIntensity;    // 夜视强度
			float nightVisionNoiseScale;   // 噪点缩放
			float nightVisionNoiseAmount;  // 噪点强度
			float nightVisionGreenTint;    // 绿色色调强度

			// 热成像效果参数
			float thermalIntensity;    // 热成像强度
			float thermalThreshold;    // 热成像阈值
			float thermalContrast;     // 热成像对比度
			float thermalNoiseAmount;  // 热成像噪点强度

			// Color Grading LUT权重
			float lutWeights[4];  // 4个LUT的混合权重
		};

    public:
		static D3DHooks* GetSington();
        bool Initialize();
		bool PreInit();

        // D3D11 function hooks
        static void WINAPI hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
		static HRESULT WINAPI hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
		static LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		static void WINAPI hkRSSetViewports(ID3D11DeviceContext* pContext, UINT NumViewports, const D3D11_VIEWPORT* pViewports);

		ID3D11DeviceContext* GetContext();
		ID3D11Device* GetDevice();

        static bool IsScopeQuadBeingDrawn(ID3D11DeviceContext* pContext, UINT IndexCount);
        static bool IsScopeQuadBeingDrawnShape(ID3D11DeviceContext* pContext, UINT IndexCount);
		static BOOL __stdcall ClipCursorHook(RECT* lpRect);

        static ID3D11ShaderResourceView* s_ScopeTextureView;

		static HRESULT CreateShaderFromFile(const WCHAR* csoFileNameInOut, const WCHAR* hlslFileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppBlobOut);
		static bool IsEnableRender() { return s_EnableRender; }
		static void SetEnableRender(bool value) { s_EnableRender = value; }

		static void UpdateScopeParallaxSettings(float relativeFogRadius, float scopeSwayAmount, float maxTravel, float radius);
		static void UpdateNightVisionSettings(float intensity, float noiseScale, float noiseAmount, float greenTint, int enable);
		static void UpdateThermalVisionSettings(float intensity, float threshold, float contrast, float noiseAmount, int enable);
		static void SetReticleScale(float scale)
		{
			s_ReticleScale = std::clamp(scale, 0.1f, 32.0f);
		}

		static void SetReticleOffset(float offsetX, float offsetY)
		{
			s_ReticleOffsetX = std::clamp(offsetX, 0.0f, 1.0f);
			s_ReticleOffsetY = std::clamp(offsetY, 0.0f, 1.0f);
		}

		static void UpdateReticleSettings(float scale, float offsetX, float offsetY)
		{
			SetReticleScale(scale);
			SetReticleOffset(offsetX, offsetY);
		}

		static float GetReticleScale() { return s_ReticleScale; }
		static float GetReticleOffsetX() { return s_ReticleOffsetX; }
		static float GetReticleOffsetY() { return s_ReticleOffsetY; }
		static bool isSelfDrawCall;

		// LUT纹理捕获相关
		static void CaptureLUTTextures(ID3D11DeviceContext* context);
		static ID3D11ShaderResourceView* GetCapturedLUT(int index) { 
			return (index >= 0 && index < 4) ? s_CapturedLUTs[index].Get() : nullptr; 
		}
		static float GetLUTWeight(int index) { 
			return (index >= 0 && index < 4) ? s_LUTWeights[index] : 0.0f; 
		}

		// 全屏三角形渲染相关
		static void RenderImageSpaceEffect(ID3D11DeviceContext* context);
		static bool InitializeImageSpaceEffectResources(ID3D11Device* device);

	private:
		struct BufferInfo
		{
			UINT stride;
			UINT offset;
			D3D11_BUFFER_DESC desc;
		};
		static bool s_EnableRender;
		static bool s_isForwardStage;
		static bool s_isDoZPrePassStage;
		static bool s_isDeferredPrePassStage;
		static Microsoft::WRL::ComPtr<IDXGISwapChain> s_SwapChain;
		static WNDPROC s_OriginalWndProc;
		static HRESULT(WINAPI* s_OriginalPresent)(IDXGISwapChain*, UINT, UINT);
		static HWND s_GameWindow;
		static RECT oldRect;
		static bool s_InPresent;  // 防止递归调用的标志
		static IAStateCache s_CachedIAState;
		static VSStateCache s_CachedVSState;
		static RSStateCache s_CachedRSState;
		static OMStateCache s_CachedOMState;
		static bool s_HasCachedState;
	private:
		static float s_CurrentRelativeFogRadius;
		static float s_CurrentScopeSwayAmount;
		static float s_CurrentMaxTravel;
		static float s_CurrentRadius;
		static float s_ReticleScale;
		static float s_ReticleOffsetX;
		static float s_ReticleOffsetY;

		// 夜视效果参数
		static float s_NightVisionIntensity;
		static float s_NightVisionNoiseScale;
		static float s_NightVisionNoiseAmount;
		static float s_NightVisionGreenTint;
		

		// 热成像效果参数
		static float s_ThermalIntensity;
		static float s_ThermalThreshold;
		static float s_ThermalContrast;
		static float s_ThermalNoiseAmount;
		
	public:
		static int s_EnableThermalVision;
		static int s_EnableNightVision;

	private:
		static bool s_EnableFOVAdjustment;
		static float s_FOVAdjustmentSensitivity;
		static DWORD64 s_LastGamepadInputTime;  // 防止手柄输入过于频繁


	private:
		static Microsoft::WRL::ComPtr<ID3D11Texture2D> s_ReticleTexture;
		static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_ReticleSRV;

		// LUT纹理捕获相关
		static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_CapturedLUTs[4];  // 存储4个LUT纹理
		static float s_LUTWeights[4];  // 存储4个LUT的权重

		// 全屏三角形渲染资源
		static Microsoft::WRL::ComPtr<ID3D11VertexShader> s_ImageSpaceEffectVS;
		static Microsoft::WRL::ComPtr<ID3D11PixelShader> s_ImageSpaceEffectPS;
		static Microsoft::WRL::ComPtr<ID3D11RasterizerState> s_ImageSpaceEffectRS;
		static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> s_ImageSpaceEffectDSS;
		static Microsoft::WRL::ComPtr<ID3D11SamplerState> s_ImageSpaceEffectSamplers[4];
		
		// 全屏三角形输出的RenderTarget
		static Microsoft::WRL::ComPtr<ID3D11Texture2D> s_ImageSpaceEffectOutputTexture;
		static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> s_ImageSpaceEffectOutputRTV;
		static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_ImageSpaceEffectOutputSRV;
		
		// 全屏三角形常量缓冲区
		static Microsoft::WRL::ComPtr<ID3D11Buffer> s_ImageSpaceEffectConstantBuffer;

	public:
		static void SetForwardStage(bool isForward) { s_isForwardStage = isForward; }
		static bool GetForwardStage() { return s_isForwardStage; }
		static void SetDoZPrePassStage(bool isForward) { s_isDoZPrePassStage = isForward; }
		static bool GetDoZPrePassStage() { return s_isDoZPrePassStage; }
		static void SetDeferredPrePassStage(bool isForward) { s_isDeferredPrePassStage = isForward; }
		static bool GetDeferredPrePassStage() { return s_isDeferredPrePassStage; }
        static void SetScopeTexture(ID3D11DeviceContext* pContext);
		// 添加缓存和恢复方法
		static void CacheIAState(ID3D11DeviceContext* pContext);
		static void CacheVSState(ID3D11DeviceContext* pContext);
		static void CacheRSState(ID3D11DeviceContext* pContext);
		static void CacheOMState(ID3D11DeviceContext* pContext);
		static void RestoreIAState(ID3D11DeviceContext* pContext);
		static void RestoreVSState(ID3D11DeviceContext* pContext);
		static void RestoreRSState(ID3D11DeviceContext* pContext);
		static void RestoreOMState(ID3D11DeviceContext* pContext);
		static void CacheAllStates();
		static void RestoreAllCachedStates();
		static bool LoadAimTexture(const std::string& path);
		static ID3D11ShaderResourceView* LoadAimSRV(const std::string& path);
		static void SetSecondRenderTargetAsActive();
    private:
		static bool IsTargetDrawCall(const BufferInfo& vertexInfo, const BufferInfo& indexInfo, UINT indexCount);
		static bool IsTargetDrawCall(std::vector<BufferInfo> vertexInfos, const BufferInfo& indexInfo, UINT indexCount);
		static UINT GetVertexBuffersInfo(ID3D11DeviceContext* pContext, std::vector<BufferInfo>& outInfos, UINT maxSlotsToCheck = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
		static bool GetIndexBufferInfo(ID3D11DeviceContext* pContext, BufferInfo& outInfo);

		static void ProcessGamepadFOVInput();
		static void ProcessMouseWheelFOVInput(short wheelDelta);

		void HookAllContexts();
		void HookContext(ID3D11DeviceContext* context);
		static HRESULT WINAPI D3D11CreateDeviceAndSwapChain_Hook(
			_In_opt_ IDXGIAdapter* pAdapter,
			D3D_DRIVER_TYPE DriverType,
			HMODULE Software,
			UINT Flags,
			_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
			UINT FeatureLevels,
			UINT SDKVersion,
			_In_opt_ CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
			_COM_Outptr_opt_ IDXGISwapChain** ppSwapChain,
			_COM_Outptr_opt_ ID3D11Device** ppDevice,
			_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
			_COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext);
		UINT GetDrawIndexedIndex(ID3D11DeviceContext* context);
	public:
		static void HandleFOVInput();

    };
}
