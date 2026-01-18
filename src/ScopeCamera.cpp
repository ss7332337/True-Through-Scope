#include "ScopeCamera.h"
#include "Utilities.h"

#include "DataPersistence.h"

#include "NiFLoader.h"

#include "D3DHooks.h"

#include <cmath>
#include <map>

namespace ThroughScope
{
	using namespace RE;
    // Initialize static members
	NiCamera* ScopeCamera::s_ScopeCamera = nullptr;
	NiCamera* ScopeCamera::s_OriginalCamera = nullptr;
	TESFormID ScopeCamera::s_EquippedWeaponFormID = 0;
	NiNode* ScopeCamera::s_CurrentScopeNode = nullptr;

	BGSKeyword* ScopeCamera::an_45 = nullptr;
	BGSKeyword* ScopeCamera::AnimsXM2010_scopeKH45 = nullptr;
	BGSKeyword* ScopeCamera::AnimsXM2010_scopeKM = nullptr;
	BGSKeyword* ScopeCamera::AX50_toounScope_K = nullptr;
	BGSKeyword* ScopeCamera::AnimsAX50_scopeKH45 = nullptr;
	BGSKeyword* ScopeCamera::QMW_AnimsQBZ191M_on = nullptr;
	BGSKeyword* ScopeCamera::QMW_AnimsQBZ191M_off = nullptr;
	BGSKeyword* ScopeCamera::QMW_AnimsRU556M_on = nullptr;
	BGSKeyword* ScopeCamera::QMW_AnimsRU556M_off = nullptr;
	BGSKeyword* ScopeCamera::AX50_toounScope_L = nullptr;
	BGSKeyword* ScopeCamera::AnimsAX50_scopeK = nullptr;

	float ScopeCamera::s_TargetFOV = DEFAULT_FOV;
	float ScopeCamera::s_MinMagnification = 1.0f;
	float ScopeCamera::s_MaxMagnification = 6.0f;
    
    bool ScopeCamera::s_OriginalFirstPerson = false;
    bool ScopeCamera::s_OriginalRenderDecals = false;
    bool ScopeCamera::s_IsRenderingForScope = false;
	bool ScopeCamera::s_IsUserAdjustingZoomData = false;

	bool ScopeCamera::hasFirstSpawnNode = false;
	bool ScopeCamera::isDelayStarted = false;
	bool ScopeCamera::isFirstScopeRender = true;

	// 获取玩家的场景 FOV (worldFOV) 作为倍率计算的基准
	float ScopeCamera::GetBaseFOV()
	{
		const auto playerCamera = RE::PlayerCamera::GetSingleton();
		if (playerCamera) {
			// 使用 worldFOV（场景FOV），而非 firstPersonFOV（第一人称模型FOV）
			return playerCamera->worldFOV;
		}
		// 默认值，fallback
		return 70.0f;
	}


    bool ScopeCamera::Initialize()
    {
        CreateScopeCamera();

		an_45 = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("an_45d");
		AnimsXM2010_scopeKH45 = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("AnimsXM2010_scopeKH45");
		AX50_toounScope_K = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("AX50_toounScope_K");
		AnimsXM2010_scopeKM = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("AnimsXM2010_scopeKM");
		AnimsAX50_scopeKH45 = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("AnimsAX50_scopeKH45");
		AX50_toounScope_L = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("AX50_toounScope_L");
		AnimsAX50_scopeK = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("AnimsAX50_scopeK");

		QMW_AnimsQBZ191M_on = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("QMW_AnimsQBZ191M_on");
		QMW_AnimsQBZ191M_off = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("QMW_AnimsQBZ191M_off");
		QMW_AnimsRU556M_on = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("QMW_AnimsRU556M_on");
		QMW_AnimsRU556M_off = (RE::BGSKeyword*)RE::TESForm::GetFormByEditorID("QMW_AnimsRU556M_off");

        return s_ScopeCamera != nullptr;
    }

    void ScopeCamera::Shutdown()
    {
        if (s_ScopeCamera) {
            if (s_ScopeCamera->DecRefCount() == 0) {
                s_ScopeCamera->DeleteThis();
            }
            s_ScopeCamera = nullptr;
        }
    }

    void ScopeCamera::CreateScopeCamera()
    {
        // Get the player camera
        const auto playerCamera = RE::PlayerCamera::GetSingleton();

        // Create a clone of the player camera for our scope view
        s_ScopeCamera = new RE::NiCamera();
        auto playerCharacter = RE::PlayerCharacter::GetSingleton();
        
        if (playerCharacter && playerCharacter->Get3D()) {
            auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
            if (playerCamera->cameraRoot.get()) {
                playerCamera->cameraRoot.get()->AttachChild(s_ScopeCamera, false);
                s_ScopeCamera->viewFrustum = ((RE::NiCamera*)playerCamera->cameraRoot.get())->viewFrustum;
                s_ScopeCamera->port = ((RE::NiCamera*)playerCamera->cameraRoot.get())->port;
				s_TargetFOV = playerCamera->firstPersonFOV - 10; 

            } else {
                logger::error("Failed to get camera root node");
            }
        } else {
            logger::error("Player character or 3D not available");
        }
    }


