#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "PCH.h"
#include "GlobalTypes.h"

namespace ThroughScope
{
	// ========== State Cache Structures ==========
	// These define what state is captured and restored

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

		// Constant Buffers
		static constexpr UINT MAX_CONSTANT_BUFFERS = 14;
		Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffers[MAX_CONSTANT_BUFFERS];

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
				constantBuffers[i].Reset();
			}
			for (int i = 0; i < MAX_SHADER_RESOURCES; ++i) {
				shaderResources[i].Reset();
			}
			for (int i = 0; i < MAX_SAMPLERS; ++i) {
				samplers[i].Reset();
			}
		}
	};

	struct RSStateCache
	{
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

		void Clear()
		{
			rasterizerState.Reset();
		}
	};

	struct OMStateCache
	{
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

	// ========== RAII Guard Classes ==========
	// These backup state on construction and restore on destruction

	/**
	 * @brief RAII guard for Input Assembler state
	 * 
	 * Backs up input layout, vertex/index buffers, and primitive topology.
	 * Restores all state when the guard goes out of scope.
	 */
	class ScopedIAState
	{
	public:
		explicit ScopedIAState(ID3D11DeviceContext* context)
			: m_context(context)
		{
			if (!m_context) return;

			// Backup Input Layout
			m_context->IAGetInputLayout(m_state.inputLayout.GetAddressOf());

			// Backup Vertex Buffers
			m_context->IAGetVertexBuffers(0, IAStateCache::MAX_VERTEX_BUFFERS,
				reinterpret_cast<ID3D11Buffer**>(m_state.vertexBuffers),
				m_state.strides,
				m_state.offsets);

			// Backup Index Buffer
			m_context->IAGetIndexBuffer(m_state.indexBuffer.GetAddressOf(),
				&m_state.indexFormat,
				&m_state.indexOffset);

			// Backup Primitive Topology
			m_context->IAGetPrimitiveTopology(&m_state.topology);
		}

		~ScopedIAState()
		{
			if (!m_context) return;

			// Restore Input Layout
			m_context->IASetInputLayout(m_state.inputLayout.Get());

			// Restore Vertex Buffers
			ID3D11Buffer* vertexBuffers[IAStateCache::MAX_VERTEX_BUFFERS];
			for (int i = 0; i < IAStateCache::MAX_VERTEX_BUFFERS; ++i) {
				vertexBuffers[i] = m_state.vertexBuffers[i].Get();
			}
			m_context->IASetVertexBuffers(0, IAStateCache::MAX_VERTEX_BUFFERS,
				vertexBuffers, m_state.strides, m_state.offsets);

			// Restore Index Buffer
			m_context->IASetIndexBuffer(m_state.indexBuffer.Get(),
				m_state.indexFormat, m_state.indexOffset);

			// Restore Primitive Topology
			m_context->IASetPrimitiveTopology(m_state.topology);
		}

		// Non-copyable, non-movable
		ScopedIAState(const ScopedIAState&) = delete;
		ScopedIAState& operator=(const ScopedIAState&) = delete;
		ScopedIAState(ScopedIAState&&) = delete;
		ScopedIAState& operator=(ScopedIAState&&) = delete;

	private:
		ID3D11DeviceContext* m_context;
		IAStateCache m_state{};
	};

	/**
	 * @brief RAII guard for Vertex Shader state
	 * 
	 * Backs up vertex shader, constant buffers, shader resources, and samplers.
	 * Note: This creates copies of constant buffers to preserve their data.
	 */
	class ScopedVSState
	{
	public:
		explicit ScopedVSState(ID3D11DeviceContext* context)
			: m_context(context)
		{
			if (!m_context) return;

			// Get device for buffer copying
			m_context->GetDevice(&m_device);

			// Backup Vertex Shader
			ID3D11ClassInstance* classInstances[256];
			UINT numClassInstances = 256;
			m_context->VSGetShader(m_state.vertexShader.GetAddressOf(),
				classInstances, &numClassInstances);

			// Release class instances
			for (UINT i = 0; i < numClassInstances; ++i) {
				if (classInstances[i]) {
					classInstances[i]->Release();
				}
			}

			// Backup Constant Buffers (just get references, don't copy data)
			m_context->VSGetConstantBuffers(0, VSStateCache::MAX_CONSTANT_BUFFERS,
				reinterpret_cast<ID3D11Buffer**>(m_state.constantBuffers));

			// Backup Shader Resources
			m_context->VSGetShaderResources(0, VSStateCache::MAX_SHADER_RESOURCES,
				reinterpret_cast<ID3D11ShaderResourceView**>(m_state.shaderResources));

			// Backup Samplers
			m_context->VSGetSamplers(0, VSStateCache::MAX_SAMPLERS,
				reinterpret_cast<ID3D11SamplerState**>(m_state.samplers));
		}

		~ScopedVSState()
		{
			if (!m_context) return;

			// Restore Vertex Shader
			m_context->VSSetShader(m_state.vertexShader.Get(), nullptr, 0);

			// Restore Constant Buffers
			ID3D11Buffer* constantBuffers[VSStateCache::MAX_CONSTANT_BUFFERS];
			for (int i = 0; i < VSStateCache::MAX_CONSTANT_BUFFERS; ++i) {
				constantBuffers[i] = m_state.constantBuffers[i].Get();
			}
			m_context->VSSetConstantBuffers(0, VSStateCache::MAX_CONSTANT_BUFFERS, constantBuffers);

			// Restore Shader Resources
			ID3D11ShaderResourceView* shaderResources[VSStateCache::MAX_SHADER_RESOURCES];
			for (int i = 0; i < VSStateCache::MAX_SHADER_RESOURCES; ++i) {
				shaderResources[i] = m_state.shaderResources[i].Get();
			}
			m_context->VSSetShaderResources(0, VSStateCache::MAX_SHADER_RESOURCES, shaderResources);

			// Restore Samplers
			ID3D11SamplerState* samplers[VSStateCache::MAX_SAMPLERS];
			for (int i = 0; i < VSStateCache::MAX_SAMPLERS; ++i) {
				samplers[i] = m_state.samplers[i].Get();
			}
			m_context->VSSetSamplers(0, VSStateCache::MAX_SAMPLERS, samplers);

			if (m_device) {
				m_device->Release();
			}
		}

		// Non-copyable, non-movable
		ScopedVSState(const ScopedVSState&) = delete;
		ScopedVSState& operator=(const ScopedVSState&) = delete;
		ScopedVSState(ScopedVSState&&) = delete;
		ScopedVSState& operator=(ScopedVSState&&) = delete;

	private:
		ID3D11DeviceContext* m_context;
		ID3D11Device* m_device = nullptr;
		VSStateCache m_state{};
	};

	/**
	 * @brief RAII guard for Rasterizer state
	 */
	class ScopedRSState
	{
	public:
		explicit ScopedRSState(ID3D11DeviceContext* context)
			: m_context(context)
		{
			if (!m_context) return;
			m_context->RSGetState(m_state.rasterizerState.GetAddressOf());
		}

		~ScopedRSState()
		{
			if (!m_context) return;
			m_context->RSSetState(m_state.rasterizerState.Get());
		}

		// Non-copyable, non-movable
		ScopedRSState(const ScopedRSState&) = delete;
		ScopedRSState& operator=(const ScopedRSState&) = delete;
		ScopedRSState(ScopedRSState&&) = delete;
		ScopedRSState& operator=(ScopedRSState&&) = delete;

	private:
		ID3D11DeviceContext* m_context;
		RSStateCache m_state{};
	};

	/**
	 * @brief RAII guard for Output Merger state (render targets and depth stencil)
	 */
	class ScopedOMState
	{
	public:
		explicit ScopedOMState(ID3D11DeviceContext* context)
			: m_context(context)
		{
			if (!m_context) return;

			m_context->OMGetRenderTargets(
				OMStateCache::MAX_RENDER_TARGETS,
				reinterpret_cast<ID3D11RenderTargetView**>(m_state.renderTargetViews),
				m_state.depthStencilView.GetAddressOf()
			);

			// Count actual render targets
			m_state.numRenderTargets = 0;
			for (UINT i = 0; i < OMStateCache::MAX_RENDER_TARGETS; ++i) {
				if (m_state.renderTargetViews[i].Get()) {
					m_state.numRenderTargets = i + 1;
				}
			}
		}

		~ScopedOMState()
		{
			if (!m_context) return;

			ID3D11RenderTargetView* renderTargets[OMStateCache::MAX_RENDER_TARGETS];
			for (UINT i = 0; i < OMStateCache::MAX_RENDER_TARGETS; ++i) {
				renderTargets[i] = m_state.renderTargetViews[i].Get();
			}

			m_context->OMSetRenderTargets(
				m_state.numRenderTargets,
				renderTargets,
				m_state.depthStencilView.Get()
			);
		}

		// Non-copyable, non-movable
		ScopedOMState(const ScopedOMState&) = delete;
		ScopedOMState& operator=(const ScopedOMState&) = delete;
		ScopedOMState(ScopedOMState&&) = delete;
		ScopedOMState& operator=(ScopedOMState&&) = delete;

	private:
		ID3D11DeviceContext* m_context;
		OMStateCache m_state{};
	};

	/**
	 * @brief Combined RAII guard for IA + VS + RS states
	 * 
	 * Use this when you need to backup/restore multiple pipeline stages at once.
	 */
	class ScopedPipelineState
	{
	public:
		explicit ScopedPipelineState(ID3D11DeviceContext* context)
			: m_iaState(context)
			, m_vsState(context)
			, m_rsState(context)
		{
		}

		// Non-copyable, non-movable
		ScopedPipelineState(const ScopedPipelineState&) = delete;
		ScopedPipelineState& operator=(const ScopedPipelineState&) = delete;
		ScopedPipelineState(ScopedPipelineState&&) = delete;
		ScopedPipelineState& operator=(ScopedPipelineState&&) = delete;

	private:
		ScopedIAState m_iaState;
		ScopedVSState m_vsState;
		ScopedRSState m_rsState;
	};

	/**
	 * @brief RAII guard for camera pointers (DrawWorld system)
	 * 
	 * Backs up and restores the DrawWorld camera pointers used by the engine.
	 */
	class ScopedCameraBackup
	{
	public:
		ScopedCameraBackup()
		{
			// Backup camera pointers
			m_savedCamera = *ptr_DrawWorldCamera;
			m_savedVisCamera = *ptr_DrawWorldVisCamera;
			m_savedSpCamera = *ptr_DrawWorldSpCamera;
		}

		~ScopedCameraBackup()
		{
			// Restore camera pointers
			*ptr_DrawWorldCamera = m_savedCamera;
			*ptr_DrawWorldVisCamera = m_savedVisCamera;
			*ptr_DrawWorldSpCamera = m_savedSpCamera;
		}

		// Non-copyable, non-movable
		ScopedCameraBackup(const ScopedCameraBackup&) = delete;
		ScopedCameraBackup& operator=(const ScopedCameraBackup&) = delete;
		ScopedCameraBackup(ScopedCameraBackup&&) = delete;
		ScopedCameraBackup& operator=(ScopedCameraBackup&&) = delete;

	private:
		RE::NiCamera* m_savedCamera = nullptr;
		RE::NiCamera* m_savedVisCamera = nullptr;
		RE::NiCamera* m_savedSpCamera = nullptr;
	};
}
