#include "ScopeCamera.h"
#include "Utilities.h"

#include "DataPersistence.h"

#include "NiFLoader.h"

#include "D3DHooks.h"

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
	float ScopeCamera::minFov = 1;
	float ScopeCamera::maxFov = 100;
    
    bool ScopeCamera::s_OriginalFirstPerson = false;
    bool ScopeCamera::s_OriginalRenderDecals = false;
    bool ScopeCamera::s_IsRenderingForScope = false;

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
                logger::info("Created scope camera successfully");
            } else {
                logger::error("Failed to get camera root node");
            }
        } else {
            logger::error("Player character or 3D not available");
        }
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
        
        logger::info("Camera position/rotation reset");
    }

	void ScopeCamera::ApplyScopeSettings(const DataPersistence::ScopeConfig& config)
	{
		// 更新D3DHooks中的瞄准镜设置
		D3DHooks::UpdateScopeSettings(
			config.parallaxSettings.relativeFogRadius,
			config.parallaxSettings.scopeSwayAmount,
			config.parallaxSettings.maxTravel,
			config.parallaxSettings.radius);

		// 设置摄像头FOV
		ScopeCamera::SetFOVMinMax(config.scopeSettings.minFOV, config.scopeSettings.maxFOV);

		logger::info("Applied scope settings - FOV: {}-{}, Parallax: relativeFogRadius={:.3f}, scopeSwayAmount={:.3f}, maxTravel={:.3f}, radius={:.3f}",
			config.scopeSettings.minFOV, config.scopeSettings.maxFOV,
			config.parallaxSettings.relativeFogRadius, config.parallaxSettings.scopeSwayAmount,
			config.parallaxSettings.maxTravel, config.parallaxSettings.radius);
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

		logger::info("Applied transform to scope node - Pos({:.3f}, {:.3f}, {:.3f}), Rot({:.1f}, {:.1f}, {:.1f}), Scale({:.3f})",
			adjustments.deltaPosX, adjustments.deltaPosY, adjustments.deltaPosZ,
			adjustments.deltaRot[0], adjustments.deltaRot[1], adjustments.deltaRot[2],
			adjustments.deltaScale);
	}

	void ScopeCamera::SetupScopeForWeapon(const DataPersistence::WeaponInfo& weaponInfo)
	{
		auto nifLoader = NIFLoader::GetSington();
		if (!weaponInfo.currentConfig) {
			logger::warn("No configuration provided for scope setup");
			return;
		}

		const auto& config = *weaponInfo.currentConfig;
		logger::info("Setting up scope with model: {}", config.modelName);

		// 1. 加载NIF模型（如果指定了modelName）
		if (!config.modelName.empty()) {
			std::string fullPath = "Meshes\\TTS\\ScopeShape\\" + config.modelName;
			auto scopeNode = nifLoader->LoadNIF(fullPath.c_str());
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

			if (scopeNode) {
				// 应用变换
				ApplyScopeTransform(scopeNode, config.cameraAdjustments);
				logger::info("Successfully loaded and positioned scope model: {}", config.modelName);
			} else {
				logger::error("Failed to load scope model: {}", config.modelName);
			}
		}

		// 2. 应用瞄准镜设置到D3DHooks
		ApplyScopeSettings(config);
		std::string reticleFullPath = "Data\\Textures\\TTS\\Reticle\\";
		if (!config.reticleSettings.customReticlePath.empty())
		{
			reticleFullPath += config.reticleSettings.customReticlePath;
			D3DHooks::LoadAimTexture(reticleFullPath);
			D3DHooks::UpdateReticleSettings(config.reticleSettings.scale, config.reticleSettings.offsetX, config.reticleSettings.offsetY);
		}

		if (weaponInfo.instanceData->flags.any(WEAPON_FLAGS::kHasScope)) {
			weaponInfo.instanceData->flags.set(false, WEAPON_FLAGS::kHasScope);
		}

		logger::info("Scope setup completed for weapon");
	}

	int ScopeCamera::GetScopeNodeIndexCount()
	{
		if (!s_CurrentScopeNode || !s_CurrentScopeNode->GetObjectByName("TTSEffectShape"))
		{
			return -1;
		}
		return ScopeCamera::s_CurrentScopeNode->GetObjectByName("TTSEffectShape")->IsTriShape()->numTriangles * 3;
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
					logger::warn("QMW_AnimsQBZ191M_on");
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

}
