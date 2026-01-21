#include "SecondPassRenderer.h"
#include "GlobalTypes.h"
#include "HookManager.h"

#include "RenderTargetMerger.h"
#include "RE/Bethesda/ImageSpaceManager.hpp"
#include <winternl.h>
#include <DirectXMath.h>

#include "ScopeRenderingManager.h"
#include "STSCompatibility.h"
#include "ScopeCulling.h"

namespace ThroughScope
{
	void SafeShadowNodeUpdate(RE::NiNode* node, RE::NiUpdateData* ctx)
	{
		__try {
			if (node && ctx) {
				node->Update(*ctx);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			// Catch potential 0xC0000005 AV from background thread race
		}
	}
	SecondPassRenderer::SecondPassRenderer(ID3D11DeviceContext* context, ID3D11Device* device, D3DHooks* d3dHooks) 
		: m_context(context)
		, m_device(device)
		, m_d3dHooks(d3dHooks)
		, m_lightBackup(LightBackupSystem::GetSingleton())

		, m_renderStateMgr(RenderStateManager::GetSingleton())
	{
		if (!ValidateD3DResources()) {
			logger::error("SecondPassRenderer: Invalid D3D resources provided");
		}
	}

	SecondPassRenderer::~SecondPassRenderer()
	{
		CleanupResources();


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
			// [STS兼容] 在渲染前隐藏ScopeAiming节点，避免双重视差/准星
			auto stsCompat = STSCompatibility::GetSingleton();
			RE::NiNode* weaponNode = nullptr;
			if (g_pchar && g_pchar->Get3D()) {
				auto weapon3D = g_pchar->Get3D()->GetObjectByName("Weapon");
				if (weapon3D && weapon3D->IsNode()) {
					weaponNode = static_cast<RE::NiNode*>(weapon3D);
					stsCompat->HideScopeAimingForRender(weaponNode);
				}
			}
			
			DrawScopeContent();
			m_renderExecuted = true;
			
			// [STS兼容] 恢复ScopeAiming节点可见性
			stsCompat->RestoreScopeAimingAfterRender(weaponNode);
			
			//ApplyCustomHDREffect(); 


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

			// Use centralized RenderTargetMerger to backup all required render targets
			RenderTargetMerger::GetInstance().BackupRenderTargets(m_context);

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

	bool SecondPassRenderer::BackupDepthBuffer()
	{
		if (!m_context || !m_device || !m_mainDSTexture) {
			return false;
		}

		D3DPERF_BeginEvent(0xFF00FFFF, L"BackupDepthBuffer");

		// 获取当前深度缓冲区的描述
		D3D11_TEXTURE2D_DESC depthDesc;
		m_mainDSTexture->GetDesc(&depthDesc);

		// 创建备份纹理（如果尚未创建或尺寸变化）
		if (!m_depthBackupCreated) {
			// 创建可用于 SRV 的深度备份纹理
			D3D11_TEXTURE2D_DESC backupDesc = depthDesc;
			backupDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			backupDesc.Format = DXGI_FORMAT_R32_FLOAT;  // 使用可读取的格式
			backupDesc.Usage = D3D11_USAGE_DEFAULT;

			if (FAILED(m_device->CreateTexture2D(&backupDesc, nullptr, &m_depthBackupTex))) {
				logger::error("Failed to create depth backup texture");
				D3DPERF_EndEvent();
				return false;
			}

			// 创建 SRV
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(srvDesc));
			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;

			if (FAILED(m_device->CreateShaderResourceView(m_depthBackupTex, &srvDesc, &m_depthBackupSRV))) {
				logger::error("Failed to create depth backup SRV");
				m_depthBackupTex->Release();
				m_depthBackupTex = nullptr;
				D3DPERF_EndEvent();
				return false;
			}

			m_depthBackupCreated = true;
		}

		D3DPERF_EndEvent();
		return true;
	}

	void SecondPassRenderer::DrawScopeContent()
	{
		HookManager::FlushBackgroundTasks();

		if (ptr_DrawWorldShadowNode.get() && ptr_DrawWorldShadowNode.address()) {
			D3DPERF_BeginEvent(0xffffffff, L"UpdateSceneGraph");
			auto shadowNode = *ptr_DrawWorldShadowNode;
			RE::NiUpdateData ctx{};
			ctx.flags = 0; // 默认标志
			ctx.time = 0.0f; // 假设不需要时间步进，只更新空间关系
			
			RE::NiNode* updateTarget = shadowNode;
			
			if (g_pchar) {
				auto player3D = g_pchar->Get3D(false);
				if (player3D && player3D->parent) {
					updateTarget = player3D->parent;
				}
			}

			SafeShadowNodeUpdate(updateTarget, &ctx);
			D3DPERF_EndEvent();
		}

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

		ScopedCameraBackup cameraGuard;

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

				// 保存当前矩阵到 "上一帧" 供下一帧 MV 计算使用
				if (g_ScopeViewProjMatValid) {
					g_ScopePreviousViewProjMat[0] = g_ScopeViewProjMat[0];
					g_ScopePreviousViewProjMat[1] = g_ScopeViewProjMat[1];
					g_ScopePreviousViewProjMat[2] = g_ScopeViewProjMat[2];
					g_ScopePreviousViewProjMat[3] = g_ScopeViewProjMat[3];
					g_ScopePreviousViewProjMatValid = true;
				}

				// 保存当前帧矩阵
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
			
			// 缓存瞄具视锥体平面用于 BSCullingGroup::Add 过滤
			// 必须在相机数据（viewFrustum, world transform）设置完成后调用
			UpdateCachedScopeFrustumPlanes(m_scopeCamera);
			
			// 使用自定义裁剪平面
			// 从瞄具相机计算视锥体平面，设置为自定义裁剪平面
			{
				ScopedCustomCulling cullGuard(*DrawWorldCullingProcess, m_scopeCamera);
				hookMgr->g_RenderPreUIOriginal(savedDrawWorld);
			}

			// 恢复 PosAdjust
			if (posAdjustUpdated) {
				RE::BSGraphics::Renderer::GetSingleton().SetPosAdjust(&backupPosAdjust);
			}

			InvalidateCachedScopeFrustumPlanes();

			uint32_t tested, passed, filtered;
			GetAndResetCullingStats(tested, passed, filtered);

			// 清除渲染标志
			ScopeCamera::SetRenderingForScope(false);

			D3DPERF_EndEvent();
		}
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
			// 注意：SetScopeTexture 内部已经完成 DrawIndexed（已合并 stencil 写入和颜色渲染）
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
					// SetScopeTexture 现在内部完成 DrawIndexed（stencil + 颜色渲染已合并）
					m_d3dHooks->SetScopeTexture(m_context);
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
		// 使用 Stencil Test 合并 Motion Vectors：
		// - Scope 区域 (stencil == 127): 使用 FirstPassMV (第一次渲染的玩家相机 MV)
		// - 非 Scope 区域 (stencil != 127): 保留当前 RT29 的 MV
		// ScopeQuad 渲染时在 SetScopeTexture 中写入 stencil = 127

		if (!m_context || !m_device) return;

		D3DPERF_BeginEvent(0xFF0000FF, L"ApplyMotionVectorMask_StencilBased");

		// 1. 获取 Motion Vector Render Target (Index 29)
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		ID3D11RenderTargetView* mvRTV = nullptr;
		if (rendererData && rendererData->renderTargets[29].rtView) {
			mvRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[29].rtView;
		} else {
			D3DPERF_EndEvent();
			return; // No MV RT found
		}

		// 2. 检查 FirstPassMV 是否可用
		ID3D11ShaderResourceView* firstPassMVSRV = RenderUtilities::GetFirstPassMVSRV();
		if (!firstPassMVSRV) {
			D3DPERF_EndEvent();
			return; // 没有第一次渲染的 MV 备份，无法进行合并
		}

		// 3. 获取带 Stencil 的 DSV (depthStencilTargets[2])
		ID3D11DepthStencilView* stencilDSV = nullptr;
		if (rendererData && rendererData->depthStencilTargets[2].dsView[0]) {
			stencilDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
		}
		if (!stencilDSV) {
			D3DPERF_EndEvent();
			return; // 没有 DSV，无法进行 stencil 测试
		}
		
		// 4. 备份当前渲染状态
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
			// 5. 复制当前 RT29 到临时纹理（用于 shader 读取）
			ID3D11Texture2D* tempMVTexture = RenderUtilities::GetTempMVTexture();
			ID3D11ShaderResourceView* tempMVSRV = RenderUtilities::GetTempMVSRV();
			ID3D11ShaderResourceView* stencilSRV = RenderUtilities::GetStencilSRV();
			ID3D11PixelShader* mvBlendPS = RenderUtilities::GetMVBlendPS();

			if (!tempMVTexture || !tempMVSRV || !stencilSRV || !mvBlendPS) {
				// Fallback: 使用原来的硬边缘方式
				logger::warn("ApplyMotionVectorMask: Missing resources for edge feathering, using hard edge fallback");
				
				// 配置 Stencil Test (fallback - 硬边缘)
				D3D11_DEPTH_STENCIL_DESC dssDesc;
				ZeroMemory(&dssDesc, sizeof(dssDesc));
				dssDesc.DepthEnable = FALSE;
				dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				dssDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				dssDesc.StencilEnable = TRUE;
				dssDesc.StencilReadMask = 0xFF;
				dssDesc.StencilWriteMask = 0x00;
				dssDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
				dssDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
				dssDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
				dssDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
				dssDesc.BackFace = dssDesc.FrontFace;

				Microsoft::WRL::ComPtr<ID3D11DepthStencilState> stencilTestDSS;
				m_device->CreateDepthStencilState(&dssDesc, &stencilTestDSS);
				
				m_context->OMSetRenderTargets(1, &mvRTV, stencilDSV);
				m_context->OMSetDepthStencilState(stencilTestDSS.Get(), 127);
				m_context->VSSetShader(RenderUtilities::GetFullscreenVS(), nullptr, 0);
				m_context->PSSetShader(RenderUtilities::GetMVCopyPS(), nullptr, 0);
				m_context->PSSetShaderResources(0, 1, &firstPassMVSRV);
			} else {
				// Edge Feathering Mode：使用 MVBlendPS 做平滑过渡
				
				// Step 1: 复制当前 RT29 到临时纹理
				ID3D11Resource* mvResource = nullptr;
				mvRTV->GetResource(&mvResource);
				if (mvResource) {
					m_context->CopyResource(tempMVTexture, mvResource);
					mvResource->Release();
				}

				// Step 2: 禁用 stencil test（shader 内部读取 stencil）
				D3D11_DEPTH_STENCIL_DESC dssDesc;
				ZeroMemory(&dssDesc, sizeof(dssDesc));
				dssDesc.DepthEnable = FALSE;
				dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				dssDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				dssDesc.StencilEnable = FALSE;  // 禁用硬件 stencil test

				Microsoft::WRL::ComPtr<ID3D11DepthStencilState> noStencilDSS;
				m_device->CreateDepthStencilState(&dssDesc, &noStencilDSS);
				
				// Step 3: 设置 Render Target（不需要 DSV）
				m_context->OMSetRenderTargets(1, &mvRTV, nullptr);
				m_context->OMSetDepthStencilState(noStencilDSS.Get(), 0);
				
				// Step 4: 设置 Shaders
				m_context->VSSetShader(RenderUtilities::GetFullscreenVS(), nullptr, 0);
				m_context->PSSetShader(mvBlendPS, nullptr, 0);
				
				// Step 5: 绑定 3 个输入纹理
				// t0 = FirstPassMV, t1 = TempMV (当前 RT29), t2 = StencilSRV
				ID3D11ShaderResourceView* srvs[3] = { firstPassMVSRV, tempMVSRV, stencilSRV };
				m_context->PSSetShaderResources(0, 3, srvs);
				
				// Step 6: 创建并绑定常量缓冲区
				struct BlendConstants {
					float TexelSizeX;
					float TexelSizeY;
					float FeatherRadius;
					float StencilRef;
				};
				
				UINT mvWidth = 0, mvHeight = 0;
				ID3D11Resource* mvRes = nullptr;
				mvRTV->GetResource(&mvRes);
				if (mvRes) {
					ID3D11Texture2D* mvTex = nullptr;
					if (SUCCEEDED(mvRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&mvTex))) {
						D3D11_TEXTURE2D_DESC desc;
						mvTex->GetDesc(&desc);
						mvWidth = desc.Width;
						mvHeight = desc.Height;
						mvTex->Release();
					}
					mvRes->Release();
				}
				
				BlendConstants blendConst;
				blendConst.TexelSizeX = 1.0f / (float)mvWidth;
				blendConst.TexelSizeY = 1.0f / (float)mvHeight;
				blendConst.FeatherRadius = 5.0f;  // 5 像素羽化半径
				blendConst.StencilRef = 127.0f;   // Stencil 参考值
				
				D3D11_BUFFER_DESC cbDesc;
				ZeroMemory(&cbDesc, sizeof(cbDesc));
				cbDesc.ByteWidth = sizeof(BlendConstants);
				cbDesc.Usage = D3D11_USAGE_DYNAMIC;
				cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				
				D3D11_SUBRESOURCE_DATA cbData;
				cbData.pSysMem = &blendConst;
				cbData.SysMemPitch = 0;
				cbData.SysMemSlicePitch = 0;
				
				Microsoft::WRL::ComPtr<ID3D11Buffer> blendCB;
				m_device->CreateBuffer(&cbDesc, &cbData, &blendCB);
				m_context->PSSetConstantBuffers(0, 1, blendCB.GetAddressOf());
			}

