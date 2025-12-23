#include "SecondPassRenderer.h"
#include "GlobalTypes.h"
#include "HookManager.h"
#include "ThermalVision.h"
#include "HDRStateCache.h"
#include "ScopePostProcess.h"
#include "RE/Bethesda/ImageSpaceManager.hpp"
#include <winternl.h>

#include "ScopeRenderingManager.h"

namespace ThroughScope
{
	SecondPassRenderer::SecondPassRenderer(ID3D11DeviceContext* context, ID3D11Device* device, D3DHooks* d3dHooks) 
		: m_context(context)
		, m_device(device)
		, m_d3dHooks(d3dHooks)
		, m_lightBackup(LightBackupSystem::GetSingleton())
		, m_thermalVision(nullptr)
		, m_renderStateMgr(RenderStateManager::GetSingleton())
	{
		if (!ValidateD3DResources()) {
			logger::error("SecondPassRenderer: Invalid D3D resources provided");
		}
	}

	SecondPassRenderer::~SecondPassRenderer()
	{
		CleanupResources();

		// 清理热成像资源
		if (m_thermalRTV) m_thermalRTV->Release();
		if (m_thermalSRV) m_thermalSRV->Release();
		if (m_hdrTempTexture) m_hdrTempTexture->Release();
		if (m_hdrTempSRV) m_hdrTempSRV->Release();
		if (m_thermalRenderTarget) m_thermalRenderTarget->Release();
		if (m_thermalVision) {
			m_thermalVision->Shutdown();
		}
	}

	bool SecondPassRenderer::ExecuteSecondPass()
	{
		if (!CanExecuteSecondPass()) {

			return false;
		}
		
		// 初始化相机指针
		m_scopeCamera = ScopeCamera::GetScopeCamera();
		m_playerCamera = *ptr_DrawWorldCamera;

		// 重置状态标志
		m_texturesBackedUp = false;
		m_cameraUpdated = false;
		m_lightingSynced = false;
		m_renderExecuted = false;

		// 重置每帧的渲染标志
		RenderUtilities::SetFirstPassComplete(false);
		RenderUtilities::SetSecondPassComplete(false);

		try {
			// 1. 备份第一次渲染的纹理
			if (!BackupFirstPassTextures()) {
				logger::error("Failed to backup first pass textures");
				return false;
			}

			// 2. 更新瞄具相机配置
			if (!UpdateScopeCamera()) {
				logger::error("Failed to update scope camera");
				RestoreFirstPass();
				return false;
			}

			// 3. 清理渲染目标
			ClearRenderTargets();

			// 4. 同步光照状态
			if (!SyncLighting()) {
				logger::error("Failed to sync lighting");
				RestoreFirstPass();
				return false;
			}

			// 5. 执行第二次渲染
			DrawScopeContent();
			m_renderExecuted = true;

			// 5.6 应用自定义 HDR 后处理
			/*if (!ScopeRenderingManager::GetSingleton()->IsFO4TestCompatibilityEnabled())
			{
				ApplyCustomHDREffect(); 
			}*/
			
			//ApplyCustomHDREffect(); 

			// 5.6 如果启用热成像，应用热成像效果
			if (m_thermalVisionEnabled) {
				ApplyThermalVisionEffect();
			}

			// 6. 恢复第一次渲染状态
			RestoreFirstPass();
			return true;

		} catch (const std::exception& e) {
			logger::error("Exception during second pass rendering: {}", e.what());
			RestoreFirstPass();
			return false;
		} catch (...) {
			logger::error("Unknown exception during second pass rendering");
			RestoreFirstPass();
			return false;
		}
	}

	bool SecondPassRenderer::CanExecuteSecondPass() const
	{
		// 检查渲染状态
		// 注意：移除了 IsEnableRender 检查，因为它在主线程设置，可能与渲染线程有竞争
		if (!m_renderStateMgr->IsScopeReady() || !m_renderStateMgr->IsRenderReady()) {
			return false;
		}

		// 检查瞄具相机
		auto scopeCamera = ScopeCamera::GetScopeCamera();
		if (!scopeCamera || !scopeCamera->parent) {
			return false;
		}

		// 检查玩家相机
		auto playerCamera = *ptr_DrawWorldCamera;
		if (!playerCamera) {

			return false;
		}

		// 检查D3D资源
		if (!ValidateD3DResources()) {

			return false;
		}
		return true;
	}

	// ========== fo4test 兼容：分阶段渲染 ==========
	
	bool SecondPassRenderer::ExecuteSceneRendering()
	{
		if (!CanExecuteSecondPass()) {
			return false;
		}
		
		// 初始化相机指针
		m_scopeCamera = ScopeCamera::GetScopeCamera();
		m_playerCamera = *ptr_DrawWorldCamera;

		// 重置状态标志
		m_texturesBackedUp = false;
		m_cameraUpdated = false;
		m_lightingSynced = false;
		m_renderExecuted = false;
		m_sceneRenderingComplete = false;

		// 重置每帧的渲染标志
		RenderUtilities::SetFirstPassComplete(false);
		RenderUtilities::SetSecondPassComplete(false);

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || !rendererData->context) {
			logger::error("[FO4Test-Compat] ExecuteSceneRendering: Renderer data is null");
			return false;
		}