	void ScopeCamera::CleanupScopeResources()
	{
		if (s_CurrentScopeNode) {
			s_CurrentScopeNode->DecRefCount();
			s_CurrentScopeNode = nullptr;  // 重要：设置为nullptr避免悬空指针
		}

		// 清理D3D相关资源
		// ThroughScope::D3DHooks::CleanupStaticResources();

	}

    void ScopeCamera::ResetCamera()
    {
        if (!s_ScopeCamera)
            return;
            
        s_ScopeCamera->local.translate = RE::NiPoint3();
        s_ScopeCamera->local.rotate.MakeIdentity();
        
        RE::NiUpdateData tempData{};
        tempData.camera = s_ScopeCamera;
        s_ScopeCamera->Update(tempData);
        

    }

	void ScopeCamera::ApplyScopeSettings(const DataPersistence::ScopeConfig* config)
	{
		if (!config) return;

		// 应用视差设置 - 基本参数
		D3DHooks::UpdateScopeParallaxSettings(
			config->parallaxSettings.parallaxStrength,
			config->parallaxSettings.exitPupilRadius,
			config->parallaxSettings.vignetteStrength,
			config->parallaxSettings.vignetteRadius
		);

		// 应用视差设置 - 高级参数
		D3DHooks::UpdateParallaxAdvancedSettings(
			config->parallaxSettings.parallaxSmoothing,
			config->parallaxSettings.exitPupilSoftness,
			config->parallaxSettings.vignetteSoftness,
			config->parallaxSettings.eyeReliefDistance,
			config->parallaxSettings.enableParallax ? 1 : 0
		);

		// 应用夜视效果设置
		D3DHooks::UpdateNightVisionSettings(
			config->scopeSettings.nightVisionIntensity,
			config->scopeSettings.nightVisionNoiseScale,
			config->scopeSettings.nightVisionNoiseAmount,
			config->scopeSettings.nightVisionGreenTint
		);



		D3DHooks::UpdateSphericalDistortionSettings(
			config->scopeSettings.sphericalDistortionStrength,
			config->scopeSettings.sphericalDistortionRadius,
			config->scopeSettings.sphericalDistortionCenterX,
			config->scopeSettings.sphericalDistortionCenterY
		);

		D3DHooks::SetEnableSphericalDistortion(config->scopeSettings.enableSphericalDistortion);
		D3DHooks::SetEnableChromaticAberration(config->scopeSettings.enableChromaticAberration);

		// 设置倍率范围
		ScopeCamera::SetMagnificationRange(config->scopeSettings.minMagnification, config->scopeSettings.maxMagnification);


	}

	void ScopeCamera::ApplyScopeTransform(RE::NiNode* scopeNode, const DataPersistence::CameraAdjustments& adjustments)
	{
		if (!scopeNode || !ScopeCamera::GetScopeCamera()) {
			logger::warn("No scope node to apply transform to");
			return;
		}

		// 应用位置偏移
		scopeNode->local.translate.x = adjustments.deltaPosX;
		scopeNode->local.translate.y = adjustments.deltaPosY;
		scopeNode->local.translate.z = adjustments.deltaPosZ;

		// 应用旋转（从度数转换为弧度并创建旋转矩阵）
		RE::NiMatrix3 rotMatrix;
		rotMatrix.MakeIdentity();

		// 分别应用XYZ旋转
		float pitchRad = adjustments.deltaRot[0] * 0.01745329251f;
		float yawRad = adjustments.deltaRot[1] * 0.01745329251f;
		float rollRad = adjustments.deltaRot[2] * 0.01745329251f;

		rotMatrix.FromEulerAnglesXYZ(pitchRad, yawRad, rollRad);
		scopeNode->local.rotate = rotMatrix;

		// 应用缩放
		scopeNode->local.scale = adjustments.deltaScale;

		// 更新变换
		RE::NiUpdateData updateData;
		updateData.camera = ScopeCamera::GetScopeCamera();
		updateData.time = 0.0f;
		updateData.flags = 0;

		scopeNode->Update(updateData);


	}

