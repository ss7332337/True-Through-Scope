#include "RenderTargetMerger.h"
#include "RenderUtilities.h"
#include <d3d9.h>  // For D3DPERF markers

namespace ThroughScope
{
	RenderTargetMerger& RenderTargetMerger::GetInstance()
	{
		static RenderTargetMerger instance;
		return instance;
	}

	RenderTargetMerger::~RenderTargetMerger()
	{
		Shutdown();
	}

	bool RenderTargetMerger::Initialize()
	{
		if (m_initialized) {
			return true;
		}

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) {
			logger::error("RenderTargetMerger: RendererData not available");
			return false;
		}

		ID3D11Device* device = (ID3D11Device*)rendererData->device;
		if (!device) {
			logger::error("RenderTargetMerger: D3D11 device not available");
			return false;
		}

		// Initialize configs and backups for each RT
		constexpr int numRTs = sizeof(DEFAULT_MERGE_RTS) / sizeof(DEFAULT_MERGE_RTS[0]);
		m_rtBackups.reserve(numRTs);
		m_configs.reserve(numRTs);

		for (int i = 0; i < numRTs; i++) {
			int rtIndex = DEFAULT_MERGE_RTS[i];
			const char* rtName = RT_NAMES[i];

			// Add config
			RTConfig config;
			config.rtIndex = rtIndex;
			config.name = rtName;
			config.enabled = true;
			m_configs.push_back(config);

			// Create backup
			RTBackup backup;
			backup.rtIndex = rtIndex;
			backup.name = rtName;
			backup.enabled = true;

			if (CreateBackupTexture(backup, device)) {
				m_rtBackups.push_back(backup);
				logger::info("RenderTargetMerger: Created backup for RT_{} ({})", rtIndex, rtName);
			} else {
				logger::warn("RenderTargetMerger: Failed to create backup for RT_{} ({})", rtIndex, rtName);
			}
		}