		try {
			// 1. 简化的备份 - 在 Forward 阶段不需要获取后缓冲
			// 直接从 rendererData 获取主渲染目标
			m_mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
			m_mainDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
			m_mainRTTexture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
			m_mainDSTexture = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;

			if (!m_mainRTTexture || !m_mainDSTexture) {
				logger::error("[FO4Test-Compat] ExecuteSceneRendering: Main render targets not available");
				return false;
			}

			// 复制主渲染目标到我们的备份纹理
			if (m_mainRTTexture) {
				D3D11_TEXTURE2D_DESC firstPassColorDesc, mainRTDesc;
				RenderUtilities::GetFirstPassColorTexture()->GetDesc(&firstPassColorDesc);
				m_mainRTTexture->GetDesc(&mainRTDesc);

				if (firstPassColorDesc.Width == mainRTDesc.Width && firstPassColorDesc.Height == mainRTDesc.Height) {
					m_context->CopyResource(RenderUtilities::GetFirstPassColorTexture(), m_mainRTTexture);
				} else {

				}
			}

			if (m_mainDSTexture) {
				D3D11_TEXTURE2D_DESC firstPassDepthDesc, mainDSDesc;
				RenderUtilities::GetFirstPassDepthTexture()->GetDesc(&firstPassDepthDesc);
				m_mainDSTexture->GetDesc(&mainDSDesc);

				if (firstPassDepthDesc.Width == mainDSDesc.Width && firstPassDepthDesc.Height == mainDSDesc.Height) {
					m_context->CopyResource(RenderUtilities::GetFirstPassDepthTexture(), m_mainDSTexture);
				} else {

				}
			}

			RenderUtilities::SetFirstPassComplete(true);
			m_texturesBackedUp = true;  // 标记为已备份，但不需要完整的 BackBuffer 恢复

			// 2. 更新瞄具相机配置
			if (!UpdateScopeCamera()) {
				logger::error("[FO4Test-Compat] ExecuteSceneRendering: Failed to update scope camera");
				return false;
			}

			// 3. 清理渲染目标
			ClearRenderTargets();

			// 4. 同步光照状态
			if (!SyncLighting()) {
				logger::error("[FO4Test-Compat] ExecuteSceneRendering: Failed to sync lighting");
				return false;
			}

			// 5. 执行瞄具场景渲染
			D3DPERF_BeginEvent(0xFF00FF00, L"TrueThroughScope_SceneRendering");
			DrawScopeContent();
			D3DPERF_EndEvent();
			
			m_renderExecuted = true;
			m_sceneRenderingComplete = true;
			

			return true;

		} catch (const std::exception& e) {
			logger::error("[FO4Test-Compat] Exception during scene rendering: {}", e.what());
			return false;
		} catch (...) {
			logger::error("[FO4Test-Compat] Unknown exception during scene rendering");
			return false;
		}
	}
	
	bool SecondPassRenderer::ExecutePostProcessing()
	{
		// 检查场景渲染是否完成
		if (!m_sceneRenderingComplete) {

			return false;
		}

		try {
			D3DPERF_BeginEvent(0xFF00FFFF, L"TrueThroughScope_PostProcessing");
			
			// 应用自定义 HDR 后处理
			ApplyCustomHDREffect(); 

			// 如果启用热成像，应用热成像效果
			if (m_thermalVisionEnabled) {
				ApplyThermalVisionEffect();
			}

			// 恢复第一次渲染状态
			RestoreFirstPass();
			
			D3DPERF_EndEvent();
			

			return true;

		} catch (const std::exception& e) {
			logger::error("[FO4Test-Compat] Exception during post processing: {}", e.what());
			RestoreFirstPass();
			return false;
		} catch (...) {
			logger::error("[FO4Test-Compat] Unknown exception during post processing");
			RestoreFirstPass();
			return false;
		}
	}

	bool SecondPassRenderer::BackupFirstPassTextures()
	{
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || !rendererData->context) {
			m_lastError = "Renderer data or context is null";
			return false;
		}

		// 获取主要的渲染目标
		m_mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
		m_mainDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
		m_mainRTTexture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
		m_mainDSTexture = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;

		// 获取当前绑定的渲染目标
		m_context->OMGetRenderTargets(2, m_savedRTVs, nullptr);

		// 获取后缓冲纹理
		// 优先从当前绑定的 RT 获取，如果不可用则使用 renderTargets[0] (SwapChain)
		ID3D11Resource* rtResource = nullptr;
		
		if (m_savedRTVs[1]) {
			// TAA 阶段: RT slot 1 有绑定
			m_savedRTVs[1]->GetResource(&rtResource);
			if (rtResource != nullptr) {
				rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&m_rtTexture2D);
				rtResource->Release();
			}
		}
		
		// 如果从绑定的 RT 获取失败，尝试使用 SwapChain 纹理
		if (!m_rtTexture2D) {
			// PreUI 阶段或其他情况: 直接使用 SwapChain 纹理
			auto swapChainRT = rendererData->renderTargets[0].rtView;
			if (swapChainRT) {
				reinterpret_cast<ID3D11RenderTargetView*>(swapChainRT)->GetResource(&rtResource);
				if (rtResource != nullptr) {
					rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&m_rtTexture2D);
					rtResource->Release();
				}
			}
		}
		
		// 如果还是失败，使用主渲染目标纹理
		if (!m_rtTexture2D) {
			m_rtTexture2D = m_mainRTTexture;
			if (m_rtTexture2D) {
				m_rtTexture2D->AddRef();  // 增加引用计数以匹配后续的 Release
			}
		}

		if (!m_rtTexture2D) {
			m_lastError = "Failed to get render target texture from any source";
			return false;
		}

		// 创建临时后缓冲纹理
		if (!CreateTemporaryBackBuffer()) {
			return false;
		}

		// 验证临时纹理和源纹理的尺寸是否匹配
		D3D11_TEXTURE2D_DESC tempDesc, rtDesc;
		m_tempBackBufferTex->GetDesc(&tempDesc);
		m_rtTexture2D->GetDesc(&rtDesc);

		if (tempDesc.Width != rtDesc.Width || tempDesc.Height != rtDesc.Height || tempDesc.Format != rtDesc.Format) {
			logger::error("Temporary BackBuffer size mismatch: temp({}x{}, {:X}) vs rt({}x{}, {:X})",
				tempDesc.Width, tempDesc.Height, (UINT)tempDesc.Format,
				rtDesc.Width, rtDesc.Height, (UINT)rtDesc.Format);
			m_lastError = "Temporary BackBuffer size mismatch";
			return false;
		}

		// 复制当前渲染目标内容到临时BackBuffer
		m_context->CopyResource(m_tempBackBufferTex, m_rtTexture2D);

		// 复制主渲染目标到我们的纹理
		if (m_mainRTTexture && m_mainDSTexture) {
			// 验证尺寸匹配
			D3D11_TEXTURE2D_DESC firstPassColorDesc, mainRTDesc;
			RenderUtilities::GetFirstPassColorTexture()->GetDesc(&firstPassColorDesc);
			m_mainRTTexture->GetDesc(&mainRTDesc);

			if (firstPassColorDesc.Width == mainRTDesc.Width && firstPassColorDesc.Height == mainRTDesc.Height) {
				m_context->CopyResource(RenderUtilities::GetFirstPassColorTexture(), m_mainRTTexture);
			} else {
				logger::warn("Skipping first pass color copy: size mismatch {}x{} vs {}x{}",
					firstPassColorDesc.Width, firstPassColorDesc.Height,
					mainRTDesc.Width, mainRTDesc.Height);
			}

			// 验证深度纹理尺寸匹配
			D3D11_TEXTURE2D_DESC firstPassDepthDesc, mainDSDesc;
			RenderUtilities::GetFirstPassDepthTexture()->GetDesc(&firstPassDepthDesc);
			m_mainDSTexture->GetDesc(&mainDSDesc);

			if (firstPassDepthDesc.Width == mainDSDesc.Width && firstPassDepthDesc.Height == mainDSDesc.Height) {
				m_context->CopyResource(RenderUtilities::GetFirstPassDepthTexture(), m_mainDSTexture);
			} else {
				logger::warn("Skipping first pass depth copy: size mismatch {}x{} vs {}x{}",
					firstPassDepthDesc.Width, firstPassDepthDesc.Height,
					mainDSDesc.Width, mainDSDesc.Height);
			}

			RenderUtilities::SetFirstPassComplete(true);
		} else {
			m_lastError = "Failed to find valid render target textures";
			return false;
		}

		m_texturesBackedUp = true;
		return true;
	}

	bool SecondPassRenderer::UpdateScopeCamera()
	{
		// 创建原始相机的克隆
		RE::NiCloningProcess tempP{};
		m_originalCamera = (RE::NiCamera*)(m_playerCamera->CreateClone(tempP));
		if (!m_originalCamera) {
			m_lastError = "Failed to clone original camera";
			return false;
		}

		// 更新瞄具相机的位置和旋转
		m_scopeCamera->local.translate = m_originalCamera->local.translate;
		m_scopeCamera->local.rotate = m_originalCamera->local.rotate;
		m_scopeCamera->local.translate.y += 15;

		// 更新父节点
		if (m_scopeCamera->parent) {
			RE::NiUpdateData parentUpdate{};
			parentUpdate.camera = m_scopeCamera;
			m_scopeCamera->parent->Update(parentUpdate);
		}

		// 更新世界变换
		m_scopeCamera->world.translate = m_originalCamera->world.translate;
		m_scopeCamera->world.rotate = m_originalCamera->world.rotate;
		m_scopeCamera->world.scale = m_originalCamera->world.scale;
		m_scopeCamera->world.translate.y += 15;

		m_scopeCamera->UpdateWorldBound();

		// 配置瞄具的视锥体
		ConfigureScopeFrustum(m_scopeCamera, m_originalCamera);

		// 更新相机
		RE::NiUpdateData updateData{};
		updateData.camera = m_scopeCamera;
		m_scopeCamera->Update(updateData);
		m_scopeCamera->UpdateWorldData(&updateData);
		m_scopeCamera->UpdateWorldBound();

		m_cameraUpdated = true;
		return true;
	}

	void SecondPassRenderer::ClearRenderTargets()
	{
		// 清理主输出，准备第二次渲染
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		auto RTVs = rendererData->renderTargets;

		// 清理主要的渲染目标
		for (size_t i = 0; i < 4; i++) {
			if (RTVs[i].rtView) {
				m_context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
			}
		}

		// 清理深度模板缓冲区
		m_context->ClearDepthStencilView(m_mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// ENB 兼容模式：清理所有可能被 ENB 读取的 G-Buffer 通道
		// 这对于防止第一次渲染的武器模型"鬼影"至关重要
		// 扩展清理范围从 (8, 9) 到 (5-19) 以覆盖所有 ENB 可能读取的缓冲区
		static const int allGBuffers[] = { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
		for (int bufIdx : allGBuffers) {
			if (bufIdx < 100 && rendererData->renderTargets[bufIdx].rtView) {
				float clearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				// 对于法线缓冲区 (RT8)，使用默认法线值 (0.5, 0.5, 1.0) 表示向上的法线
				if (bufIdx == 8) {
					clearValue[0] = 0.5f;
					clearValue[1] = 0.5f;
					clearValue[2] = 1.0f;
					clearValue[3] = 1.0f;
				}
				m_context->ClearRenderTargetView(
					(ID3D11RenderTargetView*)rendererData->renderTargets[bufIdx].rtView, 
					clearValue);
			}
		}
	}

	bool SecondPassRenderer::SyncLighting()
	{

		auto pShadowSceneNode = *ptr_DrawWorldShadowNode;
		if (!pShadowSceneNode) {
			m_lastError = "Shadow scene node is null";
			return false;
		}

		// 备份第一次渲染后的光源状态
		m_lightBackup->BackupLightStates(pShadowSceneNode);

		// 设置剔除进程
		if (*DrawWorldCullingProcess) {
			m_lightBackup->SetCullingProcess(*DrawWorldCullingProcess);
		}

		// 应用优化的光源状态用于第二次渲染（使用RenderOptimization设置）
		m_lightBackup->ApplyLightStatesForScope(
			false,
			32);

		// 同步累积器的眼睛位置
		SyncAccumulatorEyePosition(m_scopeCamera);

		m_lightingSynced = true;
		return true;
	}

	void SecondPassRenderer::DrawScopeContent()
	{

		// 设置性能标记
		D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");

		// 配置DrawWorld系统使用瞄具相机
		DrawWorld::SetCamera(m_scopeCamera);
		DrawWorld::SetUpdateCameraFOV(true);
		DrawWorld::SetAdjusted1stPersonFOV(ScopeCamera::GetTargetFOV());
		DrawWorld::SetCameraFov(ScopeCamera::GetTargetFOV());

		// 同步BSShaderManager的相机指针
		*ptr_BSShaderManagerSpCamera = m_scopeCamera;

		// 设置渲染标志
		ScopeCamera::SetRenderingForScope(true);

		// 备份原始相机指针
		auto SpCam = *ptr_DrawWorldSpCamera;
		auto DrawWorldCamera = *ptr_DrawWorldCamera;
		auto DrawWorldVisCamera = *ptr_DrawWorldVisCamera;

		// 获取渲染状态
		auto gState = RE::BSGraphics::State::GetSingleton();

		// 在 SetCameraData 前重新配置视锥体（NiCamera::Update 调用可能重置了 viewFrustum）
		{
			float targetFOV = ScopeCamera::GetTargetFOV();
			float fovRad = targetFOV * 0.01745329251f;  // degrees to radians
			float halfFovTan = tan(fovRad * 0.5f);

			// 从渲染状态获取实际宽高比
			float aspectRatio = 1.7777778f;  // 默认 16:9
			if (gState.backBufferHeight > 0) {
				aspectRatio = static_cast<float>(gState.backBufferWidth) / static_cast<float>(gState.backBufferHeight);
			}

			// 重新设置视锥体参数
			m_scopeCamera->viewFrustum.top = m_scopeCamera->viewFrustum.nearPlane * halfFovTan;
			m_scopeCamera->viewFrustum.bottom = -m_scopeCamera->viewFrustum.top;
			m_scopeCamera->viewFrustum.right = m_scopeCamera->viewFrustum.top * aspectRatio;
			m_scopeCamera->viewFrustum.left = -m_scopeCamera->viewFrustum.right;
		}

		// 设置瞄具相机为当前相机
		*ptr_DrawWorldCamera = m_scopeCamera;
		*ptr_DrawWorldVisCamera = m_scopeCamera;

		// 更新渲染系统的相机数据（视图/投影矩阵）
		// 这对于正确渲染天空盒和其他依赖相机数据的效果至关重要
		gState.SetCameraData(m_scopeCamera, true,
			m_scopeCamera->viewFrustum.nearPlane,
			m_scopeCamera->viewFrustum.farPlane);

		// 保存正确的 viewProjMat 用于 BSSkyShader::SetupGeometry Hook
		{
			static REL::Relocation<uint32_t*> ptr_tls_index{ REL::ID(842564) };
			static REL::Relocation<uintptr_t*> ptr_DefaultContext{ REL::ID(33539) };

			_TEB* teb = NtCurrentTeb();
			auto tls_index = *ptr_tls_index;
			uintptr_t contextPtr = *(uintptr_t*)(*((uint64_t*)teb->Reserved1[11] + tls_index) + 2848i64);
			if (!contextPtr) {
				contextPtr = *ptr_DefaultContext;
			}

			if (contextPtr) {
				auto context = (RE::BSGraphics::Context*)contextPtr;
				auto viewProjMat = context->shadowState.CameraData.viewProjMat;
				g_ScopeViewProjMat[0] = viewProjMat[0];
				g_ScopeViewProjMat[1] = viewProjMat[1];
				g_ScopeViewProjMat[2] = viewProjMat[2];
				g_ScopeViewProjMat[3] = viewProjMat[3];
				g_ScopeViewProjMatValid = true;
			}
		}

		// 更新 PosAdjust 用于天空渲染
		RE::NiPoint3 backupPosAdjust;
		bool posAdjustUpdated = false;

		if (m_scopeCamera) {
			// 获取当前 PosAdjust 用于备份
			static REL::Relocation<uint32_t*> ptr_tls_index{ REL::ID(842564) };
			static REL::Relocation<uintptr_t*> ptr_DefaultContext{ REL::ID(33539) };

			_TEB* teb = NtCurrentTeb();
			auto tls_index = *ptr_tls_index;

			uintptr_t contextPtr = *(uintptr_t*)(*((uint64_t*)teb->Reserved1[11] + tls_index) + 2848i64);
			if (!contextPtr) {
				contextPtr = *ptr_DefaultContext;
			}

			if (contextPtr) {
				auto context = (RE::BSGraphics::Context*)contextPtr;
				backupPosAdjust = context->shadowState.PosAdjust;

				// 设置 PosAdjust 为瞄具相机的世界位置
				RE::NiPoint3 scopePosAdjust;
				scopePosAdjust.x = m_scopeCamera->world.translate.x;
				scopePosAdjust.y = m_scopeCamera->world.translate.y;
				scopePosAdjust.z = m_scopeCamera->world.translate.z;

				RE::BSGraphics::Renderer::GetSingleton().SetPosAdjust(&scopePosAdjust);
				posAdjustUpdated = true;
			}

			// 执行第二次渲染
			auto hookMgr = HookManager::GetSingleton();
			
			// TODO: BSMTAManager 竞态条件问题待解决
			// 禁用 BSMTAManager 会导致阴影渲染崩溃，需要找其他方案
			
			hookMgr->g_RenderPreUIOriginal(savedDrawWorld);

			// 恢复 PosAdjust
			if (posAdjustUpdated) {
				RE::BSGraphics::Renderer::GetSingleton().SetPosAdjust(&backupPosAdjust);
			}

			// 恢复相机指针
			*ptr_DrawWorldCamera = DrawWorldCamera;
			*ptr_DrawWorldVisCamera = DrawWorldVisCamera;

			// 清除保存的矩阵有效性标志
			g_ScopeViewProjMatValid = false;

			// 清除渲染标志
			ScopeCamera::SetRenderingForScope(false);

			D3DPERF_EndEvent();
		}
	}

	void SecondPassRenderer::ApplyThermalVisionEffect()
	{


		// 初始化热成像系统（如果尚未初始化）
		if (!m_thermalVision) {
			m_thermalVision = ThermalVision::GetSingleton();
			if (!m_thermalVision->Initialize(m_device, m_context)) {
				logger::error("Failed to initialize thermal vision system");
				return;
			}
		}

		// 创建热成像渲染目标（如果尚未创建）
		if (!m_thermalRenderTarget) {
			// 获取当前渲染目标的描述
			D3D11_TEXTURE2D_DESC desc;
			m_mainRTTexture->GetDesc(&desc);

			// 创建热成像渲染目标
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_thermalRenderTarget);
			if (FAILED(hr)) {
				logger::error("Failed to create thermal render target");
				return;
			}

			// 创建RTV
			hr = m_device->CreateRenderTargetView(m_thermalRenderTarget, nullptr, &m_thermalRTV);
			if (FAILED(hr)) {
				logger::error("Failed to create thermal RTV");
				return;
			}

			// 创建SRV
			hr = m_device->CreateShaderResourceView(m_thermalRenderTarget, nullptr, &m_thermalSRV);
			if (FAILED(hr)) {
				logger::error("Failed to create thermal SRV");
				return;
			}
		}

		// 获取深度缓冲的SRV（用于温度估算）
		ID3D11ShaderResourceView* depthSRV = nullptr;
		if (m_mainDSTexture) {
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			ID3D11ShaderResourceView* tempDepthSRV = nullptr;
			HRESULT hr = m_device->CreateShaderResourceView(m_mainDSTexture, &srvDesc, &tempDepthSRV);
			if (SUCCEEDED(hr)) {
				depthSRV = tempDepthSRV;
			}
		}

		// 应用热成像效果
		// 将第二次渲染的结果作为源，处理后输出到热成像渲染目标
		m_thermalVision->ApplyThermalEffect(m_mainRTV, m_thermalRTV, depthSRV);

		// 将热成像结果复制回主渲染目标
		m_context->CopyResource(m_mainRTTexture, m_thermalRenderTarget);

		// 清理临时深度SRV
		if (depthSRV) {
			depthSRV->Release();
		}


	}

	void SecondPassRenderer::ApplyEngineHDREffect()
	{
		D3DPERF_BeginEvent(0xFF00FF00, L"SecondPass_EngineHDR");

		// 检查 HDR 状态缓存是否有效
		if (!g_HDRStateCache.IsValid()) {
			logger::warn("ApplyEngineHDREffect: HDR state cache is not valid, skipping");
			D3DPERF_EndEvent();
			return;
		}

		// 获取 ImageSpaceManager 单例来获取屏幕三角形
		auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
		if (!imageSpaceManager) {
			logger::warn("ApplyEngineHDREffect: ImageSpaceManager is null");
			D3DPERF_EndEvent();
			return;
		}

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) {
			logger::warn("ApplyEngineHDREffect: RendererData is null");
			D3DPERF_EndEvent();
			return;
		}

		try {
			// 备份当前完整渲染状态
			HDRStateCache stateBackup;
            stateBackup.Capture(m_context);
            
            // 检查 HDR 状态是否已捕获，如果没有则跳过
            if (!g_HDRStateCache.IsValid()) {

                stateBackup.Apply(m_context);
                D3DPERF_EndEvent();
                return;
            }
            
            // 检查 HDR 状态是否是当前帧捕获的（关键！防止使用过时状态）
            if (!D3DHooks::IsHDRStateCurrentFrame()) {

                stateBackup.Apply(m_context);
                D3DPERF_EndEvent();
                return;
            }
            
            // 获取主渲染目标纹理
            ID3D11Texture2D* mainRtTex = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
            if (!mainRtTex) {
                logger::warn("ApplyEngineHDREffect: MainRT texture is null");
                stateBackup.Apply(m_context); // 恢复状态
                D3DPERF_EndEvent();
                return;
            }

            // 获取其描述
            D3D11_TEXTURE2D_DESC mainDesc;
            mainRtTex->GetDesc(&mainDesc);

            // 检查并创建/重建临时纹理
            bool needCreate = false;
            if (!m_hdrTempTexture) {
                needCreate = true;
            } else {
                D3D11_TEXTURE2D_DESC tempDesc;
                m_hdrTempTexture->GetDesc(&tempDesc);
                if (tempDesc.Width != mainDesc.Width || tempDesc.Height != mainDesc.Height || tempDesc.Format != mainDesc.Format) {
                    m_hdrTempTexture->Release();
                    m_hdrTempTexture = nullptr;
                    if (m_hdrTempSRV) {
                        m_hdrTempSRV->Release();
                        m_hdrTempSRV = nullptr;
                    }
                    needCreate = true;
                }
            }

            if (needCreate) {
                D3D11_TEXTURE2D_DESC desc = mainDesc;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // 只需要作为 SRV
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.CPUAccessFlags = 0;
                desc.MiscFlags = 0;

                HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_hdrTempTexture);
                if (FAILED(hr)) {
                    logger::error("ApplyEngineHDREffect: Failed to create temp texture");
                    stateBackup.Apply(m_context); // 恢复状态
                    D3DPERF_EndEvent();
                    return;
                }

                hr = m_device->CreateShaderResourceView(m_hdrTempTexture, nullptr, &m_hdrTempSRV);
                if (FAILED(hr)) {
                    logger::error("ApplyEngineHDREffect: Failed to create temp SRV");
                    m_hdrTempTexture->Release();
                    m_hdrTempTexture = nullptr;
                    stateBackup.Apply(m_context); // 恢复状态
                    D3DPERF_EndEvent();
                    return;
                }
                // logger::debug("ApplyEngineHDREffect: Created temp texture {}x{}", desc.Width, desc.Height);
            }

            // 复制 MainRT 到 临时纹理
            m_context->CopyResource(m_hdrTempTexture, mainRtTex);
			
			// 应用捕获的 HDR 状态（shader、常量缓冲区等）
			g_HDRStateCache.Apply(m_context);
			
			// 关键：先清除所有 PS SRV 绑定，避免与 RTV 冲突
			ID3D11ShaderResourceView* nullSRVs[16] = {};
			m_context->PSSetShaderResources(0, 16, nullSRVs);
			
			// 设置输出渲染目标（MainRT 4）
			// 必须在设置 SRV 之前设置 RTV，否则 MainRT 同时作为 SRV 和 RTV 会冲突
			ID3D11RenderTargetView* mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
			if (mainRTV) {
				m_context->OMSetRenderTargets(1, &mainRTV, nullptr);
			}
			
			// 现在安全地设置 SRV
			// t0 = Bloom - 使用复制的纹理（避免开枪时内容被修改）
			// t1 = Scene - 使用临时纹理避免读写冲突
			// t2 = Luminance - 使用复制的纹理（避免开枪时内容被修改）
			// t3 = Mask - 保留捕获的原始 SRV（mask可能依赖场景内容）

			// 使用复制的纹理SRV，如果复制失败则回退到原始SRV
			ID3D11ShaderResourceView* srv0 = g_HDRStateCache.bloomSRVCopy.Get();
			if (!srv0) {
				srv0 = g_HDRStateCache.psSRVs[0].Get();  // 回退到原始SRV

			}
			ID3D11ShaderResourceView* srv2 = g_HDRStateCache.luminanceSRVCopy.Get();
			if (!srv2) {
				srv2 = g_HDRStateCache.psSRVs[2].Get();  // 回退到原始SRV

			}
			ID3D11ShaderResourceView* srv3 = g_HDRStateCache.psSRVs[3].Get();  // Mask保持原始

			if (srv0) {
				m_context->PSSetShaderResources(0, 1, &srv0);  // t0: Bloom (复制)
			}
			if (m_hdrTempSRV) {
				m_context->PSSetShaderResources(1, 1, &m_hdrTempSRV);  // t1: Scene (临时纹理，避免冲突)
			}
			if (srv2) {
				m_context->PSSetShaderResources(2, 1, &srv2);  // t2: Luminance (复制)
			}
			if (srv3) {
				m_context->PSSetShaderResources(3, 1, &srv3);  // t3: Mask (原始)
			}
			
			// 设置正确的 viewport
			auto gState = RE::BSGraphics::State::GetSingleton();
			D3D11_VIEWPORT vp = {};
			vp.Width = static_cast<float>(gState.backBufferWidth);
			vp.Height = static_cast<float>(gState.backBufferHeight);
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0.0f;
			vp.TopLeftY = 0.0f;
			m_context->RSSetViewports(1, &vp);
			
			// 绘制全屏三角形
			m_context->Draw(3, 0);

			// 清除所有 SRV 绑定以避免资源冲突
			ID3D11ShaderResourceView* nullSRVsCleanup[4] = { nullptr, nullptr, nullptr, nullptr };
			m_context->PSSetShaderResources(0, 4, nullSRVsCleanup);
			
			// 清除 RTV 绑定，防止后续 CopyResource 出错
			ID3D11RenderTargetView* nullRTV = nullptr;
			m_context->OMSetRenderTargets(1, &nullRTV, nullptr);

			// 恢复完整渲染状态
            stateBackup.Apply(m_context);
			stateBackup.Clear();

		} catch (...) {
			logger::error("ApplyEngineHDREffect: Exception during HDR effect rendering");
		}

		D3DPERF_EndEvent();
	}

	void SecondPassRenderer::ApplyCustomHDREffect()
	{
		D3DPERF_BeginEvent(0xFF00FF00, L"SecondPass_CustomHDR");

		// 配置: 是否使用新的完整后处理管线 (Bloom + DOF + HDR)
		// 设置为 true 使用新管线，false 使用旧的 ScopeHDR
		static bool useNewPostProcessPipeline = true;

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) {
			logger::warn("ApplyCustomHDREffect: RendererData is null");
			D3DPERF_EndEvent();
			return;
		}

		try {
			// 输入: 从 rt4 读取当前瞄具场景
			ID3D11Texture2D* mainRtTex = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
			// 输出: 写入 rt4 (RestoreFirstPass 从 rt4 复制到 SecondPassColorTexture)
			ID3D11RenderTargetView* outputRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;

			if (!mainRtTex || !outputRTV) {
				logger::warn("ApplyCustomHDREffect: MainRT texture or output RTV is null");
				D3DPERF_EndEvent();
				return;
			}

			// 获取纹理描述
			D3D11_TEXTURE2D_DESC mainDesc;
			mainRtTex->GetDesc(&mainDesc);

			// 创建或重建临时纹理（用于保存场景内容，避免读写冲突）
			bool needCreate = false;
			if (!m_hdrTempTexture) {
				needCreate = true;
			} else {
				D3D11_TEXTURE2D_DESC tempDesc;
				m_hdrTempTexture->GetDesc(&tempDesc);
				if (tempDesc.Width != mainDesc.Width || tempDesc.Height != mainDesc.Height || tempDesc.Format != mainDesc.Format) {
					if (m_hdrTempTexture) m_hdrTempTexture->Release();
					if (m_hdrTempSRV) m_hdrTempSRV->Release();
					m_hdrTempTexture = nullptr;
					m_hdrTempSRV = nullptr;
					needCreate = true;
				}
			}

			if (needCreate) {
				D3D11_TEXTURE2D_DESC desc = mainDesc;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_hdrTempTexture);
				if (FAILED(hr)) {
					logger::error("ApplyCustomHDREffect: Failed to create temp texture");
					D3DPERF_EndEvent();
					return;
				}

				hr = m_device->CreateShaderResourceView(m_hdrTempTexture, nullptr, &m_hdrTempSRV);
				if (FAILED(hr)) {
					logger::error("ApplyCustomHDREffect: Failed to create temp SRV");
					m_hdrTempTexture->Release();
					m_hdrTempTexture = nullptr;
					D3DPERF_EndEvent();
					return;
				}
			}

			// 复制当前场景到临时纹理
			m_context->CopyResource(m_hdrTempTexture, mainRtTex);

			// 清除 RTV 绑定，确保 CopyResource 完成
			ID3D11RenderTargetView* nullRTV = nullptr;
			m_context->OMSetRenderTargets(1, &nullRTV, nullptr);

			// 获取纹理资源
			ID3D11ShaderResourceView* bloomSRV = nullptr;
			ID3D11ShaderResourceView* luminanceSRV = nullptr;
			ID3D11ShaderResourceView* depthSRV = nullptr;
			ID3D11ShaderResourceView* maskSRV = nullptr;

			// ========== 独立计算瞄镜场景亮度 ==========
			// 配置: 是否使用独立亮度计算 (true = 使用 LuminancePass, false = 使用主画面捕获的亮度)
			static bool useScopeLuminance = true;  // 启用独立亮度计算

			if (useScopeLuminance) {
				// 初始化 LuminancePass
				if (!m_luminancePass) {
					m_luminancePass = LuminancePass::GetSingleton();
					if (!m_luminancePass->IsInitialized()) {
						if (!m_luminancePass->Initialize(m_device, m_context)) {
							logger::error("ApplyCustomHDREffect: Failed to initialize LuminancePass");
							m_luminancePass = nullptr;
						}
					}
				}

				// 使用瞄镜场景计算独立亮度
				if (m_luminancePass && m_luminancePass->IsInitialized()) {
					luminanceSRV = m_luminancePass->Compute(
						m_hdrTempSRV,
						mainDesc.Width,
						mainDesc.Height,
						0.016f  // deltaTime, 约60fps
					);

					if (luminanceSRV) {
						//logger::debug("ApplyCustomHDREffect: Using scope-specific luminance");
					}
				}
			}

			// 回退到主画面捕获的亮度
			// [FIXED] 禁用主场景缓存的 luminance - 可能与瞄具 FOV 不匹配
			// if (!luminanceSRV && g_HDRStateCache.IsValid()) {
			// 	luminanceSRV = g_HDRStateCache.luminanceSRVCopy.Get();
			// 	if (!luminanceSRV) {
			// 		luminanceSRV = g_HDRStateCache.psSRVs[2].Get();
			// 	}
			// 	logger::debug("ApplyCustomHDREffect: Fallback to captured main scene luminance");
			// }
			// 使用 nullptr，让 HDR shader 使用默认值

			// 获取 Bloom 纹理
			// [FIXED] 禁用主场景缓存的 bloom - 可能与瞄具 FOV 不匹配
			// if (g_HDRStateCache.IsValid()) {
			// 	bloomSRV = g_HDRStateCache.bloomSRVCopy.Get();
			// 	if (!bloomSRV) {
			// 		bloomSRV = g_HDRStateCache.psSRVs[0].Get();
			// 	}
			// }
			// 使用 nullptr，让 HDR shader 使用默认值
			bloomSRV = nullptr;


			// 获取深度纹理 (用于 DOF)
			// 使用 srViewDepth (offset 0x88) 而不是 dsView
			// dsView 是 DepthStencilView，不能转换为 ShaderResourceView
			if (rendererData->depthStencilTargets[2].srViewDepth) {
				depthSRV = (ID3D11ShaderResourceView*)rendererData->depthStencilTargets[2].srViewDepth;
			}

			// [FIXED] 不使用 Mask 纹理 (GameRT_24 = TAA Jitter Mask)
			// 原因: TAA Jitter Mask 是从主场景捕获的，包含主场景物体的轮廓信息。
			// 当瞄具 FOV 与主相机 FOV 不同时（特别是用户调整 FOV 时），
			// Mask 中的轮廓数据与当前帧不匹配，导致物体轮廓伪影随 FOV 变化。
			// 解决方案: 瞄具场景的 HDR 处理不使用 Mask 纹理，改为传入 nullptr。
			// HDR shader 应该有处理 nullptr mask 的逻辑（使用默认值或跳过 mask 相关处理）。
			maskSRV = nullptr;



			// ========== 新版后处理管线 ==========
			if (useNewPostProcessPipeline) {
				// 初始化 ScopePostProcess
				if (!m_postProcess) {
					m_postProcess = ScopePostProcess::GetSingleton();
					if (!m_postProcess->IsInitialized()) {
						if (!m_postProcess->Initialize(m_device, m_context)) {
							logger::error("ApplyCustomHDREffect: Failed to initialize ScopePostProcess, falling back to legacy");
							useNewPostProcessPipeline = false;
						}
					}
				}

				if (m_postProcess && m_postProcess->IsInitialized()) {
					// 配置后处理参数
					PostProcessConfig config;
					config.bloomEnabled = false;  // Bloom 纹理似乎是空的，暂时禁用
					config.bloomIntensity = 1.0f;
					config.dofEnabled = false;  // DOF 暂时禁用
					config.dofStrength = 0.3f;
					config.focalPlane = 10.0f;
					config.focalRange = 5.0f;
					config.nearBlurEnabled = true;
					config.farBlurEnabled = true;
					config.hdrEnabled = true;   // 启用 HDR（场景可能需要曝光调整）
					config.lutEnabled = true;

					// 设置 LUT 纹理 (从 ImageSpaceManager 获取)
					auto imageSpaceMgr = RE::ImageSpaceManager::GetSingleton();
					if (imageSpaceMgr) {
						const auto& lutData = imageSpaceMgr->lutData;
						ID3D11ShaderResourceView* lut0 = lutData.texture[0] ? (ID3D11ShaderResourceView*)lutData.texture[0]->pSRView : nullptr;
						ID3D11ShaderResourceView* lut1 = lutData.texture[1] ? (ID3D11ShaderResourceView*)lutData.texture[1]->pSRView : nullptr;
						ID3D11ShaderResourceView* lut2 = lutData.texture[2] ? (ID3D11ShaderResourceView*)lutData.texture[2]->pSRView : nullptr;
						ID3D11ShaderResourceView* lut3 = lutData.texture[3] ? (ID3D11ShaderResourceView*)lutData.texture[3]->pSRView : nullptr;

						if (lut0 || lut1 || lut2 || lut3) {
							m_postProcess->SetLUTTextures(lut0, lut1, lut2, lut3);
						}

						// 更新 LUT 混合权重 (传递给 LUTPass)
						auto* lutPass = m_postProcess->GetLUTPass();
						if (lutPass) {
							lutPass->SetLUTWeights(
								lutData.weight[0],
								lutData.weight[1],
								lutData.weight[2],
								lutData.weight[3]
							);
						}
					}

					// 应用完整后处理流水线
					m_postProcess->Apply(
						m_hdrTempSRV,     // 场景纹理
						bloomSRV,         // Bloom 纹理
						depthSRV,         // 深度纹理 (用于 DOF)
						luminanceSRV,     // Luminance (用于自动曝光)
						maskSRV,          // Mask
						outputRTV,        // 输出
						mainDesc.Width,
						mainDesc.Height,
						config
					);

					D3DPERF_EndEvent();
					return;
				}
			}

			// ========== 旧版后处理 (ScopeHDR only) ==========
			// 如果新管线失败或被禁用，回退到旧版本
			if (!m_scopeHDR) {
				m_scopeHDR = ScopeHDR::GetSingleton();
				if (!m_scopeHDR->IsInitialized()) {
					if (!m_scopeHDR->Initialize(m_device, m_context)) {
						logger::error("ApplyCustomHDREffect: Failed to initialize ScopeHDR");
						D3DPERF_EndEvent();
						return;
					}
				}
			}

			// 配置 HDR 参数
			//m_scopeHDR->SetSkipHDRTonemapping(false);
			//m_scopeHDR->SetExposure(0);
			//m_scopeHDR->SetExposureMultiplier(1.0f);
			//m_scopeHDR->GetConstants().WhitePoint = 0.03f;

			// 注意: LUT 纹理和混合权重现在由 ScopePostProcess -> LUTPass 处理
			// 此处直接调用 m_scopeHDR 只进行 HDR tonemapping

			m_scopeHDR->Apply(
				m_hdrTempSRV,
				bloomSRV,
				luminanceSRV,
				maskSRV,
				outputRTV
			);

		} catch (...) {
			logger::error("ApplyCustomHDREffect: Exception during HDR effect rendering");
		}

		// 清理渲染状态以防止纹理泄漏到后续渲染
		// 这解决了放大 FOV 时半透明物体错误显示的问题
		{
			// 清除所有 PS SRV 绑定 (slot 0-15)
			ID3D11ShaderResourceView* nullSRVs[16] = {};
			m_context->PSSetShaderResources(0, 16, nullSRVs);

			// 清除 CS SRV 绑定 (slot 0-7)
			m_context->CSSetShaderResources(0, 8, nullSRVs);

			// 清除 CS UAV 绑定
			ID3D11UnorderedAccessView* nullUAVs[4] = {};
			m_context->CSSetUnorderedAccessViews(0, 4, nullUAVs, nullptr);

			// 清除 RTV 绑定
			ID3D11RenderTargetView* nullRTV = nullptr;
			m_context->OMSetRenderTargets(1, &nullRTV, nullptr);

			// 清除采样器
			ID3D11SamplerState* nullSamplers[4] = {};
			m_context->PSSetSamplers(0, 4, nullSamplers);
		}

		D3DPERF_EndEvent();
	}


	void SecondPassRenderer::RestoreFirstPass()
	{
		D3DPERF_BeginEvent(0xFFF00000, L"SecondPassRenderer::RestoreFirstPass");
		if (m_lightingSynced) {
			// 恢复光源状态
			m_lightBackup->RestoreLightStates();

			// 恢复阴影节点状态
			auto pShadowSceneNode = *ptr_DrawWorldShadowNode;
			if (pShadowSceneNode) {
				pShadowSceneNode->bDisableLightUpdate = false;
			}
		}

		if (m_texturesBackedUp) {
			// 复制第二次渲染结果到我们的纹理
			D3D11_TEXTURE2D_DESC secondPassColorDesc, mainRTDesc;
			RenderUtilities::GetSecondPassColorTexture()->GetDesc(&secondPassColorDesc);
			m_mainRTTexture->GetDesc(&mainRTDesc);

			if (secondPassColorDesc.Width == mainRTDesc.Width && secondPassColorDesc.Height == mainRTDesc.Height) {
				m_context->CopyResource(RenderUtilities::GetSecondPassColorTexture(), m_mainRTTexture);
			} else {
				logger::warn("Skipping second pass color copy: size mismatch {}x{} vs {}x{}",
					secondPassColorDesc.Width, secondPassColorDesc.Height,
					mainRTDesc.Width, mainRTDesc.Height);
			}

			// 恢复BackBuffer
			D3D11_TEXTURE2D_DESC rtDesc, tempBackBufferDesc;
			m_rtTexture2D->GetDesc(&rtDesc);
			m_tempBackBufferTex->GetDesc(&tempBackBufferDesc);

			if (rtDesc.Width == tempBackBufferDesc.Width && rtDesc.Height == tempBackBufferDesc.Height) {
				m_context->CopyResource(m_rtTexture2D, m_tempBackBufferTex);
			} else {
				logger::warn("Skipping BackBuffer restore: size mismatch {}x{} vs {}x{}",
					rtDesc.Width, rtDesc.Height,
					tempBackBufferDesc.Width, tempBackBufferDesc.Height);
			}

			RenderUtilities::SetSecondPassComplete(true);

			// 如果第二次渲染完成，恢复主渲染目标内容用于正常显示
			if (RenderUtilities::IsSecondPassComplete()) {
				D3D11_TEXTURE2D_DESC firstPassColorDesc, mainRTDesc2;
				RenderUtilities::GetFirstPassColorTexture()->GetDesc(&firstPassColorDesc);
				m_mainRTTexture->GetDesc(&mainRTDesc2);

				if (firstPassColorDesc.Width == mainRTDesc2.Width && firstPassColorDesc.Height == mainRTDesc2.Height) {
					m_context->CopyResource(m_mainRTTexture, RenderUtilities::GetFirstPassColorTexture());
				} else {
					logger::warn("Skipping first pass color restore: size mismatch {}x{} vs {}x{}",
						firstPassColorDesc.Width, firstPassColorDesc.Height,
						mainRTDesc2.Width, mainRTDesc2.Height);
				}

				D3D11_TEXTURE2D_DESC firstPassDepthDesc, mainDSDesc;
				RenderUtilities::GetFirstPassDepthTexture()->GetDesc(&firstPassDepthDesc);
				m_mainDSTexture->GetDesc(&mainDSDesc);

				if (firstPassDepthDesc.Width == mainDSDesc.Width && firstPassDepthDesc.Height == mainDSDesc.Height) {
					m_context->CopyResource(m_mainDSTexture, RenderUtilities::GetFirstPassDepthTexture());
				} else {
					logger::warn("Skipping first pass depth restore: size mismatch {}x{} vs {}x{}",
						firstPassDepthDesc.Width, firstPassDepthDesc.Height,
						mainDSDesc.Width, mainDSDesc.Height);
				}
			}

			// 恢复渲染目标
			ID3D11RenderTargetView* targetRTV = nullptr;
			ID3D11Texture2D* targetTexture = nullptr;
			UINT targetWidth = 0, targetHeight = 0;
			
			// [FIX] Upscaling 兼容：当 Upscaling 激活时，渲染到 kFrameBuffer (索引0)
			// 因为 Upscaling::PostDisplay() 从 kFrameBuffer 复制
			auto scopeRenderMgr = ScopeRenderingManager::GetSingleton();
			auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
			
			if (scopeRenderMgr->IsUpscalingActive() && rendererData) {
				// Upscaling 模式：必须渲染到 kFrameBuffer (索引0)
				auto& frameBuffer = rendererData->renderTargets[0];  // kFrameBuffer = 0
				ID3D11RenderTargetView* frameBufferRTV = reinterpret_cast<ID3D11RenderTargetView*>(frameBuffer.rtView);
				
				if (frameBufferRTV) {
					targetRTV = frameBufferRTV;
					m_context->OMSetRenderTargets(1, &frameBufferRTV, nullptr);
					
					// 获取 FrameBuffer 尺寸
					ID3D11Resource* rtResource = nullptr;
					frameBufferRTV->GetResource(&rtResource);
					if (rtResource) {
						ID3D11Texture2D* rtTexture = nullptr;
						if (SUCCEEDED(rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&rtTexture))) {
							D3D11_TEXTURE2D_DESC rtDesc;
							rtTexture->GetDesc(&rtDesc);
							targetWidth = rtDesc.Width;
							targetHeight = rtDesc.Height;

							rtTexture->Release();
						}
						rtResource->Release();
					}
				}
			} else if (m_savedRTVs[1]) {
				targetRTV = m_savedRTVs[1];
				m_context->OMSetRenderTargets(1, &m_savedRTVs[1], nullptr);
				
				// 获取渲染目标的实际尺寸
				ID3D11Resource* rtResource = nullptr;
				m_savedRTVs[1]->GetResource(&rtResource);
				if (rtResource) {
					ID3D11Texture2D* rtTexture = nullptr;
					if (SUCCEEDED(rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&rtTexture))) {
						D3D11_TEXTURE2D_DESC rtDesc;
						rtTexture->GetDesc(&rtDesc);
						targetWidth = rtDesc.Width;
						targetHeight = rtDesc.Height;
						rtTexture->Release();
					}
					rtResource->Release();
				}
			} else {
				// PreUI 阶段或其他情况: 使用主渲染目标
				if (rendererData && m_mainRTV) {
					targetRTV = m_mainRTV;
					m_context->OMSetRenderTargets(1, &m_mainRTV, nullptr);
					
					if (m_mainRTTexture) {
						D3D11_TEXTURE2D_DESC mainDesc;
						m_mainRTTexture->GetDesc(&mainDesc);
						targetWidth = mainDesc.Width;
						targetHeight = mainDesc.Height;
					}
				}
			}


			// 渲染瞄具内容
			int scopeNodeIndexCount = ScopeCamera::GetScopeNodeIndexCount();
			if (scopeNodeIndexCount != -1 && targetRTV) {
				try {
					// 设置 viewport
					D3D11_VIEWPORT viewport = {};
					
					auto scopeRenderMgr = ScopeRenderingManager::GetSingleton();
					if (scopeRenderMgr->IsUpscalingActive()) {
						// Upscaling 模式：必须使用 FirstPassViewport（动态分辨率）
						// Upscaling 动态调整 viewport 大小，如 1129.4 x 635.3
						if (!RenderUtilities::GetFirstPassViewport(viewport)) 
						{
							auto rendererState = RE::BSGraphics::State::GetSingleton();
								viewport.TopLeftX = 0;
								viewport.TopLeftY = 0;
								viewport.Width = static_cast<float>(rendererState.backBufferWidth);
								viewport.Height = static_cast<float>(rendererState.backBufferHeight);
								viewport.MinDepth = 0.0f;
								viewport.MaxDepth = 1.0f;	
						}
					} else {
						// 非 Upscaling 模式：使用 targetWidth/Height
						viewport.TopLeftX = 0;
						viewport.TopLeftY = 0;
						viewport.Width = static_cast<float>(targetWidth);
						viewport.Height = static_cast<float>(targetHeight);
						viewport.MinDepth = 0.0f;
						viewport.MaxDepth = 1.0f;
					}
					
					m_context->RSSetViewports(1, &viewport);
					
					RenderUtilities::SetRender_PreUIComplete(true);
					m_d3dHooks->SetScopeTexture(m_context);
					D3DHooks::isSelfDrawCall = true;
					m_context->DrawIndexed(scopeNodeIndexCount, 0, 0);
					D3DHooks::isSelfDrawCall = false;
					RenderUtilities::SetRender_PreUIComplete(false);
				} catch (...) {
					logger::error("Exception during scope content rendering");
					RenderUtilities::SetRender_PreUIComplete(false);
				}
			}
		}

		if (m_cameraUpdated) {
			// 恢复原始相机
			DrawWorld::SetCamera(m_originalCamera);
			DrawWorld::SetUpdateCameraFOV(true);

			RE::NiUpdateData nData;
			nData.camera = m_originalCamera;
			m_originalCamera->Update(nData);
		}
		D3DPERF_EndEvent();
	}

	void SecondPassRenderer::CleanupResources()
	{
		SAFE_RELEASE(m_tempBackBufferTex);
		SAFE_RELEASE(m_tempBackBufferSRV);
		SAFE_RELEASE(m_rtTexture2D);
		SAFE_RELEASE(m_savedRTVs[0]);
		SAFE_RELEASE(m_savedRTVs[1]);

		// 清理Camera克隆
		if (m_originalCamera) {
			if (m_originalCamera->DecRefCount() == 0) {
				m_originalCamera->DeleteThis();
			}
			m_originalCamera = nullptr;
		}

		// 重置状态标志
		m_texturesBackedUp = false;
		m_cameraUpdated = false;
		m_lightingSynced = false;
		m_renderExecuted = false;
	}

	bool SecondPassRenderer::ValidateD3DResources() const
	{
		if (!m_context || !m_device || !m_d3dHooks) {
			m_lastError = "D3D resources are null";
			return false;
		}

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || !rendererData->context) {
			m_lastError = "Renderer data or context is null";
			return false;
		}

		return true;
	}

	bool SecondPassRenderer::CreateTemporaryBackBuffer()
	{
		if (!m_rtTexture2D) {
			m_lastError = "No render target texture available";
			return false;
		}

		D3D11_TEXTURE2D_DESC originalDesc;
		m_rtTexture2D->GetDesc(&originalDesc);

		// 如果已存在临时纹理，检查尺寸是否匹配
		if (m_tempBackBufferTex) {
			D3D11_TEXTURE2D_DESC existingDesc;
			m_tempBackBufferTex->GetDesc(&existingDesc);

			// 如果尺寸匹配，直接返回
			if (existingDesc.Width == originalDesc.Width &&
				existingDesc.Height == originalDesc.Height &&
				existingDesc.Format == originalDesc.Format) {
				return true;
			}

			SAFE_RELEASE(m_tempBackBufferTex);
			SAFE_RELEASE(m_tempBackBufferSRV);
		}

		D3D11_TEXTURE2D_DESC rtTextureDesc = originalDesc;
		rtTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		HRESULT hr = m_device->CreateTexture2D(&rtTextureDesc, nullptr, &m_tempBackBufferTex);
		if (FAILED(hr)) {
			m_lastError = "Failed to create temporary BackBuffer texture";
			return false;
		}

		hr = m_device->CreateShaderResourceView(m_tempBackBufferTex, NULL, &m_tempBackBufferSRV);
		if (FAILED(hr)) {
			m_lastError = "Failed to create temporary BackBuffer SRV";
			SAFE_RELEASE(m_tempBackBufferTex);
			return false;
		}

		return true;
	}

	void SecondPassRenderer::ConfigureScopeFrustum(RE::NiCamera* scopeCamera, RE::NiCamera* originalCamera)
	{
		if (!scopeCamera || !originalCamera) {
			return;
		}

		// 使用原始相机的视锥体作为基础
		scopeCamera->viewFrustum = originalCamera->viewFrustum;

		// 调整视锥体参数以适应瞄具的FOV
		float aspectRatio = scopeCamera->viewFrustum.right / scopeCamera->viewFrustum.top;
		float targetFOV = ScopeCamera::GetTargetFOV();

		// 限制FOV范围以避免极端俯仰角下的数值不稳定
		float pitch = asin(-scopeCamera->world.rotate.entry[2][1]);
		float pitchDeg = pitch * 57.295779513f;
		if (abs(pitchDeg) > 70.0f) {
			float factor = (90.0f - abs(pitchDeg)) / 20.0f;
			factor = std::max(0.3f, std::min(1.0f, factor));
			targetFOV = targetFOV * factor;
		}

		float fovRad = targetFOV * 0.01745329251f;
		float halfFovTan = tan(fovRad * 0.5f);

		// 根据FOV重新计算视锥体边界
		scopeCamera->viewFrustum.top = scopeCamera->viewFrustum.nearPlane * halfFovTan;
		scopeCamera->viewFrustum.bottom = -scopeCamera->viewFrustum.top;
		scopeCamera->viewFrustum.right = scopeCamera->viewFrustum.top * aspectRatio;
		scopeCamera->viewFrustum.left = -scopeCamera->viewFrustum.right;

		// 保持远裁剪面距离
		scopeCamera->viewFrustum.farPlane = originalCamera->viewFrustum.farPlane;
	}

	void SecondPassRenderer::SyncAccumulatorEyePosition(RE::NiCamera* scopeCamera)
	{
		if (!scopeCamera) {
			return;
		}

		auto pDrawWorldAccum = *ptr_DrawWorldAccum;
		auto p1stPersonAccum = *ptr_Draw1stPersonAccum;
		auto pShadowSceneNode = *ptr_DrawWorldShadowNode;

		RE::NiPoint3A scopeEyePos = RE::NiPoint3A(scopeCamera->world.translate.x, scopeCamera->world.translate.y, scopeCamera->world.translate.z);

		if (pDrawWorldAccum) {
			pDrawWorldAccum->kEyePosition = scopeEyePos;
			pDrawWorldAccum->ClearActivePasses(false);
			pDrawWorldAccum->m_pkCamera = scopeCamera;
		}

		if (p1stPersonAccum) {
			p1stPersonAccum->kEyePosition = scopeEyePos;
		}

		if (pShadowSceneNode) {
			pShadowSceneNode->kEyePosition = scopeEyePos;
			pShadowSceneNode->bDisableLightUpdate = false;
			pShadowSceneNode->fStoredFarClip = scopeCamera->viewFrustum.farPlane;
			pShadowSceneNode->bAlwaysUpdateLights = true;
		}

		// 配置几何剔除进程
		auto geomListCullProc0 = *DrawWorldGeomListCullProc0;
		auto geomListCullProc1 = *DrawWorldGeomListCullProc1;

		if (geomListCullProc0) {
			geomListCullProc0->m_pkCamera = scopeCamera;
			geomListCullProc0->SetFrustum(&scopeCamera->viewFrustum);
		}

		if (geomListCullProc1) {
			geomListCullProc1->m_pkCamera = scopeCamera;
			geomListCullProc1->SetFrustum(&scopeCamera->viewFrustum);
		}
	}
	// ========== Motion Vector Mask (fo4test 兼容) ==========
	bool SecondPassRenderer::InitializeMotionVectorMask()
	{
		return true; // Use shared shaders from RenderUtilities
	}

	void SecondPassRenderer::ShutdownMotionVectorMask()
	{
		// Nothing to clean up locally
	}

	void SecondPassRenderer::ApplyMotionVectorMask()
	{
		// 使用 Stencil Test 自动清除 Scope 区域的 Motion Vectors (RT 29)
		// Scope 渲染时会写入 Stencil，此处利用 Stencil != 0 来通过测试
		// 相比 Scissor Rect，这能完美贴合任意形状的瞄具模型

		if (!m_context || !m_device || !m_mainDSV) return;

		D3DPERF_BeginEvent(0xFF0000FF, L"ApplyMotionVectorMask");

		// 1. 获取 Motion Vector Render Target (Index 29)
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		ID3D11RenderTargetView* mvRTV = nullptr;
		if (rendererData && rendererData->renderTargets[29].rtView) {
			mvRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[29].rtView;
		} else {
			D3DPERF_EndEvent();
			return; // No MV RT found
		}

		// 2. 备份当前渲染状态
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
		m_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs[0].GetAddressOf(), oldDSV.GetAddressOf());
		
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDSS;
		UINT oldStencilRef;
		m_context->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldStencilRef);

		D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		m_context->RSGetViewports(&numViewports, oldViewports);

		Microsoft::WRL::ComPtr<ID3D11RasterizerState> oldRS;
		m_context->RSGetState(oldRS.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11BlendState> oldBS;
		float oldBlendFactor[4];
		UINT oldSampleMask;
		m_context->OMGetBlendState(oldBS.GetAddressOf(), oldBlendFactor, &oldSampleMask);

		Microsoft::WRL::ComPtr<ID3D11VertexShader> oldVS;
		m_context->VSGetShader(oldVS.GetAddressOf(), nullptr, nullptr);
		Microsoft::WRL::ComPtr<ID3D11PixelShader> oldPS;
		m_context->PSGetShader(oldPS.GetAddressOf(), nullptr, nullptr);
		D3D11_PRIMITIVE_TOPOLOGY oldTopology;
		m_context->IAGetPrimitiveTopology(&oldTopology);
		Microsoft::WRL::ComPtr<ID3D11InputLayout> oldInputLayout;
		m_context->IAGetInputLayout(oldInputLayout.GetAddressOf());

		try {
			// 3. 配置 Depth Test (Masking)
			// 使用深度测试代替 Stencil。背景被 Clear 到 1.0，Scope 几何体深度 < 1.0。
			// 我们绘制一个全屏 Quad，强制其深度为 1.0 (通过 Viewport Min/MaxDepth)。
			// 设置 DepthFunc = GREATER (1.0 > BufferValue)。
			// - 如果 Buffer 是背景(1.0)，1.0 > 1.0 为假，不绘制。
			// - 如果 Buffer 是 Scope(<1.0)，1.0 > 0.x 为真，绘制(清除 MV)。
			D3D11_DEPTH_STENCIL_DESC dssDesc;
			ZeroMemory(&dssDesc, sizeof(dssDesc));
			dssDesc.DepthEnable = TRUE;
			dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 不写入深度
			dssDesc.DepthFunc = D3D11_COMPARISON_GREATER; 
			dssDesc.StencilEnable = FALSE;

			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dssState;
			m_device->CreateDepthStencilState(&dssDesc, &dssState);
			m_context->OMSetDepthStencilState(dssState.Get(), 0); 

			// 4. 配置 Rasterizer State
			D3D11_RASTERIZER_DESC rsDesc;
			ZeroMemory(&rsDesc, sizeof(rsDesc));
			rsDesc.FillMode = D3D11_FILL_SOLID;
			rsDesc.CullMode = D3D11_CULL_NONE;
			rsDesc.FrontCounterClockwise = FALSE;
			rsDesc.DepthBias = 0;
			rsDesc.SlopeScaledDepthBias = 0.0f;
			rsDesc.DepthBiasClamp = 0.0f;
			rsDesc.DepthClipEnable = TRUE;
			rsDesc.ScissorEnable = FALSE;
			rsDesc.MultisampleEnable = FALSE;
			rsDesc.AntialiasedLineEnable = FALSE;

			Microsoft::WRL::ComPtr<ID3D11RasterizerState> rsState;
			m_device->CreateRasterizerState(&rsDesc, &rsState);
			m_context->RSSetState(rsState.Get());

			// 设置 Viewport 为Motion Vector RT的实际尺寸，强制深度为 1.0
			// [FIX] 直接从RT29获取尺寸，而不是使用硬编码的屏幕尺寸
			// 这修复了2160p分辨率下的TAA鬼影问题（之前使用1920x1080导致只覆盖左上1/4）
			D3D11_VIEWPORT vp;
			UINT mvWidth = 0, mvHeight = 0;
			
			// 从Motion Vector RTV获取实际纹理尺寸
			ID3D11Resource* mvResource = nullptr;
			mvRTV->GetResource(&mvResource);
			if (mvResource) {
				ID3D11Texture2D* mvTexture = nullptr;
				if (SUCCEEDED(mvResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&mvTexture))) {
					D3D11_TEXTURE2D_DESC mvDesc;
					mvTexture->GetDesc(&mvDesc);
					mvWidth = mvDesc.Width;
					mvHeight = mvDesc.Height;
					mvTexture->Release();
				}
				mvResource->Release();
			}
			
			// 回退到深度缓冲尺寸如果无法获取MV尺寸
			if (mvWidth == 0 || mvHeight == 0) {
				if (m_mainDSTexture) {
					D3D11_TEXTURE2D_DESC dsDesc;
					m_mainDSTexture->GetDesc(&dsDesc);
					mvWidth = dsDesc.Width;
					mvHeight = dsDesc.Height;
				}
			}
			
			vp.Width = (float)mvWidth;
			vp.Height = (float)mvHeight;
			vp.MinDepth = 1.0f; // Force Z = 1.0
			vp.MaxDepth = 1.0f; // Force Z = 1.0
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			m_context->RSSetViewports(1, &vp);

			// 5. 设置 Render Target (Attach MV RTV and Main DSV)
			m_context->OMSetRenderTargets(1, &mvRTV, m_mainDSV);

			// 6. 设置 Shaders
			m_context->VSSetShader(RenderUtilities::GetClearVelocityVS(), nullptr, 0);
			m_context->PSSetShader(RenderUtilities::GetClearVelocityPS(), nullptr, 0);
			m_context->IASetInputLayout(nullptr);
			m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 7. 设置 Blend State (Overwrite)
			D3D11_BLEND_DESC blendDesc;
			ZeroMemory(&blendDesc, sizeof(blendDesc));
			blendDesc.RenderTarget[0].BlendEnable = FALSE;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
			m_device->CreateBlendState(&blendDesc, &blendState);
			float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_context->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);

			// 8. 绘制 Fullscreen Triangle
			m_context->Draw(3, 0);

		} catch (...) {
			logger::warn("ApplyMotionVectorMask: Exception");
		}

	// 9. 恢复状态
		m_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldRTVs[0].GetAddressOf(), oldDSV.Get());
		m_context->OMSetDepthStencilState(oldDSS.Get(), oldStencilRef);
		m_context->RSSetState(oldRS.Get());
		m_context->RSSetViewports(numViewports, oldViewports); 
		m_context->OMSetBlendState(oldBS.Get(), oldBlendFactor, oldSampleMask);
		m_context->VSSetShader(oldVS.Get(), nullptr, 0);
		m_context->PSSetShader(oldPS.Get(), nullptr, 0);
		m_context->IASetPrimitiveTopology(oldTopology);
		m_context->IASetInputLayout(oldInputLayout.Get());

		D3DPERF_EndEvent();
	}
}