	void ScopeCamera::SetupScopeForWeapon(const DataPersistence::WeaponInfo& weaponInfo)
	{
		auto nifLoader = NIFLoader::GetSingleton();
		if (!weaponInfo.currentConfig) {
			logger::warn("No configuration provided for scope setup");
			return;
		}

		const auto& config = *weaponInfo.currentConfig;

		// Clean up potentially existing old model to prevent duplicate generation
		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			if (auto root = player->Get3D(true)) {
				if (auto weaponObj = root->GetObjectByName("Weapon")) {
					if (auto weaponNode = weaponObj->IsNode()) {
						if (auto existingNode = weaponNode->GetObjectByName("TTSNode")) {
							weaponNode->DetachChild(existingNode);
						}
					}
				}
			}
		}

		// Ensure static reference is cleared
		if (s_CurrentScopeNode) {
			s_CurrentScopeNode->DecRefCount();
			s_CurrentScopeNode = nullptr;
		}

		// 保存原始ZoomData值，用于后续恢复
		static std::map<TESFormID, BGSZoomData::Data> originalZoomDataCache;

		// 1. 加载NIF模型（如果指定了modelName）
		if (!config.modelName.empty()) {
			std::string fullPath = "Meshes\\TTS\\ScopeShape\\" + config.modelName;
			auto scopeNode = nifLoader->LoadNIF(fullPath.c_str());
			if (scopeNode) {
				// 设置节点名称，这对于后续查找至关重要
				scopeNode->name = "TTSNode";
				s_CurrentScopeNode = scopeNode;
				auto weaponnode = RE::PlayerCharacter::GetSingleton()->Get3D(true)->GetObjectByName("Weapon")->IsNode();
				weaponnode->AttachChild(scopeNode, false);
				scopeNode->local.translate = RE::NiPoint3(0, 0, 10);
				scopeNode->local.rotate.MakeIdentity();


				RE::NiUpdateData updateData{};
				updateData.camera = ScopeCamera::GetScopeCamera();
				if (updateData.camera) {
					scopeNode->Update(updateData);
					weaponnode->Update(updateData);
				}

				// 应用变换
				ApplyScopeTransform(scopeNode, config.cameraAdjustments);

			} else {
				logger::error("Failed to load scope model: {}", config.modelName);
				return; // 如果NIF加载失败，退出函数
			}
		}

		// 2. 应用瞄准镜设置到D3DHooks
		ApplyScopeSettings(&config);
		std::string reticleFullPath = "Data\\Textures\\TTS\\Reticle\\";
		if (!config.reticleSettings.customReticlePath.empty())
		{
			reticleFullPath += config.reticleSettings.customReticlePath;
			D3DHooks::LoadAimTexture(reticleFullPath);
			D3DHooks::UpdateReticleSettings(config.reticleSettings.scale, config.reticleSettings.offsetX, config.reticleSettings.offsetY);
		}


		auto weaponIns = weaponInfo.instanceData;
		if (weaponIns->flags.any(WEAPON_FLAGS::kHasScope)) {
			weaponIns->flags.set(false, WEAPON_FLAGS::kHasScope);
		}

