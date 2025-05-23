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
			float padding1[2];  // 16字节对齐

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

			XMFLOAT4X4 CameraRotation = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		};

    public:
		static D3DHooks* GetSington();
        bool Initialize();

        // D3D11 function hooks
        static void WINAPI hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
		static HRESULT WINAPI hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
		static LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        static bool IsScopeQuadBeingDrawn(ID3D11DeviceContext* pContext, UINT IndexCount);
        static bool IsScopeQuadBeingDrawnShape(ID3D11DeviceContext* pContext, UINT IndexCount);
		static BOOL __stdcall ClipCursorHook(RECT* lpRect);

        static ID3D11ShaderResourceView* s_ScopeTextureView;

		static HRESULT CreateShaderFromFile(const WCHAR* csoFileNameInOut, const WCHAR* hlslFileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppBlobOut);
		static bool IsEnableRender() { return s_EnableRender; }
		static void SetEnableRender(bool value) { s_EnableRender = value; }

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
		static RSStateCache s_CachedRSState;  // 新增
		static bool s_HasCachedState;

		
		

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
		static void RestoreIAState(ID3D11DeviceContext* pContext);
		static void RestoreVSState(ID3D11DeviceContext* pContext);
		static void RestoreRSState(ID3D11DeviceContext* pContext);
		static void RestoreAllCachedStates(ID3D11DeviceContext* pContext);
    private:
		static bool IsTargetDrawCall(const BufferInfo& vertexInfo, const BufferInfo& indexInfo, UINT indexCount);
		static bool IsTargetDrawCall(std::vector<BufferInfo> vertexInfos, const BufferInfo& indexInfo, UINT indexCount);
		static UINT GetVertexBuffersInfo(ID3D11DeviceContext* pContext, std::vector<BufferInfo>& outInfos, UINT maxSlotsToCheck = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
		static bool GetIndexBufferInfo(ID3D11DeviceContext* pContext, BufferInfo& outInfo);
    };
}