			// 配置 Rasterizer State
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

			// 设置 Viewport
			UINT mvWidth2 = 0, mvHeight2 = 0;
			ID3D11Resource* mvResource2 = nullptr;
			mvRTV->GetResource(&mvResource2);
			if (mvResource2) {
				ID3D11Texture2D* mvTexture = nullptr;
				if (SUCCEEDED(mvResource2->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&mvTexture))) {
					D3D11_TEXTURE2D_DESC mvDesc;
					mvTexture->GetDesc(&mvDesc);
					mvWidth2 = mvDesc.Width;
					mvHeight2 = mvDesc.Height;
					mvTexture->Release();
				}
				mvResource2->Release();
			}
			
			D3D11_VIEWPORT vp;
			vp.Width = (float)mvWidth2;
			vp.Height = (float)mvHeight2;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			m_context->RSSetViewports(1, &vp);
			
			// 设置点采样器
			D3D11_SAMPLER_DESC sampDesc;
			ZeroMemory(&sampDesc, sizeof(sampDesc));
			sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			Microsoft::WRL::ComPtr<ID3D11SamplerState> pointSampler;
			m_device->CreateSamplerState(&sampDesc, &pointSampler);
			m_context->PSSetSamplers(0, 1, pointSampler.GetAddressOf());
			
			m_context->IASetInputLayout(nullptr);
			m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 设置 Blend State (Overwrite)
			D3D11_BLEND_DESC blendDesc;
			ZeroMemory(&blendDesc, sizeof(blendDesc));
			blendDesc.RenderTarget[0].BlendEnable = FALSE;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
			m_device->CreateBlendState(&blendDesc, &blendState);
			float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_context->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);

			// 绘制 Fullscreen Triangle
			m_context->Draw(3, 0);
			
			// 清理 SRV 绑定
			ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
			m_context->PSSetShaderResources(0, 3, nullSRVs);

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

	// ========== ApplyGBufferMask ==========
	// Merges first pass GBuffers with second pass GBuffers using stencil test
	// Outside scope region (stencil != 127): restore first pass GBuffer
	// Inside scope region (stencil == 127): keep second pass GBuffer
	void SecondPassRenderer::ApplyGBufferMask()
	{
		if (!m_context || !m_device) return;

		// Check if GBuffer backups are available
		ID3D11ShaderResourceView* firstPassNormalSRV = RenderUtilities::GetFirstPassGBufferNormalSRV();
		ID3D11ShaderResourceView* firstPassAlbedoSRV = RenderUtilities::GetFirstPassGBufferAlbedoSRV();
		
		if (!firstPassNormalSRV && !firstPassAlbedoSRV) {
			return; // No GBuffer backups available
		}

		D3DPERF_BeginEvent(0xFF00FF00, L"ApplyGBufferMask_StencilBased");

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		
		// Get stencil DSV
		ID3D11DepthStencilView* stencilDSV = nullptr;
		if (rendererData && rendererData->depthStencilTargets[2].dsView[0]) {
			stencilDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
		}
		if (!stencilDSV) {
			D3DPERF_EndEvent();
			return;
		}

		// Backup current state
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
			// Configure Stencil Test: only write where stencil != 127 (outside scope)
			D3D11_DEPTH_STENCIL_DESC dssDesc;
			ZeroMemory(&dssDesc, sizeof(dssDesc));
			dssDesc.DepthEnable = FALSE;
			dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dssDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
			dssDesc.StencilEnable = TRUE;
			dssDesc.StencilReadMask = 0xFF;
			dssDesc.StencilWriteMask = 0x00;
			dssDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dssDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			dssDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			dssDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;  // Pass where stencil != 127
			dssDesc.BackFace = dssDesc.FrontFace;

			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> stencilTestDSS;
			m_device->CreateDepthStencilState(&dssDesc, &stencilTestDSS);

			// Setup common rendering state
			D3D11_RASTERIZER_DESC rsDesc;
			ZeroMemory(&rsDesc, sizeof(rsDesc));
			rsDesc.FillMode = D3D11_FILL_SOLID;
			rsDesc.CullMode = D3D11_CULL_NONE;
			rsDesc.DepthClipEnable = TRUE;

			Microsoft::WRL::ComPtr<ID3D11RasterizerState> rsState;
			m_device->CreateRasterizerState(&rsDesc, &rsState);
			m_context->RSSetState(rsState.Get());

			D3D11_SAMPLER_DESC sampDesc;
			ZeroMemory(&sampDesc, sizeof(sampDesc));
			sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			Microsoft::WRL::ComPtr<ID3D11SamplerState> pointSampler;
			m_device->CreateSamplerState(&sampDesc, &pointSampler);
			m_context->PSSetSamplers(0, 1, pointSampler.GetAddressOf());

			m_context->IASetInputLayout(nullptr);
			m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			D3D11_BLEND_DESC blendDesc;
			ZeroMemory(&blendDesc, sizeof(blendDesc));
			blendDesc.RenderTarget[0].BlendEnable = FALSE;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
			m_device->CreateBlendState(&blendDesc, &blendState);
			float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_context->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);

			m_context->VSSetShader(RenderUtilities::GetFullscreenVS(), nullptr, 0);
			m_context->PSSetShader(RenderUtilities::GetMVCopyPS(), nullptr, 0);  // Reuse copy PS

			// Process RT_20 (Normal GBuffer)
			if (firstPassNormalSRV && rendererData->renderTargets[20].rtView) {
				D3DPERF_BeginEvent(0xFF00FF00, L"RestoreGBuffer_Normal_RT20");
				
				ID3D11RenderTargetView* normalRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[20].rtView;
				
				// Get dimensions for viewport
				ID3D11Texture2D* normalTex = (ID3D11Texture2D*)rendererData->renderTargets[20].texture;
				D3D11_TEXTURE2D_DESC texDesc;
				normalTex->GetDesc(&texDesc);

				D3D11_VIEWPORT vp;
				vp.Width = (float)texDesc.Width;
				vp.Height = (float)texDesc.Height;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				vp.TopLeftX = 0;
				vp.TopLeftY = 0;
				m_context->RSSetViewports(1, &vp);

				m_context->OMSetRenderTargets(1, &normalRTV, stencilDSV);
				m_context->OMSetDepthStencilState(stencilTestDSS.Get(), 127);
				m_context->PSSetShaderResources(0, 1, &firstPassNormalSRV);

				m_context->Draw(3, 0);

				ID3D11ShaderResourceView* nullSRV = nullptr;
				m_context->PSSetShaderResources(0, 1, &nullSRV);
				
				D3DPERF_EndEvent();
			}

			// Process RT_22 (Albedo GBuffer)
			if (firstPassAlbedoSRV && rendererData->renderTargets[22].rtView) {
				D3DPERF_BeginEvent(0xFF00FF00, L"RestoreGBuffer_Albedo_RT22");
				
				ID3D11RenderTargetView* albedoRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[22].rtView;
				
				// Get dimensions for viewport
				ID3D11Texture2D* albedoTex = (ID3D11Texture2D*)rendererData->renderTargets[22].texture;
				D3D11_TEXTURE2D_DESC texDesc;
				albedoTex->GetDesc(&texDesc);

				D3D11_VIEWPORT vp;
				vp.Width = (float)texDesc.Width;
				vp.Height = (float)texDesc.Height;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				vp.TopLeftX = 0;
				vp.TopLeftY = 0;
				m_context->RSSetViewports(1, &vp);

				m_context->OMSetRenderTargets(1, &albedoRTV, stencilDSV);
				m_context->OMSetDepthStencilState(stencilTestDSS.Get(), 127);
				m_context->PSSetShaderResources(0, 1, &firstPassAlbedoSRV);

				m_context->Draw(3, 0);

				ID3D11ShaderResourceView* nullSRV = nullptr;
				m_context->PSSetShaderResources(0, 1, &nullSRV);
				
				D3DPERF_EndEvent();
			}

		} catch (...) {
			logger::warn("ApplyGBufferMask: Exception");
		}

		// Restore state
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

	// ========== WriteToMVRegionOverrideMask ==========
	// Writes white pixels to fo4test's interpolation skip mask in the scope region
	// Uses the same stencil test as ApplyMotionVectorMask to identify scope pixels
	void SecondPassRenderer::WriteToMVRegionOverrideMask(ID3D11RenderTargetView* maskRTV)
	{
		if (!m_context || !m_device || !maskRTV) return;

		D3DPERF_BeginEvent(0xFFFF8800, L"WriteToMVRegionOverrideMask");

		// Get stencil DSV
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		ID3D11DepthStencilView* stencilDSV = nullptr;
		if (rendererData && rendererData->depthStencilTargets[2].dsView[0]) {
			stencilDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
		}
		if (!stencilDSV) {
			D3DPERF_EndEvent();
			return;
		}

		ID3D11PixelShader* whitePS = RenderUtilities::GetWhiteOutputPS();
		ID3D11VertexShader* scopeVS = RenderUtilities::GetFullscreenVS();
		if (!whitePS || !scopeVS) {
			D3DPERF_EndEvent();
			return;
		}

		// Backup current state
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
		m_context->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> oldDSS;
		UINT oldStencilRef;
		m_context->OMGetDepthStencilState(oldDSS.GetAddressOf(), &oldStencilRef);

		Microsoft::WRL::ComPtr<ID3D11VertexShader> oldVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> oldPS;
		m_context->VSGetShader(oldVS.GetAddressOf(), nullptr, nullptr);
		m_context->PSGetShader(oldPS.GetAddressOf(), nullptr, nullptr);

		D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		m_context->RSGetViewports(&numViewports, oldViewports);

		try {
			// Set render target to mask texture
			m_context->OMSetRenderTargets(1, &maskRTV, stencilDSV);

			// Configure stencil test: only pass where stencil == 127 (scope region)
			D3D11_DEPTH_STENCIL_DESC dssDesc;
			ZeroMemory(&dssDesc, sizeof(dssDesc));
			dssDesc.DepthEnable = FALSE;
			dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dssDesc.StencilEnable = TRUE;
			dssDesc.StencilReadMask = 0xFF;
			dssDesc.StencilWriteMask = 0x00;
			dssDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			dssDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			dssDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dssDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			dssDesc.BackFace = dssDesc.FrontFace;

			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> stencilDSS;
			HRESULT hr = m_device->CreateDepthStencilState(&dssDesc, stencilDSS.GetAddressOf());
			if (SUCCEEDED(hr)) {
				m_context->OMSetDepthStencilState(stencilDSS.Get(), 127);
			}

			// Get texture size for viewport
			ID3D11Texture2D* maskTex = nullptr;
			maskRTV->GetResource((ID3D11Resource**)&maskTex);
			if (maskTex) {
				D3D11_TEXTURE2D_DESC texDesc;
				maskTex->GetDesc(&texDesc);
				maskTex->Release();

				D3D11_VIEWPORT maskViewport;
				maskViewport.TopLeftX = 0;
				maskViewport.TopLeftY = 0;
				maskViewport.Width = (float)texDesc.Width;
				maskViewport.Height = (float)texDesc.Height;
				maskViewport.MinDepth = 0.0f;
				maskViewport.MaxDepth = 1.0f;
				m_context->RSSetViewports(1, &maskViewport);
			}

			// Set shaders
			m_context->VSSetShader(scopeVS, nullptr, 0);
			m_context->PSSetShader(whitePS, nullptr, 0);
			m_context->IASetInputLayout(nullptr);
			m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Draw fullscreen triangle (stencil test filters to scope region only)
			m_context->Draw(3, 0);

			// Unbind
			m_context->PSSetShader(nullptr, nullptr, 0);
			m_context->VSSetShader(nullptr, nullptr, 0);

		} catch (...) {
			logger::error("WriteToMVRegionOverrideMask: Exception during mask write");
		}

		// Restore state
		m_context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
		m_context->OMSetDepthStencilState(oldDSS.Get(), oldStencilRef);
		m_context->VSSetShader(oldVS.Get(), nullptr, 0);
		m_context->PSSetShader(oldPS.Get(), nullptr, 0);
		m_context->RSSetViewports(numViewports, oldViewports);

		D3DPERF_EndEvent();
	}

	// ========== MV Debug Overlay ==========
	bool SecondPassRenderer::s_ShowMVDebug = false;  // 默认关闭调试
	int SecondPassRenderer::s_DebugGBufferIndex = 20;  // 默认显示 Normal GBuffer

	void SecondPassRenderer::RenderMVDebugOverlay()
	{
		if (!s_ShowMVDebug) return;

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData) return;

		ID3D11DeviceContext* context = (ID3D11DeviceContext*)rendererData->context;
		ID3D11Device* device = (ID3D11Device*)rendererData->device;
		if (!context || !device) return;

		D3DPERF_BeginEvent(0xFFFF0000, L"GBuffer_Debug_Overlay");

		try {
			// 可以切换显示不同的纹理: 20=Normal, 22=Albedo, 23=Emissive, 24=Material, 29=MotionVector
			ID3D11ShaderResourceView* gbufferSRV = (ID3D11ShaderResourceView*)rendererData->renderTargets[s_DebugGBufferIndex].srView;
			if (!gbufferSRV) {
				D3DPERF_EndEvent();
				return;
			}

			// 直接获取 backbuffer RTV (renderTargets[0] 通常是 backbuffer)
			ID3D11RenderTargetView* backbufferRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[0].rtView;
			if (!backbufferRTV) {
				// 尝试从 SwapChain 获取
				D3DPERF_EndEvent();
				return;
			}

			// 备份当前 RTV
			ID3D11RenderTargetView* currentRTV = nullptr;
			ID3D11DepthStencilView* currentDSV = nullptr;
			context->OMGetRenderTargets(1, &currentRTV, &currentDSV);

			// 备份当前状态
			Microsoft::WRL::ComPtr<ID3D11VertexShader> oldVS;
			Microsoft::WRL::ComPtr<ID3D11PixelShader> oldPS;
			Microsoft::WRL::ComPtr<ID3D11BlendState> oldBS;
			FLOAT oldBlendFactor[4];
			UINT oldSampleMask;
			D3D11_PRIMITIVE_TOPOLOGY oldTopology;
			Microsoft::WRL::ComPtr<ID3D11InputLayout> oldInputLayout;

			context->VSGetShader(oldVS.GetAddressOf(), nullptr, nullptr);
			context->PSGetShader(oldPS.GetAddressOf(), nullptr, nullptr);
			context->OMGetBlendState(oldBS.GetAddressOf(), oldBlendFactor, &oldSampleMask);
			context->IAGetPrimitiveTopology(&oldTopology);
			context->IAGetInputLayout(oldInputLayout.GetAddressOf());

			// 使用简单的全屏三角形 shader 渲染 GBuffer 纹理
			// 这里我们直接绘制到一个小的 viewport

			// 设置小的 viewport (右上角 1/4 屏幕)
			D3D11_VIEWPORT oldViewport;
			UINT numViewports = 1;
			context->RSGetViewports(&numViewports, &oldViewport);

			D3D11_VIEWPORT debugViewport;
			debugViewport.TopLeftX = oldViewport.Width * 0.75f;
			debugViewport.TopLeftY = 0;
			debugViewport.Width = oldViewport.Width * 0.25f;
			debugViewport.Height = oldViewport.Height * 0.25f;
			debugViewport.MinDepth = 0.0f;
			debugViewport.MaxDepth = 1.0f;
			context->RSSetViewports(1, &debugViewport);

			// 设置 shader - 根据纹理类型选择合适的 shader
			context->VSSetShader(RenderUtilities::GetFullscreenVS(), nullptr, 0);
			
			// 选择 Pixel Shader:
			// - RT_29 MotionVector: 使用 MVDebugPS (10x amplification, color-coded)
			// - RT_23 Emissive: 使用 EmissiveDebugPS (50x amplification, tone mapping)
			// - 其他 (Normal, Albedo, Material): 使用 GBufferCopyPS (直接显示)
			ID3D11PixelShader* debugPS = nullptr;
			switch (s_DebugGBufferIndex) {
				case 29:  // MotionVector
					debugPS = RenderUtilities::GetMVDebugPS();
					break;
				case 23:  // Emissive
					debugPS = RenderUtilities::GetEmissiveDebugPS();
					break;
				default:  // Normal (20), Albedo (22), Material (24), etc.
					debugPS = RenderUtilities::GetGBufferCopyPS();
					break;
			}
			
			if (!debugPS) {
				debugPS = RenderUtilities::GetGBufferCopyPS();  // Fallback
			}
			context->PSSetShader(debugPS, nullptr, 0);

			// 绑定纹理
			context->PSSetShaderResources(0, 1, &gbufferSRV);

			// 创建 sampler (如果还没有)
			static Microsoft::WRL::ComPtr<ID3D11SamplerState> mvDebugSampler;
			if (!mvDebugSampler) {
				D3D11_SAMPLER_DESC sampDesc = {};
				sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
				sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
				sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
				sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
				device->CreateSamplerState(&sampDesc, mvDebugSampler.GetAddressOf());
			}
			context->PSSetSamplers(0, 1, mvDebugSampler.GetAddressOf());

			// 禁用混合
			static Microsoft::WRL::ComPtr<ID3D11BlendState> mvDebugBlend;
			if (!mvDebugBlend) {
				D3D11_BLEND_DESC blendDesc = {};
				blendDesc.RenderTarget[0].BlendEnable = FALSE;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				device->CreateBlendState(&blendDesc, mvDebugBlend.GetAddressOf());
			}
			float blendFactor[4] = { 0, 0, 0, 0 };
			context->OMSetBlendState(mvDebugBlend.Get(), blendFactor, 0xFFFFFFFF);

			// 设置 RTV 到 backbuffer
			context->OMSetRenderTargets(1, &backbufferRTV, nullptr);

			// 创建禁用剔除的 Rasterizer State
			static Microsoft::WRL::ComPtr<ID3D11RasterizerState> mvDebugRS;
			if (!mvDebugRS) {
				D3D11_RASTERIZER_DESC rsDesc = {};
				rsDesc.FillMode = D3D11_FILL_SOLID;
				rsDesc.CullMode = D3D11_CULL_NONE;  // 禁用剔除！
				rsDesc.FrontCounterClockwise = FALSE;
				rsDesc.DepthClipEnable = TRUE;
				device->CreateRasterizerState(&rsDesc, mvDebugRS.GetAddressOf());
			}
			
			// 备份并设置 RS
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> oldRS;
			context->RSGetState(oldRS.GetAddressOf());
			context->RSSetState(mvDebugRS.Get());

			// 绘制全屏三角形
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context->IASetInputLayout(nullptr);
			context->Draw(3, 0);
			
			// 恢复 RS
			context->RSSetState(oldRS.Get());

			// 清理 SRV 绑定
			ID3D11ShaderResourceView* nullSRV = nullptr;
			context->PSSetShaderResources(0, 1, &nullSRV);

			// 恢复状态
			context->RSSetViewports(1, &oldViewport);
			context->VSSetShader(oldVS.Get(), nullptr, 0);
			context->PSSetShader(oldPS.Get(), nullptr, 0);
			context->OMSetBlendState(oldBS.Get(), oldBlendFactor, oldSampleMask);
			context->IASetPrimitiveTopology(oldTopology);
			context->IASetInputLayout(oldInputLayout.Get());

			if (currentRTV) currentRTV->Release();
			if (currentDSV) currentDSV->Release();

		} catch (...) {
			logger::warn("RenderMVDebugOverlay: Exception");
		}

		D3DPERF_EndEvent();
	}
}
