#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include "RenderUtilities.h"
#include <wrl/client.h>
#include "rendering/D3DResourceManager.h"
#include "rendering/ScopedRenderState.h"

namespace ThroughScope {
	using namespace DirectX;

	// State cache structs are now defined in ScopedRenderState.h
	// VSStateCache with copied constant buffers for D3DHooks special case
	struct VSStateCacheWithCopy : public VSStateCache
	{
		// Additional copied constant buffers for data preservation
		Microsoft::WRL::ComPtr<ID3D11Buffer> copiedConstantBuffers[MAX_CONSTANT_BUFFERS];

		void Clear()
		{
			VSStateCache::Clear();
			for (int i = 0; i < MAX_CONSTANT_BUFFERS; ++i) {
				copiedConstantBuffers[i].Reset();
			}
		}
	};


    class D3DHooks 
	{
	private:
	// 缓存的常量缓冲区数据
		struct CachedScopeConstantBuffer
		{
			float screenWidth = 0;
			float screenHeight = 0;
			int enableNightVision = -1;

			float cameraPosition[3] = {0, 0, 0};
			float scopePosition[3] = {0, 0, 0};
			float reticleScale = 0;
			float reticleOffsetX = 0;
			float reticleOffsetY = 0;
			float sphericalDistortionStrength = 0;
			float sphericalDistortionRadius = 0;
			float sphericalDistortionCenter[2] = {0, 0};
			int enableSphericalDistortion = 0;
			int enableChromaticAberration = 0;
			float brightnessBoost = 1.0f;
			float ambientOffset = 0.0f;
			float parallaxStrength = 0;
			float parallaxSmoothing = 0;
			float exitPupilRadius = 0;
			float exitPupilSoftness = 0;
			float vignetteStrength = 0;
			float vignetteRadius = 0;
			float vignetteSoftness = 0;
			float eyeReliefDistance = 0;
			int enableParallax = 0;
			
			bool NeedsUpdate(const ScopeConstantBuffer& newData) const {
				return screenWidth != newData.screenWidth ||
					   screenHeight != newData.screenHeight ||
					   enableNightVision != newData.enableNightVision ||

					   memcmp(cameraPosition, newData.cameraPosition, sizeof(cameraPosition)) != 0 ||
					   memcmp(scopePosition, newData.scopePosition, sizeof(scopePosition)) != 0 ||
					   reticleScale != newData.reticleScale ||
					   reticleOffsetX != newData.reticleOffsetX ||
					   reticleOffsetY != newData.reticleOffsetY ||
					   sphericalDistortionStrength != newData.sphericalDistortionStrength ||
					   sphericalDistortionRadius != newData.sphericalDistortionRadius ||
					   memcmp(sphericalDistortionCenter, newData.sphericalDistortionCenter, sizeof(sphericalDistortionCenter)) != 0 ||
					   enableSphericalDistortion != newData.enableSphericalDistortion ||
					   enableChromaticAberration != newData.enableChromaticAberration ||
					   brightnessBoost != newData.brightnessBoost ||
					   ambientOffset != newData.ambientOffset ||
					   parallaxStrength != newData.parallaxStrength ||
					   parallaxSmoothing != newData.parallaxSmoothing ||
					   exitPupilRadius != newData.exitPupilRadius ||
					   exitPupilSoftness != newData.exitPupilSoftness ||
					   vignetteStrength != newData.vignetteStrength ||
					   vignetteRadius != newData.vignetteRadius ||
					   vignetteSoftness != newData.vignetteSoftness ||
					   eyeReliefDistance != newData.eyeReliefDistance ||
					   enableParallax != newData.enableParallax;
			}
			
