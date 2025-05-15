#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include "RenderUtilities.h"

namespace ThroughScope {
    class D3DHooks {

	private:
		const char* pixelShaderCode = R"(
            Texture2D scopeTexture : register(t0);
            SamplerState scopeSampler : register(s0);
            
            struct PS_INPUT {
                float4 position : SV_POSITION;
                float4 normal : NORMAL;
                float4 color : COLOR0;
                float4 fogColor : COLOR1;
            };
            
            float4 main(PS_INPUT input) : SV_TARGET {
                // 采样纹理 - 使用屏幕空间坐标
                float2 texCoord = input.position.xy / float2(1920, 1080); // 使用屏幕分辨率来归一化
                
                // 对纹理进行采样
                float4 texColor = scopeTexture.Sample(scopeSampler, texCoord);
                texColor.a = 1;
                return texColor;
            }
        )";
    public:
        static bool Initialize();
        static void Shutdown();

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
        static void SaveD3DState(ID3D11DeviceContext* pContext, RenderUtilities::SavedD3DState& state);
        static void RestoreD3DState(ID3D11DeviceContext* pContext, const RenderUtilities::SavedD3DState& state);
		static bool IsTargetDrawCall(const BufferInfo& vertexInfo, const BufferInfo& indexInfo, UINT indexCount);
		static UINT GetVertexBuffersInfo(ID3D11DeviceContext* pContext, std::vector<BufferInfo>& outInfos, UINT maxSlotsToCheck = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
		static bool GetIndexBufferInfo(ID3D11DeviceContext* pContext, BufferInfo& outInfo);
    };
}