		m_initialized = true;
		logger::info("RenderTargetMerger: Initialized with {} render targets", m_rtBackups.size());
		return true;
	}

	void RenderTargetMerger::Shutdown()
	{
		for (auto& backup : m_rtBackups) {
			ReleaseBackupTexture(backup);
		}
		m_rtBackups.clear();
		m_configs.clear();
		m_initialized = false;
		logger::info("RenderTargetMerger: Shutdown complete");
	}

	bool RenderTargetMerger::CreateBackupTexture(RTBackup& backup, ID3D11Device* device)
	{
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || backup.rtIndex < 0 || backup.rtIndex >= 101) {
			return false;
		}

		auto& rt = rendererData->renderTargets[backup.rtIndex];
		if (!rt.texture) {
			logger::warn("RenderTargetMerger: RT_{} texture is null", backup.rtIndex);
			return false;
		}

		ID3D11Texture2D* originalTex = (ID3D11Texture2D*)rt.texture;
		D3D11_TEXTURE2D_DESC desc;
		originalTex->GetDesc(&desc);

		// Store original properties
		backup.format = desc.Format;
		backup.width = desc.Width;
		backup.height = desc.Height;

		// Create backup texture (SRV only, no RTV needed)
		D3D11_TEXTURE2D_DESC backupDesc = desc;
		backupDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		backupDesc.Usage = D3D11_USAGE_DEFAULT;
		backupDesc.CPUAccessFlags = 0;
		backupDesc.MiscFlags = 0;

		HRESULT hr = device->CreateTexture2D(&backupDesc, nullptr, &backup.backupTexture);
		if (FAILED(hr)) {
			logger::error("RenderTargetMerger: Failed to create backup texture for RT_{}. HRESULT: 0x{:X}", 
				backup.rtIndex, hr);
			return false;
		}

		// Create SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		hr = device->CreateShaderResourceView(backup.backupTexture, &srvDesc, &backup.backupSRV);
		if (FAILED(hr)) {
			logger::error("RenderTargetMerger: Failed to create backup SRV for RT_{}. HRESULT: 0x{:X}", 
				backup.rtIndex, hr);
			backup.backupTexture->Release();
			backup.backupTexture = nullptr;
			return false;
		}

		return true;
	}

	void RenderTargetMerger::ReleaseBackupTexture(RTBackup& backup)
	{
		if (backup.backupSRV) {
			backup.backupSRV->Release();
			backup.backupSRV = nullptr;
		}
		if (backup.backupTexture) {
			backup.backupTexture->Release();
			backup.backupTexture = nullptr;
		}
	}

	void RenderTargetMerger::SetRTEnabled(int rtIndex, bool enabled)
	{
		for (auto& config : m_configs) {
			if (config.rtIndex == rtIndex) {
				config.enabled = enabled;
				break;
			}
		}
		for (auto& backup : m_rtBackups) {
			if (backup.rtIndex == rtIndex) {
				backup.enabled = enabled;
				break;
			}
		}
	}

	bool RenderTargetMerger::IsRTEnabled(int rtIndex) const
	{
		for (const auto& config : m_configs) {
			if (config.rtIndex == rtIndex) {
				return config.enabled;
			}
		}
		return false;
	}

	int RenderTargetMerger::GetEnabledCount() const
	{
		int count = 0;
		for (const auto& backup : m_rtBackups) {
			if (backup.enabled) count++;
		}
		return count;
	}

	void RenderTargetMerger::BackupRenderTargets(ID3D11DeviceContext* context)
	{
		if (!m_initialized || !context) return;

		D3DPERF_BeginEvent(0xFF00FFFF, L"RenderTargetMerger_BackupAll");

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) {
			D3DPERF_EndEvent();
			return;
		}

		for (auto& backup : m_rtBackups) {
			if (!backup.enabled || !backup.backupTexture) continue;

			auto& rt = rendererData->renderTargets[backup.rtIndex];
			if (rt.texture) {
                // Check for dimension/format mismatch (Dynamic Resolution / DLSS support)
                D3D11_TEXTURE2D_DESC rtDesc;
                ((ID3D11Texture2D*)rt.texture)->GetDesc(&rtDesc);

                if (rtDesc.Width != backup.width || rtDesc.Height != backup.height || rtDesc.Format != backup.format) {
                   logger::info("RenderTargetMerger: resizing backup for RT_{} from {}x{} to {}x{}", 
                       backup.rtIndex, backup.width, backup.height, rtDesc.Width, rtDesc.Height);
                   ReleaseBackupTexture(backup);
                   // CreateBackupTexture will update backup struct with new dimensions from current RT
                   ID3D11Device* device = (ID3D11Device*)rendererData->device;
                   if (!CreateBackupTexture(backup, device)) {
                       logger::warn("RenderTargetMerger: Failed to resize backup for RT_{}", backup.rtIndex);
                       continue;
                   }
                }

                // Use SafeCopyTexture to handle potential dimension mismatches
                ID3D11Device* device = (ID3D11Device*)rendererData->device;
                if (device) {
				    RenderUtilities::SafeCopyTexture(context, device, backup.backupTexture, (ID3D11Texture2D*)rt.texture);
                }
			}
		}

		D3DPERF_EndEvent();
	}

	void RenderTargetMerger::MergeRenderTargets(ID3D11DeviceContext* context, ID3D11Device* device)
	{
		if (!m_initialized || !context || !device) return;

		D3DPERF_BeginEvent(0xFF00FF00, L"RenderTargetMerger_MergeAll");

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) {
			D3DPERF_EndEvent();
			return;
		}

		// Get stencil DSV
		ID3D11DepthStencilView* stencilDSV = nullptr;
		if (rendererData->depthStencilTargets[2].dsView[0]) {
			stencilDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
		}
		if (!stencilDSV) {
			logger::warn("RenderTargetMerger: Stencil DSV not available");
			D3DPERF_EndEvent();
			return;
		}

		// Create stencil test state: pass where stencil != 127 (outside scope)
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> stencilTestDSS;
		D3D11_DEPTH_STENCIL_DESC dssDesc;
		ZeroMemory(&dssDesc, sizeof(dssDesc));
		dssDesc.DepthEnable = FALSE;
		dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dssDesc.StencilEnable = TRUE;
		dssDesc.StencilReadMask = 0xFF;
		dssDesc.StencilWriteMask = 0x00;
		dssDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dssDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dssDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dssDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		dssDesc.BackFace = dssDesc.FrontFace;

		HRESULT hr = device->CreateDepthStencilState(&dssDesc, &stencilTestDSS);
		if (FAILED(hr)) {
			logger::error("RenderTargetMerger: Failed to create stencil test state");
			D3DPERF_EndEvent();
			return;
		}

		// Backup current state
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
		context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs[0].GetAddressOf(), oldDSV.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDSS;
		UINT oldStencilRef;
		context->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldStencilRef);

		D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		context->RSGetViewports(&numViewports, oldViewports);

		Microsoft::WRL::ComPtr<ID3D11RasterizerState> oldRS;
		context->RSGetState(oldRS.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11BlendState> oldBS;
		float oldBlendFactor[4];
		UINT oldSampleMask;
		context->OMGetBlendState(oldBS.GetAddressOf(), oldBlendFactor, &oldSampleMask);

		Microsoft::WRL::ComPtr<ID3D11VertexShader> oldVS;
		context->VSGetShader(oldVS.GetAddressOf(), nullptr, nullptr);
		Microsoft::WRL::ComPtr<ID3D11PixelShader> oldPS;
		context->PSGetShader(oldPS.GetAddressOf(), nullptr, nullptr);
		D3D11_PRIMITIVE_TOPOLOGY oldTopology;
		context->IAGetPrimitiveTopology(&oldTopology);
		Microsoft::WRL::ComPtr<ID3D11InputLayout> oldInputLayout;
		context->IAGetInputLayout(oldInputLayout.GetAddressOf());

		try {
			// Setup common state for all merges
			D3D11_RASTERIZER_DESC rsDesc;
			ZeroMemory(&rsDesc, sizeof(rsDesc));
			rsDesc.FillMode = D3D11_FILL_SOLID;
			rsDesc.CullMode = D3D11_CULL_NONE;
			rsDesc.DepthClipEnable = TRUE;

			Microsoft::WRL::ComPtr<ID3D11RasterizerState> rsState;
			device->CreateRasterizerState(&rsDesc, &rsState);
			context->RSSetState(rsState.Get());

			D3D11_SAMPLER_DESC sampDesc;
			ZeroMemory(&sampDesc, sizeof(sampDesc));
			sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			Microsoft::WRL::ComPtr<ID3D11SamplerState> pointSampler;
			device->CreateSamplerState(&sampDesc, &pointSampler);
			context->PSSetSamplers(0, 1, pointSampler.GetAddressOf());

			context->IASetInputLayout(nullptr);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			D3D11_BLEND_DESC blendDesc;
			ZeroMemory(&blendDesc, sizeof(blendDesc));
			blendDesc.RenderTarget[0].BlendEnable = FALSE;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
			device->CreateBlendState(&blendDesc, &blendState);
			float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			context->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);

			context->VSSetShader(RenderUtilities::GetFullscreenVS(), nullptr, 0);
			context->PSSetShader(RenderUtilities::GetGBufferCopyPS(), nullptr, 0);

			// Merge each enabled RT
			for (auto& backup : m_rtBackups) {
				if (!backup.enabled || !backup.backupSRV) continue;

				MergeSingleRT(backup, context, device, stencilDSV, stencilTestDSS.Get());
			}

		} catch (...) {
			logger::warn("RenderTargetMerger: Exception during merge");
		}

		// Restore state
		context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs[0].GetAddressOf(), oldDSV.Get());
		context->OMSetDepthStencilState(oldDSS.Get(), oldStencilRef);
		context->RSSetState(oldRS.Get());
		context->RSSetViewports(numViewports, oldViewports);
		context->OMSetBlendState(oldBS.Get(), oldBlendFactor, oldSampleMask);
		context->VSSetShader(oldVS.Get(), nullptr, 0);
		context->PSSetShader(oldPS.Get(), nullptr, 0);
		context->IASetPrimitiveTopology(oldTopology);
		context->IASetInputLayout(oldInputLayout.Get());

		D3DPERF_EndEvent();
	}

	void RenderTargetMerger::MergeSingleRT(RTBackup& backup, ID3D11DeviceContext* context,
		ID3D11Device* device, ID3D11DepthStencilView* stencilDSV,
		ID3D11DepthStencilState* stencilTestDSS)
	{
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) return;

		auto& rt = rendererData->renderTargets[backup.rtIndex];
		if (!rt.rtView) return;

		// Create wide string for D3DPERF marker
		wchar_t markerName[64];
		swprintf_s(markerName, L"Merge_RT_%d_%hs", backup.rtIndex, backup.name);
		D3DPERF_BeginEvent(0xFF00FF00, markerName);

		ID3D11RenderTargetView* rtv = (ID3D11RenderTargetView*)rt.rtView;

		// Set viewport to match RT dimensions
		D3D11_VIEWPORT vp;
		vp.Width = (float)backup.width;
		vp.Height = (float)backup.height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		context->RSSetViewports(1, &vp);

		// Check if this is a half-resolution RT (RT_09 SSR or RT_28 SSAO)
		bool isHalfRes = (backup.rtIndex == 9 || backup.rtIndex == 28);
		
		if (isHalfRes && RenderUtilities::GetHalfResMergePS()) {
			// For half-res RTs, use shader-based stencil sampling with UV*2
			// This approach uses discard to keep current content in scope region
			context->OMSetRenderTargets(1, &rtv, nullptr);  // No DSV needed
			context->OMSetDepthStencilState(nullptr, 0);    // Disable stencil test
			
			// Use the HalfResMerge shader
			context->PSSetShader(RenderUtilities::GetHalfResMergePS(), nullptr, 0);
			
			// Create and set resolution constant buffer
			struct HalfResParams {
				float FullResWidth;
				float FullResHeight;
				float Padding[2];
			};
			HalfResParams params = { 1920.0f, 1080.0f, 0.0f, 0.0f };
			
			// Get actual screen resolution
			params.FullResWidth = (float)RenderUtilities::GetScreenWidth();
			params.FullResHeight = (float)RenderUtilities::GetScreenHeight();
			
			Microsoft::WRL::ComPtr<ID3D11Buffer> cbuffer;
			D3D11_BUFFER_DESC cbDesc;
			ZeroMemory(&cbDesc, sizeof(cbDesc));
			cbDesc.ByteWidth = sizeof(HalfResParams);
			cbDesc.Usage = D3D11_USAGE_DEFAULT;
			cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			D3D11_SUBRESOURCE_DATA initData = { &params, 0, 0 };
			device->CreateBuffer(&cbDesc, &initData, &cbuffer);
			context->PSSetConstantBuffers(0, 1, cbuffer.GetAddressOf());
			
			// t0 = backup (first-pass content)
			// t1 = stencil SRV
			ID3D11ShaderResourceView* stencilSRV = RenderUtilities::GetStencilSRV();
			ID3D11ShaderResourceView* srvs[2] = { backup.backupSRV, stencilSRV };
			context->PSSetShaderResources(0, 2, srvs);
			
			// Draw fullscreen triangle
			context->Draw(3, 0);
			
			// Clear SRV bindings
			ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
			context->PSSetShaderResources(0, 2, nullSRVs);
			
			// Restore the copy shader for subsequent full-res RTs
			context->PSSetShader(RenderUtilities::GetGBufferCopyPS(), nullptr, 0);
		} else {
			// For full-res RTs, use hardware stencil test (faster)
			context->OMSetRenderTargets(1, &rtv, stencilDSV);
			context->OMSetDepthStencilState(stencilTestDSS, 127);

			// Bind backup SRV
			context->PSSetShaderResources(0, 1, &backup.backupSRV);

			// Draw fullscreen triangle
			context->Draw(3, 0);

			// Clear SRV binding
			ID3D11ShaderResourceView* nullSRV = nullptr;
			context->PSSetShaderResources(0, 1, &nullSRV);
		}

		D3DPERF_EndEvent();
	}

} // namespace ThroughScope