			void UpdateFrom(const ScopeConstantBuffer& newData) {
				screenWidth = newData.screenWidth;
				screenHeight = newData.screenHeight;
				enableNightVision = newData.enableNightVision;

				memcpy(cameraPosition, newData.cameraPosition, sizeof(cameraPosition));
				memcpy(scopePosition, newData.scopePosition, sizeof(scopePosition));
				reticleScale = newData.reticleScale;
				reticleOffsetX = newData.reticleOffsetX;
				reticleOffsetY = newData.reticleOffsetY;
				sphericalDistortionStrength = newData.sphericalDistortionStrength;
				sphericalDistortionRadius = newData.sphericalDistortionRadius;
				memcpy(sphericalDistortionCenter, newData.sphericalDistortionCenter, sizeof(sphericalDistortionCenter));
				enableSphericalDistortion = newData.enableSphericalDistortion;
				enableChromaticAberration = newData.enableChromaticAberration;
				brightnessBoost = newData.brightnessBoost;
				ambientOffset = newData.ambientOffset;
				
				parallaxStrength = newData.parallaxStrength;
				parallaxSmoothing = newData.parallaxSmoothing;
				exitPupilRadius = newData.exitPupilRadius;
				exitPupilSoftness = newData.exitPupilSoftness;
				vignetteStrength = newData.vignetteStrength;
				vignetteRadius = newData.vignetteRadius;
				vignetteSoftness = newData.vignetteSoftness;
				eyeReliefDistance = newData.eyeReliefDistance;
				enableParallax = newData.enableParallax;
			}
		};

    public:
		static D3DHooks* GetSingleton();
		static void CleanupStaticResources(); // 添加静态资源清理函数
		static void NameAllRenderTargets();   // 为RenderDoc调试命名所有101个渲染目标
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

		static bool IsEnableRender() { return s_EnableRender; }
		static void SetEnableRender(bool value) { s_EnableRender = value; }

		static void UpdateScopeParallaxSettings(float parallaxStrength, float exitPupilRadius, float vignetteStrength, float vignetteRadius);
		static void UpdateParallaxAdvancedSettings(float smoothing, float exitPupilSoftness, float vignetteSoftness, float eyeRelief, int enableParallax);
		static void UpdateNightVisionSettings(float intensity, float noiseScale, float noiseAmount, float greenTint);

		static void UpdateSphericalDistortionSettings(float strength, float radius, float centerX, float centerY);
		static void ForceConstantBufferUpdate(); // 强制更新常量缓冲区
		static void SetReticleScale(float scale)
		{
			s_ReticleScale = std::clamp(scale, 0.001f, 32.0f);
		}

		static void SetReticleOffset(float offsetX, float offsetY)
		{
			s_ReticleOffsetX = std::clamp(offsetX, -1.0f, 1.0f);
			s_ReticleOffsetY = std::clamp(offsetY, -1.0f, 1.0f);
		}

		static void UpdateReticleSettings(float scale, float offsetX, float offsetY)
		{
			SetReticleScale(scale);
			SetReticleOffset(offsetX, offsetY);
		}

		static float GetReticleScale() { return s_ReticleScale; }
		static float GetReticleOffsetX() { return s_ReticleOffsetX; }
		static float GetReticleOffsetY() { return s_ReticleOffsetY; }

		// 球形畸变设置的Getter和Setter函数
		static void SetSphericalDistortionStrength(float strength) { s_SphericalDistortionStrength = strength; }
		static void SetSphericalDistortionRadius(float radius) { s_SphericalDistortionRadius = std::clamp(radius, 0.0f, 1.0f); }
		static void SetSphericalDistortionCenter(float centerX, float centerY) { 
			s_SphericalDistortionCenterX = std::clamp(centerX, -0.5f, 0.5f);
			s_SphericalDistortionCenterY = std::clamp(centerY, -0.5f, 0.5f);
		}
		static void SetEnableSphericalDistortion(bool enable) { 
			s_EnableSphericalDistortion = enable ? 1 : 0; 
			s_CachedConstantBufferData.screenWidth = -1.0f; // 强制更新
		}
		static void SetEnableChromaticAberration(bool enable) { 
			s_EnableChromaticAberration = enable ? 1 : 0; 
			s_CachedConstantBufferData.screenWidth = -1.0f; // 强制更新
		}

		static float GetSphericalDistortionStrength() { return s_SphericalDistortionStrength; }
		static float GetSphericalDistortionRadius() { return s_SphericalDistortionRadius; }
		static float GetSphericalDistortionCenterX() { return s_SphericalDistortionCenterX; }
		static float GetSphericalDistortionCenterY() { return s_SphericalDistortionCenterY; }
		static bool GetEnableSphericalDistortion() { return s_EnableSphericalDistortion != 0; }
		static bool GetEnableChromaticAberration() { return s_EnableChromaticAberration != 0; }