		if (weaponIns->zoomData)
		{
			// 首次设置时，缓存原始值
			if (s_EquippedWeaponFormID != 0 && originalZoomDataCache.find(s_EquippedWeaponFormID) == originalZoomDataCache.end()) {
				originalZoomDataCache[s_EquippedWeaponFormID] = weaponIns->zoomData->zoomData;

			}

			// 应用配置的ZoomData
			weaponIns->zoomData->zoomData.fovMult = config.zoomDataSettings.fovMult;
			weaponIns->zoomData->zoomData.cameraOffset.x = config.zoomDataSettings.offsetX;
			weaponIns->zoomData->zoomData.cameraOffset.y = config.zoomDataSettings.offsetY;
			weaponIns->zoomData->zoomData.cameraOffset.z = config.zoomDataSettings.offsetZ;


		}


	}

	int ScopeCamera::GetScopeNodeIndexCount()
	{
		// 使用局部变量避免潜在的竞态条件
		auto* currentNode = s_CurrentScopeNode;
		if (!currentNode) {
	
			return -1;
		}

		try {
			auto* effectShape = currentNode->GetObjectByName("TTSEffectShape");
			if (!effectShape) {
				return -1;
			}

			auto* triShape = effectShape->IsTriShape();
			if (!triShape) {
				return -1;
			}

			return triShape->numTriangles * 3;
		}
		catch (...) {
			logger::error("Exception in GetScopeNodeIndexCount - returning -1");
			return -1;
		}
	}


	bool ScopeCamera::IsSideAim()
	{
		static const BGSKeyword* sideAimKeywords[] = { an_45, AnimsXM2010_scopeKH45, AnimsXM2010_scopeKM,
			AnimsAX50_scopeKH45,
			AX50_toounScope_K, AX50_toounScope_L,
			AnimsAX50_scopeK };
		return PlayerCharacter::GetSingleton() && std::any_of(std::begin(sideAimKeywords), std::end(sideAimKeywords), [](const BGSKeyword* kw) { return kw && PlayerCharacter::GetSingleton()->HasKeyword(kw); });
	}

	BGSKeyword* ScopeCamera::IsMagnifier()
	{
		auto player = PlayerCharacter::GetSingleton();
		if (player) {
			if (QMW_AnimsQBZ191M_on) {
				if (player->HasKeyword(QMW_AnimsQBZ191M_on)) {

					return QMW_AnimsQBZ191M_on;
				}
			} else if (QMW_AnimsQBZ191M_off) {
				if (player->HasKeyword(QMW_AnimsQBZ191M_off))
					return QMW_AnimsQBZ191M_off;
			} else if (QMW_AnimsRU556M_off) {
				if (player->HasKeyword(QMW_AnimsRU556M_off))
					return QMW_AnimsRU556M_off;
			} else if (QMW_AnimsRU556M_on) {
				if (player->HasKeyword(QMW_AnimsRU556M_on))
					return QMW_AnimsRU556M_on;
			}
		}
		return nullptr;
	}

	void ScopeCamera::RestoreZoomDataForCurrentWeapon()
	{
		// 如果用户正在调整ZoomData，不进行恢复
		if (s_IsUserAdjustingZoomData) {
			return;
		}

		// 获取当前武器信息
		auto weaponInfo = DataPersistence::GetCurrentWeaponInfo();
		if (!weaponInfo.currentConfig || !weaponInfo.instanceData || !weaponInfo.instanceData->zoomData) {
			return;
		}

		const auto& config = *weaponInfo.currentConfig;
		auto weaponIns = weaponInfo.instanceData;

		// 缓存上一次的值
		static std::map<TESFormID, BGSZoomData::Data> lastKnownValues;
		static std::map<TESFormID, int> skipCount;  // 用于跳过初始几帧

		// 保存当前实际值
		auto currentZoomData = weaponIns->zoomData->zoomData;

		// 初次设置该武器
		if (s_EquippedWeaponFormID != 0 && lastKnownValues.find(s_EquippedWeaponFormID) == lastKnownValues.end()) {
			lastKnownValues[s_EquippedWeaponFormID] = currentZoomData;
			skipCount[s_EquippedWeaponFormID] = 3;  // 跳过前3帧
			return;
		}

		// 跳过初始几帧（让系统稳定）
		if (skipCount[s_EquippedWeaponFormID] > 0) {
			skipCount[s_EquippedWeaponFormID]--;
			lastKnownValues[s_EquippedWeaponFormID] = currentZoomData;
			return;
		}

		auto& lastValue = lastKnownValues[s_EquippedWeaponFormID];

		// 检查是否是游戏引擎重置了ZoomData
		// 条件1：值突然变为默认值
		bool isResetToDefault = (std::abs(currentZoomData.fovMult - 1.0f) < 0.001f &&
								 std::abs(currentZoomData.cameraOffset.x) < 0.001f &&
								 std::abs(currentZoomData.cameraOffset.y) < 0.001f &&
								 std::abs(currentZoomData.cameraOffset.z) < 0.001f);

		// 条件2：上一次的值不是默认值（说明发生了重置）
		bool lastWasNotDefault = (std::abs(lastValue.fovMult - 1.0f) > 0.001f ||
								  std::abs(lastValue.cameraOffset.x) > 0.001f ||
								  std::abs(lastValue.cameraOffset.y) > 0.001f ||
								  std::abs(lastValue.cameraOffset.z) > 0.001f);

		// 条件3：配置中的值不是默认值
		bool configIsNotDefault = (std::abs(config.zoomDataSettings.fovMult - 1.0f) > 0.001f ||
								   std::abs(config.zoomDataSettings.offsetX) > 0.001f ||
								   std::abs(config.zoomDataSettings.offsetY) > 0.001f ||
								   std::abs(config.zoomDataSettings.offsetZ) > 0.001f);

		// 只有当满足所有条件时才恢复
		if (isResetToDefault && lastWasNotDefault && configIsNotDefault) {
			// 恢复配置的ZoomData值
			weaponIns->zoomData->zoomData.fovMult = config.zoomDataSettings.fovMult;
			weaponIns->zoomData->zoomData.cameraOffset.x = config.zoomDataSettings.offsetX;
			weaponIns->zoomData->zoomData.cameraOffset.y = config.zoomDataSettings.offsetY;
			weaponIns->zoomData->zoomData.cameraOffset.z = config.zoomDataSettings.offsetZ;



			// 重置跳过计数，避免频繁检测
			skipCount[s_EquippedWeaponFormID] = 3;
		}

		// 更新缓存值
		lastKnownValues[s_EquippedWeaponFormID] = weaponIns->zoomData->zoomData;
	}

}
