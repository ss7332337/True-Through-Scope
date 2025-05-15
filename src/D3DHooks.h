#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi.h>
#include "RenderUtilities.h"

namespace ThroughScope {
	using namespace DirectX;
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
        static bool Initialize();

        // D3D11 function hooks
        static void WINAPI hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
        
        // Store the original DrawIndexed function
        static LPVOID originalDrawIndexed;

        // Flag to identify our scope quad
        static bool IsScopeQuadBeingDrawn(ID3D11DeviceContext* pContext, UINT IndexCount);
        
        // Store texture resource view for the scope
        static ID3D11ShaderResourceView* s_ScopeTextureView;
		

	private:
		struct BufferInfo
		{
			UINT stride;
			UINT offset;
			D3D11_BUFFER_DESC desc;
		};

		static bool s_isForwardStage;
        
	public:
		static void SetForwardStage(bool isForward) { s_isForwardStage = isForward; }
		static bool GetForwardStage() { return s_isForwardStage; }
    private:
        // Helper methods for texture replacement
        static void SetScopeTexture(ID3D11DeviceContext* pContext);
		static bool IsTargetDrawCall(const BufferInfo& vertexInfo, const BufferInfo& indexInfo, UINT indexCount);
		static UINT GetVertexBuffersInfo(ID3D11DeviceContext* pContext, std::vector<BufferInfo>& outInfos, UINT maxSlotsToCheck = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
		static bool GetIndexBufferInfo(ID3D11DeviceContext* pContext, BufferInfo& outInfo);
    };
}
