#include "HookManager.h"
#include "Utilities.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "RenderUtilities.h"
#include "GlobalTypes.h"
#include <cmath>
#include <algorithm>

namespace ThroughScope
{
	using namespace Utilities;

	static HookManager* g_hookMgr = HookManager::GetSingleton();

	void __fastcall hkTAA(ImageSpaceEffectTemporalAA* thisPtr, BSTriShape* a_geometry, ImageSpaceEffectParam* a_param)
	{
		// 检查原始函数指针是否有效
		if (!g_hookMgr->g_TAA) {
			logger::error("g_hookMgr->g_TAA is null! Cannot call original function");
			return;
		}

		// 在执行TAA之前捕获LUT纹理
		ID3D11DeviceContext* context = d3dHooks->GetContext();
		if (context && D3DHooks::IsEnableRender()) {
			// 捕获当前绑定的LUT纹理 (t3, t4, t5, t6)
			D3DHooks::CaptureLUTTextures(context);
		}

		// 1. 先执行原本的TAA
		g_hookMgr->g_TAA(thisPtr, a_geometry, a_param);

		//if (ScopeCamera::IsRenderingForScope())
		//	return;

		if (!isScopCamReady || !isRenderReady || !D3DHooks::IsEnableRender())
			return;

		auto playerCamera = *ptr_DrawWorldCamera;
		auto scopeCamera = ScopeCamera::GetScopeCamera();

		if (!scopeCamera || !scopeCamera->parent)
			return;

		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || !rendererData->context) {
			logger::error("Renderer data or context is null");
			return;
		}

		auto RTVs = rendererData->renderTargets;
		auto RTVMain = ((ID3D11RenderTargetView*)(RTVs[4].rtView));
		auto RTVSwap = ((ID3D11RenderTargetView*)(RTVs[0].rtView));
		ID3D11RenderTargetView* mainRTV = (ID3D11RenderTargetView*)rendererData->renderTargets[4].rtView;
		ID3D11DepthStencilView* mainDSV = (ID3D11DepthStencilView*)rendererData->depthStencilTargets[2].dsView[0];
		ID3D11Texture2D* mainRTTexture = (ID3D11Texture2D*)rendererData->renderTargets[4].texture;
		ID3D11Texture2D* mainDSTexture = (ID3D11Texture2D*)rendererData->depthStencilTargets[2].texture;

		//ID3D11DeviceContext* context = (ID3D11DeviceContext*)rendererData->context;
		ID3D11Device* device = d3dHooks->GetDevice();
		ID3D11Texture2D* rtTexture2D = nullptr;
		ID3D11RenderTargetView* savedRTVs[2] = { nullptr };
		context->OMGetRenderTargets(2, savedRTVs, nullptr);