		// 视差参数的Getter和Setter函数
		static float GetParallaxStrength() { return s_ParallaxStrength; }
		static float GetParallaxSmoothing() { return s_ParallaxSmoothing; }
		static float GetExitPupilRadius() { return s_ExitPupilRadius; }
		static float GetExitPupilSoftness() { return s_ExitPupilSoftness; }
		static float GetVignetteStrength() { return s_VignetteStrength; }
		static float GetVignetteRadius() { return s_VignetteRadius; }
		static float GetVignetteSoftness() { return s_VignetteSoftness; }
		static float GetEyeReliefDistance() { return s_EyeReliefDistance; }
		static bool  GetEnableParallax() { return s_EnableParallax != 0; }

		static void SetParallaxStrength(float v) { s_ParallaxStrength = std::clamp(v, 0.0f, 0.2f); }
		static void SetParallaxSmoothing(float v) { s_ParallaxSmoothing = std::clamp(v, 0.0f, 1.0f); }
		static void SetExitPupilRadius(float v) { s_ExitPupilRadius = std::clamp(v, 0.2f, 0.8f); }
		static void SetExitPupilSoftness(float v) { s_ExitPupilSoftness = std::clamp(v, 0.05f, 0.5f); }
		static void SetVignetteStrength(float v) { s_VignetteStrength = std::clamp(v, 0.0f, 1.0f); }
		static void SetVignetteRadius(float v) { s_VignetteRadius = std::clamp(v, 0.3f, 1.0f); }
		static void SetVignetteSoftness(float v) { s_VignetteSoftness = std::clamp(v, 0.05f, 0.5f); }
		static void SetEyeReliefDistance(float v) { s_EyeReliefDistance = std::clamp(v, 0.0f, 2.0f); }
		static void SetEnableParallax(bool enable) {
			s_EnableParallax = enable ? 1 : 0;
			s_CachedConstantBufferData.screenWidth = -1.0f; // 强制更新
		}

		static bool isSelfDrawCall;



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
		static VSStateCacheWithCopy s_CachedVSState;
		static RSStateCache s_CachedRSState;
		static OMStateCache s_CachedOMState;
		static bool s_HasCachedState;
		static CachedScopeConstantBuffer s_CachedConstantBufferData; // 缓存的常量缓冲区数据
	private:
		// 新的视差参数
		static float s_ParallaxStrength;
		static float s_ParallaxSmoothing;
		static float s_ExitPupilRadius;
		static float s_ExitPupilSoftness;
		static float s_VignetteStrength;
		static float s_VignetteRadius;
		static float s_VignetteSoftness;
		static float s_EyeReliefDistance;
		static int   s_EnableParallax;
		static float s_ReticleScale;
		static float s_ReticleOffsetX;
		static float s_ReticleOffsetY;

		// 夜视效果参数
		static float s_NightVisionIntensity;
		static float s_NightVisionNoiseScale;
		static float s_NightVisionNoiseAmount;
		static float s_NightVisionGreenTint;
		


		// 球形畸变效果参数
		static float s_SphericalDistortionStrength;
		static float s_SphericalDistortionRadius;
		static float s_SphericalDistortionCenterX;
		static float s_SphericalDistortionCenterY;
		static int s_EnableSphericalDistortion;
		static int s_EnableChromaticAberration;
		
	public:

		static int s_EnableNightVision;
		
		// 夜视和热成像开关的安全setter
		static void SetEnableNightVision(bool enable) { 
			s_EnableNightVision = enable ? 1 : 0; 
			s_CachedConstantBufferData.screenWidth = -1.0f; // 强制更新
		}


	private:
		static bool s_EnableFOVAdjustment;
		static float s_FOVAdjustmentSensitivity;
		static DWORD64 s_LastGamepadInputTime;  // 防止手柄输入过于频繁


	private:



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
		
		static bool s_IsCapturingHDR;
		static uint64_t s_FrameNumber;  // 帧计数器
		static uint64_t s_HDRCapturedFrame;  // HDR 状态捕获的帧号
		
		static uint64_t GetFrameNumber() { return s_FrameNumber; }
		static uint64_t GetHDRCapturedFrame() { return s_HDRCapturedFrame; }
		static bool IsHDRStateCurrentFrame() { return s_HDRCapturedFrame == s_FrameNumber; }

    };
}
