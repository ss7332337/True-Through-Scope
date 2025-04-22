#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "renderdoc_app.h"

class Hook
{
	template <class T>
	using ComPtr = REX::W32::ComPtr<T>;

	typedef HRESULT(__stdcall* D3D11PresentHook)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
	typedef void(__stdcall* D3D11DrawIndexedHook)(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);


public:
	Hook(RE::PlayerCamera*);
	~Hook() = default;
	void InitRenderDoc();
	DWORD __stdcall HookDX11_Init();

private:

	static HRESULT __stdcall PresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
	static void __stdcall DrawIndexedHook(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);


	struct OldFuncs
	{
		WNDPROC wndProc;
		D3D11PresentHook phookD3D11Present = nullptr;
		D3D11DrawIndexedHook phookD3D11DrawIndexed = nullptr;
	} oldFuncs;

	ID3D11Buffer* m_mirrorConstantBuffer = nullptr;
	UINT m_cbSize = 0;

	ComPtr<IDXGISwapChain> g_Swapchain = nullptr;
	ComPtr<ID3D11Device> g_Device = nullptr;
	ComPtr<ID3D11DeviceContext> g_Context = nullptr;

	HWND m_hWnd = nullptr;

	DWORD_PTR* pSwapChainVTable = nullptr;
	DWORD_PTR* pDeviceVTable = nullptr;
	DWORD_PTR* pDeviceContextVTable = nullptr;

	RE::PlayerCamera* m_playerCamera = nullptr;

	#pragma region MirrorParm
	// 后视镜相关变量
	//ID3D11Texture2D* pMirrorTexture = nullptr;
	ID3D11Texture2D* pMirrorTexture = nullptr;
	ID3D11Texture2D* pMirrorDepthTexture = nullptr;
	ID3D11RenderTargetView* pMirrorRTV = nullptr;
	ID3D11ShaderResourceView* pMirrorSRV = nullptr;
	ID3D11DepthStencilView* pMirrorDSV = nullptr;
	ID3D11DepthStencilState* pMirrorDepthState = nullptr;
	ID3D11SamplerState* pMirrorSampler = nullptr;
	ID3D11Buffer* mirrorConstantBuffer = nullptr;

	ID3D11Texture2D* pMainDepthTexture = nullptr;
	ID3D11DepthStencilView* pMainDSV = nullptr;
	DXGI_FORMAT mainDepthFormat = DXGI_FORMAT_UNKNOWN;

	std::vector<ID3D11RenderTargetView*> mirrorRTVs;

	ComPtr<ID3D11VertexShader> m_mirrorVS;
	ComPtr<ID3D11PixelShader> m_mirrorPS;
	ComPtr<ID3D11InputLayout> m_mirrorInputLayout;
	// 后视镜尺寸
	const UINT MIRROR_WIDTH = 400;
	const UINT MIRROR_HEIGHT = 300;

	int windowWidth;
	int windowHeight;

	// 后视镜位置和大小
	RECT mirrorRect = { 0, 0, (LONG)MIRROR_WIDTH, (LONG)MIRROR_HEIGHT };
	static bool mirror_initialized;
	static bool mirror_firstClear;

	// 顶点结构
	struct SimpleVertex
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT2 Tex;
	};
	// 顶点着色器常量缓冲区
	struct ConstantBuffer
	{
		DirectX::XMMATRIX mWorld;
		DirectX::XMMATRIX mView;
		DirectX::XMMATRIX mProjection;
	};

	struct HookInfo
	{
		UINT index;
		void* hook;
		void** original;
		const char* name;  // 用于日志
	};

	struct MirrorConstants
	{
		DirectX::XMMATRIX viewMatrix;
		DirectX::XMMATRIX projectionMatrix;
		DirectX::XMFLOAT4 mirrorPlane;  // Define mirror plane for reflection
	};




#pragma endregion
};