		ID3D11Resource* rtResource = nullptr;
		savedRTVs[1]->GetResource(&rtResource);
		if (rtResource != nullptr) {
			rtResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&rtTexture2D);
			rtResource->Release();  // 释放原始资源引用
		}

		D3D11_TEXTURE2D_DESC originalDesc;
		D3D11_TEXTURE2D_DESC rtTextureDesc;
		rtTexture2D->GetDesc(&originalDesc);  // 获取原纹理参数
		rtTextureDesc = originalDesc;
		rtTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// 1. 创建临时的BackBuffer纹理和SRV用于此次渲染
		ID3D11Texture2D* tempBackBufferTex = nullptr;
		ID3D11ShaderResourceView* tempBackBufferSRV = nullptr;

		HRESULT hr = device->CreateTexture2D(&rtTextureDesc, nullptr, &tempBackBufferTex);
		if (SUCCEEDED(hr)) {
			hr = device->CreateShaderResourceView(tempBackBufferTex, NULL, &tempBackBufferSRV);
			if (FAILED(hr)) {
				logger::error("Failed to create temporary BackBuffer SRV: 0x{:X}", hr);
				SAFE_RELEASE(tempBackBufferTex);
				return;
			}
		} else {
			logger::error("Failed to create temporary BackBuffer texture: 0x{:X}", hr);
			return;
		}

		// 复制当前渲染目标内容到临时BackBuffer
		context->CopyResource(tempBackBufferTex, rtTexture2D);

		if (mainRTTexture) {
			// Copy the render target to our texture
			context->CopyResource(RenderUtilities::GetFirstPassColorTexture(), mainRTTexture);
			context->CopyResource(RenderUtilities::GetFirstPassDepthTexture(), mainDSTexture);
			RenderUtilities::SetFirstPassComplete(true);
		} else {
			logger::error("Failed to find a valid render target texture");
		}

		//更新瞄具镜头
		NiCloningProcess tempP{};
		NiCamera* originalCamera = (NiCamera*)((*ptr_DrawWorldCamera)->CreateClone(tempP));

		auto originalCamera1st = *ptr_DrawWorldCamera;

		auto originalCamera1stport = (*ptr_DrawWorld1stCamera)->port;
		scopeCamera->local.translate = originalCamera->local.translate;
		scopeCamera->local.rotate = originalCamera->local.rotate;
		scopeCamera->local.translate.y += 15;

		if (scopeCamera->parent) {
			NiUpdateData parentUpdate{};
			parentUpdate.camera = scopeCamera;
			scopeCamera->parent->Update(parentUpdate);
		}

		scopeCamera->world.translate = originalCamera->world.translate;
		scopeCamera->world.rotate = originalCamera->world.rotate;
		scopeCamera->world.scale = originalCamera->world.scale;
		scopeCamera->world.translate.y += 15;

		scopeCamera->UpdateWorldBound();

		//清理主输出，准备第二次渲染
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		for (size_t i = 0; i < 4; i++)
		{
			context->ClearRenderTargetView((ID3D11RenderTargetView*)RTVs[i].rtView, clearColor);
		}

		context->ClearDepthStencilView(mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// 清理延迟渲染的G-Buffer以确保光照信息不会残留
		// G-Buffer通常包括：法线、反照率、深度等
		// 扩展清理范围，包括所有可能的光照相关缓冲区
		static const int lightingBuffers[] = { 8, 9, 10, 11, 12, 13, 14, 15 }; // G-Buffer和光照累积缓冲区
		for (int bufIdx : lightingBuffers) {
			if (bufIdx < 100 && rendererData->renderTargets[bufIdx].rtView) {
				float clearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				// 对于法线缓冲区，使用默认法线值
				if (bufIdx == 8) {
					clearValue[0] = 0.5f;
					clearValue[1] = 0.5f;
					clearValue[2] = 1.0f;
					clearValue[3] = 1.0f;
				}
				context->ClearRenderTargetView((ID3D11RenderTargetView*)rendererData->renderTargets[bufIdx].rtView, clearValue);
			}
		}

		D3DPERF_BeginEvent(0xffffffff, L"Second Render_PreUI");
		DrawWorld::SetCamera(scopeCamera);
		DrawWorld::SetUpdateCameraFOV(true);
		DrawWorld::SetAdjusted1stPersonFOV(ScopeCamera::GetTargetFOV());
		DrawWorld::SetCameraFov(ScopeCamera::GetTargetFOV());

		// 同步BSShaderManager的相机指针，确保着色器使用正确的视图矩阵
		*ptr_BSShaderManagerSpCamera = scopeCamera;

		NiUpdateData nData;
		ScopeCamera::SetRenderingForScope(true);

		auto SpCam = *ptr_DrawWorldSpCamera;
		auto fpsCam = *ptr_DrawWorld1stCamera;
		auto DrawWorldCamera = *ptr_DrawWorldCamera;
		auto DrawWorldVisCamera = *ptr_DrawWorldVisCamera;
		// 不再使用固定的scopeFrustum，而是使用动态更新的视锥体
		// NiFrustum modifiedFrustum = scopeFrustum; // 移除这行

		*ptr_DrawWorldCamera = scopeCamera;
		*ptr_DrawWorldVisCamera = scopeCamera;

		if (scopeCamera) {
			// 使用原始相机的视锥体作为基础，然后调整FOV
			scopeCamera->viewFrustum = originalCamera->viewFrustum;

			// 调整视锥体参数以适应瞄具的FOV
			float aspectRatio = scopeCamera->viewFrustum.right / scopeCamera->viewFrustum.top;
			float targetFOV = ScopeCamera::GetTargetFOV();

			// 限制FOV范围以避免极端俯仰角下的数值不稳定
			// 当俯仰角接近±90度时，减小FOV以保持投影矩阵稳定
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

			// 保持远裁剪面距离，确保远处的光源不会被裁剪
			scopeCamera->viewFrustum.farPlane = originalCamera->viewFrustum.farPlane;
		}

		// 同步光照累积器的眼睛位置，确保全方向光源的距离判断正确
		auto pDrawWorldAccum = *ptr_DrawWorldAccum;
		auto p1stPersonAccum = *ptr_Draw1stPersonAccum;
		auto pShadowSceneNode = *ptr_DrawWorldShadowNode;

		// 保存原始的眼睛位置以便后续恢复
		NiPoint3 originalAccumEyePos;
		NiPoint3 originalShadowNodeEyePos;
		NiPoint3 original1stAccumEyePos;
		float originalFarClip = 0.0f;

		// 备份光源和阴影状态，防止第二次渲染破坏渲染信息
		BSTArray<NiPointer<BSLight>> backupLightList;
		BSTArray<NiPointer<BSLight>> backupShadowLightList;
		BSTArray<NiPointer<BSLight>> backupAmbientLightList;
		bool lightsBackedUp = false;

		// 关键：保存光源的原始世界变换，因为渲染过程可能会修改它们
		struct LightTransformBackup {
			BSLight* light;
			NiTransform originalTransform;
		};
		std::vector<LightTransformBackup> lightTransformBackups;

		// 阴影系统状态管理标志
		bool shadowStateBackedUp = false;

		if (pShadowSceneNode) {
			pShadowSceneNode->bDisableLightUpdate = true;
		}

		if (pDrawWorldAccum) {
			originalAccumEyePos = pDrawWorldAccum->kEyePosition;
			pDrawWorldAccum->kEyePosition.x = scopeCamera->world.translate.x;
			pDrawWorldAccum->kEyePosition.y = scopeCamera->world.translate.y;
			pDrawWorldAccum->kEyePosition.z = scopeCamera->world.translate.z;
			pDrawWorldAccum->ClearActivePasses(true);
			pDrawWorldAccum->ClearRenderPasses();

			// 重置太阳遮挡查询
			pDrawWorldAccum->ResetSunOcclusion();
		}
		if (p1stPersonAccum) {
			original1stAccumEyePos = p1stPersonAccum->kEyePosition;
			p1stPersonAccum->kEyePosition.x = scopeCamera->world.translate.x;
			p1stPersonAccum->kEyePosition.y = scopeCamera->world.translate.y;
			p1stPersonAccum->kEyePosition.z = scopeCamera->world.translate.z;
		}

		// 同步ShadowSceneNode的眼睛位置
		if (pShadowSceneNode) {
			originalShadowNodeEyePos = pShadowSceneNode->kEyePosition;
			pShadowSceneNode->kEyePosition.x = scopeCamera->world.translate.x;
			pShadowSceneNode->kEyePosition.y = scopeCamera->world.translate.y;
			pShadowSceneNode->kEyePosition.z = scopeCamera->world.translate.z;

			// 确保光源更新不被禁用
			pShadowSceneNode->bDisableLightUpdate = false;

			originalFarClip = pShadowSceneNode->fStoredFarClip;
			pShadowSceneNode->fStoredFarClip = scopeCamera->viewFrustum.farPlane;
			pShadowSceneNode->bAlwaysUpdateLights = true;
		}
		auto geomListCullProc0 = *DrawWorldGeomListCullProc0;
		auto geomListCullProc1 = *DrawWorldGeomListCullProc1;

		if (geomListCullProc0 && scopeCamera) {
			geomListCullProc0->m_pkCamera = scopeCamera;
			geomListCullProc0->SetFrustum(&scopeCamera->viewFrustum);
		}

		if (geomListCullProc1 && scopeCamera) {
			geomListCullProc1->m_pkCamera = scopeCamera;
			geomListCullProc1->SetFrustum(&scopeCamera->viewFrustum);
		}

		//scopeCamera->port = scopeViewPort;
		NiUpdateData updateData{};
		updateData.camera = scopeCamera;
		scopeCamera->Update(updateData);
		scopeCamera->UpdateWorldData(&updateData);
		scopeCamera->UpdateWorldBound();

		// 计算视图矩阵的逆矩阵（世界矩阵）
		NiMatrix3 viewToWorld = scopeCamera->world.rotate;
		viewToWorld.Transpose(); // 旋转矩阵的逆等于其转置

		if (pDrawWorldAccum) {
			// 设置累积器使用瞄具相机进行光照计算
			pDrawWorldAccum->StartAccumulating(scopeCamera);

			pDrawWorldAccum->m_pkCamera = scopeCamera;
		}

		etRenderMode originalRenderMode;
		bool originalZPrePass = false;
		bool originalRenderDecals = false;
		bool original1stPerson = false;

		if (pDrawWorldAccum) {
			originalRenderMode = pDrawWorldAccum->GetRenderMode();
			originalZPrePass = pDrawWorldAccum->QZPrePass();
			originalRenderDecals = pDrawWorldAccum->QRenderDecals();
			original1stPerson = pDrawWorldAccum->Q1stPerson();
			shadowStateBackedUp = true;
		}

		if (pShadowSceneNode) {
			// 备份当前的光源状态（这是第一次渲染后的正确状态）
			backupLightList = pShadowSceneNode->lLightList;
			backupShadowLightList = pShadowSceneNode->lShadowLightList;
			backupAmbientLightList = pShadowSceneNode->lAmbientLightList;
			lightsBackedUp = true;
		}

		{
			// 保存当前光源列表的状态
			auto lightCount = pShadowSceneNode->lLightList.size();
			auto shadowLightCount = pShadowSceneNode->lShadowLightList.size();
			auto ambientLightCount = pShadowSceneNode->lAmbientLightList.size();
		}

		// === 第一次渲染后保存光源状态 ===
		// 在这个时间点，第一次渲染已经完成，光源状态应该是有效的
		g_LightStateBackups.clear();

		// 保存所有光源的当前状态（第一次渲染后的状态）
		if (pShadowSceneNode) {
			g_LightStateBackups.reserve(pShadowSceneNode->lLightList.size());
			for (size_t i = 0; i < pShadowSceneNode->lLightList.size(); i++) {
				auto bsLight = pShadowSceneNode->lLightList[i];
				if (bsLight && bsLight.get()) {
					LightStateBackup backup{};
					backup.light = bsLight;
					backup.frustumCull = bsLight->usFrustumCull;
					backup.occluded = bsLight->bOccluded;
					backup.temporary = bsLight->bTemporary;
					backup.dynamic = bsLight->bDynamicLight;
					backup.lodDimmer = bsLight->fLODDimmer;
					backup.camera = bsLight->spCamera;
					backup.cullingProcess = bsLight->pCullingProcess;
					g_LightStateBackups.push_back(backup);
				}
			}
		}

		// 在第二次渲染之前应用优化的光源状态
		if (!g_LightStateBackups.empty()) {
			for (const auto& firstRenderState : g_LightStateBackups) {
				auto bsLight = firstRenderState.light.get();
				if (!bsLight) {
					continue;
				}

				if (firstRenderState.frustumCull == 0xFF || firstRenderState.frustumCull == 0xFE) {
					bsLight->usFrustumCull = firstRenderState.frustumCull;
				} else {
					bsLight->usFrustumCull = 0xFF;  // BSL_ALL
				}

				bsLight->SetOccluded(firstRenderState.occluded);
				bsLight->SetTemporary(firstRenderState.temporary);
				bsLight->SetLODFade(false);  // 暂不持久保存LODFade，保持关闭以提升可见性
				bsLight->fLODDimmer = firstRenderState.lodDimmer;
				if (bsLight->bDynamicLight != firstRenderState.dynamic) {
					bsLight->SetDynamic(firstRenderState.dynamic);
				}

				bsLight->SetAffectLand(true);
				bsLight->SetAffectWater(true);
				bsLight->SetIgnoreRoughness(false);
				bsLight->SetIgnoreRim(false);
				bsLight->SetAttenuationOnly(false);
				bsLight->spCamera = firstRenderState.camera;

				if (firstRenderState.cullingProcess) {
					bsLight->SetCullingProcess(firstRenderState.cullingProcess);
				} else if (*DrawWorldCullingProcess) {
					bsLight->SetCullingProcess(*DrawWorldCullingProcess);
				}
			}
		}

		if (pDrawWorldAccum) {
			static int debugFrameCount = 0;
			debugFrameCount++;

			// 确保眼睛位置是瞄准镜相机的位置
			if (scopeCamera) {
				NiPoint3A scopeEyePos = NiPoint3A(scopeCamera->world.translate.x, scopeCamera->world.translate.y, scopeCamera->world.translate.z);
				pDrawWorldAccum->kEyePosition = scopeEyePos;

				pDrawWorldAccum->ClearActivePasses(false);  // 不清除全部，只清除活动的pass
			}
		}

		g_hookMgr->g_RenderPreUIOriginal(savedDrawWorld);

		// 立即重新启用光源更新
		if (pShadowSceneNode) {
			pShadowSceneNode->bDisableLightUpdate = false;
		}

		// === 第二次渲染后恢复原始光源状态 ===
		if (!g_LightStateBackups.empty()) {
			for (const auto& backup : g_LightStateBackups) {
				auto bsLight = backup.light.get();
				if (!bsLight) {
					continue;
				}

				bsLight->usFrustumCull = backup.frustumCull;
				bsLight->SetOccluded(backup.occluded);
				bsLight->SetTemporary(backup.temporary);
				bsLight->fLODDimmer = backup.lodDimmer;
				bsLight->spCamera = backup.camera;
				bsLight->SetCullingProcess(backup.cullingProcess);
				if (bsLight->bDynamicLight != backup.dynamic) {
					bsLight->SetDynamic(backup.dynamic);
				}
			}
		}

		ScopeCamera::SetRenderingForScope(false);

		// 立即恢复原始的眼睛位置和状态
		// 这是关键：确保下一帧的光照计算使用正确的参考点
		if (pDrawWorldAccum) {
			pDrawWorldAccum->kEyePosition = originalAccumEyePos;
			// 注意：我们在最后的状态恢复中再次清理累积器
		}
		if (p1stPersonAccum) {
			p1stPersonAccum->kEyePosition = original1stAccumEyePos;
			p1stPersonAccum->ClearActivePasses(false);
		}
		if (pShadowSceneNode) {
			pShadowSceneNode->kEyePosition = originalShadowNodeEyePos;
			pShadowSceneNode->bDisableLightUpdate = false;

			// 恢复原始的远裁剪面和AlwaysUpdateLights设置
			pShadowSceneNode->fStoredFarClip = originalFarClip;
			pShadowSceneNode->bAlwaysUpdateLights = false;

			// 不再恢复光源列表，让引擎自己管理
			// 恢复光源列表可能导致引用计数问题
			if (false && lightsBackedUp) {
				static int frameCount1 = 0;
				frameCount1++;
				pShadowSceneNode->lLightList = backupLightList;
				pShadowSceneNode->lShadowLightList = backupShadowLightList;
				pShadowSceneNode->lAmbientLightList = backupAmbientLightList;

				// 调试：检查恢复后的光源数量
				if (frameCount1 % 30 == 0) {
					logger::debug("Restored lights: {}, shadows: {}, ambient: {}",
						pShadowSceneNode->lLightList.size(),
						pShadowSceneNode->lShadowLightList.size(),
						pShadowSceneNode->lAmbientLightList.size());
				}
			}

			// 清理光照队列，防止队列状态影响下一帧
			// 注意：我们不清空主光源列表，只清理队列
			if (pShadowSceneNode->lLightQueueAdd.size() > 0) {
				pShadowSceneNode->lLightQueueAdd.resize(0);
			}
			if (pShadowSceneNode->lLightQueueRemove.size() > 0) {
				pShadowSceneNode->lLightQueueRemove.resize(0);
			}
		}

		// 完全恢复BSShaderAccumulator的状态
		if (pDrawWorldAccum && shadowStateBackedUp) {
			// 恢复原始的渲染模式和状态
			pDrawWorldAccum->SetRenderMode(originalRenderMode);
			pDrawWorldAccum->SetZPrePass(originalZPrePass);
			pDrawWorldAccum->SetRenderDecals(originalRenderDecals);
			pDrawWorldAccum->Set1stPerson(original1stPerson);

			// 重置所有可能被第二次渲染影响的状态
			pDrawWorldAccum->ResetSunOcclusion();
			pDrawWorldAccum->ClearSunQueries();
			pDrawWorldAccum->ClearActivePasses(false);
			pDrawWorldAccum->ClearRenderPasses();
		}

		D3DPERF_EndEvent();

		context->CopyResource(RenderUtilities::GetSecondPassColorTexture(), mainRTTexture);
		context->CopyResource(rtTexture2D, tempBackBufferTex);
		RenderUtilities::SetSecondPassComplete(true);

		// 现在更新scope模型的纹理
		if (RenderUtilities::IsSecondPassComplete()) {
			// Restore the original render target content for normal display
			context->CopyResource(mainRTTexture, RenderUtilities::GetFirstPassColorTexture());
			context->CopyResource(mainDSTexture, RenderUtilities::GetFirstPassDepthTexture());
		}

		context->OMSetRenderTargets(1, &savedRTVs[1], nullptr);

		int scopeNodeIndexCount = ScopeCamera::GetScopeNodeIndexCount();
		if (scopeNodeIndexCount != -1) {
			try {
				RenderUtilities::SetRender_PreUIComplete(true);
				d3dHooks->SetScopeTexture(context);
				D3DHooks::isSelfDrawCall = true;
				context->DrawIndexed(scopeNodeIndexCount, 0, 0);
				D3DHooks::isSelfDrawCall = false;
				RenderUtilities::SetRender_PreUIComplete(false);
			} catch (...) {
				logger::error("Exception during scope content rendering");
				RenderUtilities::SetRender_PreUIComplete(false);
			}
		}

		*ptr_DrawWorldCamera = originalCamera;
		DrawWorld::SetCamera(originalCamera);
		DrawWorld::SetUpdateCameraFOV(true);

		nData.camera = originalCamera;
		originalCamera->Update(nData);

		// === 资源清理部分 ===
		// 清理临时创建的D3D资源
		SAFE_RELEASE(tempBackBufferTex);
		SAFE_RELEASE(tempBackBufferSRV);
		SAFE_RELEASE(rtTexture2D);
		SAFE_RELEASE(savedRTVs[0]);
		SAFE_RELEASE(savedRTVs[1]);

		// 清理Camera克隆
		if (originalCamera) {
			if (originalCamera->DecRefCount() == 0) {
				originalCamera->DeleteThis();
			}
		}
	}
}