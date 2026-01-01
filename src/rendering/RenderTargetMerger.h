#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <string>

namespace ThroughScope
{
	/**
	 * RenderTargetMerger - Centralized manager for render target backup and merge operations
	 * 
	 * During scope rendering, the second pass overwrites various render targets.
	 * This class backs up the first-pass content and merges it back after scope rendering,
	 * using stencil test to only restore pixels outside the scope region (stencil != 127).
	 * 
	 * Managed Render Targets:
	 * - RT_09: SSR_BlurredExtra (half-res)
	 * - RT_20: GBuffer_Normal
	 * - RT_22: GBuffer_Albedo
	 * - RT_23: GBuffer_Emissive
	 * - RT_24: GBuffer_Material
	 * - RT_28: SSAO (half-res)
	 * - RT_29: MotionVectors
	 * - RT_39: DepthMips
	 * - RT_57: Mask
	 * - RT_58: DeferredDiffuse
	 * - RT_59: DeferredSpecular
	 */
	class RenderTargetMerger
	{
	public:
		static RenderTargetMerger& GetInstance();

		// Lifecycle
		bool Initialize();
		void Shutdown();
		bool IsInitialized() const { return m_initialized; }

		// Configuration
		struct RTConfig
		{
			int rtIndex;
			const char* name;
			bool enabled;
		};
		void SetRTEnabled(int rtIndex, bool enabled);
		bool IsRTEnabled(int rtIndex) const;
		const std::vector<RTConfig>& GetConfiguredRTs() const { return m_configs; }

		// Main operations - called from SecondPassRenderer
		void BackupRenderTargets(ID3D11DeviceContext* context);
		void MergeRenderTargets(ID3D11DeviceContext* context, ID3D11Device* device);

		// Debug
		int GetBackupCount() const { return static_cast<int>(m_rtBackups.size()); }
		int GetEnabledCount() const;

	private:
		RenderTargetMerger() = default;
		~RenderTargetMerger();

		// Non-copyable
		RenderTargetMerger(const RenderTargetMerger&) = delete;
		RenderTargetMerger& operator=(const RenderTargetMerger&) = delete;

		// Internal backup structure
		struct RTBackup
		{
			int rtIndex = -1;
			const char* name = nullptr;
			ID3D11Texture2D* backupTexture = nullptr;
			ID3D11ShaderResourceView* backupSRV = nullptr;
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
			UINT width = 0;
			UINT height = 0;
			bool enabled = true;
		};

		std::vector<RTBackup> m_rtBackups;
		std::vector<RTConfig> m_configs;
		bool m_initialized = false;

		// Default RTs to merge (excludes RT_03 SceneMain)
		static constexpr int DEFAULT_MERGE_RTS[] = {
			9,   // SSR_BlurredExtra (half-res)
			20,  // GBuffer_Normal
			22,  // GBuffer_Albedo
			23,  // GBuffer_Emissive
			24,  // GBuffer_Material
			28,  // SSAO (half-res)
			29,  // MotionVectors
			39,  // DepthMips
			57,  // Mask
			58,  // DeferredDiffuse
			59   // DeferredSpecular
		};

		static constexpr const char* RT_NAMES[] = {
			"SSR_BlurredExtra",  // 9
			"GBuffer_Normal",    // 20
			"GBuffer_Albedo",    // 22
			"GBuffer_Emissive",  // 23
			"GBuffer_Material",  // 24
			"SSAO",              // 28
			"MotionVectors",     // 29
			"DepthMips",         // 39
			"Mask",              // 57
			"DeferredDiffuse",   // 58
			"DeferredSpecular"   // 59
		};

		bool CreateBackupTexture(RTBackup& backup, ID3D11Device* device);
		void ReleaseBackupTexture(RTBackup& backup);
		void MergeSingleRT(RTBackup& backup, ID3D11DeviceContext* context,
			ID3D11Device* device, ID3D11DepthStencilView* stencilDSV,
			ID3D11DepthStencilState* stencilTestDSS);
	};

} // namespace ThroughScope
