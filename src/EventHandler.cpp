// EventHandler.cpp
#include "EventHandler.h"
#include "DataPersistence.h"
#include <NiFLoader.h>

#include "D3DHooks.h"

namespace ThroughScope
{
	using namespace RE;
	// Initialize static members
	EquipWatcher* EquipWatcher::s_Instance = nullptr;
	bool EquipWatcher::s_IsScopeActive = false;
	TESFormID EquipWatcher::s_EquippedWeaponFormID = 0;
	std::string EquipWatcher::s_LastAnimEvent = "";
	RE::NiNode* EquipWatcher::s_CurrentScopeNode = nullptr;

	BGSKeyword* an_45;
	BGSKeyword* AnimsXM2010_scopeKH45;
	BGSKeyword* AnimsXM2010_scopeKM;
	BGSKeyword* AX50_toounScope_K;
	BGSKeyword* AnimsAX50_scopeKH45;
	BGSKeyword* QMW_AnimsQBZ191M_on;
	BGSKeyword* QMW_AnimsQBZ191M_off;
	BGSKeyword* QMW_AnimsRU556M_on;
	BGSKeyword* QMW_AnimsRU556M_off;
	BGSKeyword* AX50_toounScope_L;
	BGSKeyword* AnimsAX50_scopeK;

	NIFLoader* m_NIFLoader = NIFLoader::GetSington();

	bool IsSideAim()
	{
		static const BGSKeyword* sideAimKeywords[] = { an_45, AnimsXM2010_scopeKH45, AnimsXM2010_scopeKM,
			AnimsAX50_scopeKH45,
			AX50_toounScope_K, AX50_toounScope_L,
			AnimsAX50_scopeK };
		return PlayerCharacter::GetSingleton() && std::any_of(std::begin(sideAimKeywords), std::end(sideAimKeywords), [](const BGSKeyword* kw) { return kw && PlayerCharacter::GetSingleton()->HasKeyword(kw); });
	}

	BGSKeyword* IsMagnifier()
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



	EquipWatcher* EquipWatcher::GetSingleton()
	{
		if (!s_Instance) {
			s_Instance = new EquipWatcher();
		}
		return s_Instance;
	}

	bool EquipWatcher::Initialize()
	{
		auto handler = GetSingleton();
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
		handler->RegisterForEvents();
		logger::info("EventHandler initialized");
		return true;
	}

	void EquipWatcher::Shutdown()
	{
		if (s_Instance) {
			s_Instance->UnregisterForEvents();
			delete s_Instance;
			s_Instance = nullptr;
		}
	}

	void EquipWatcher::RegisterForEvents()
	{
		TESEquipEvent::GetSingleton()->RegisterSink(this);
	}

	void EquipWatcher::UnregisterForEvents()
	{
		// Unregister from equip events
		auto equipEventSource = RE::TESEquipEvent::GetEventSource();
		if (equipEventSource) {
			equipEventSource->UnregisterSink(this);
			logger::info("Unregistered from TESEquipEvent");
		}
	}

	// Modify EventHandler.cpp to properly handle weapon switching

	RE::BSEventNotifyControl EquipWatcher::ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource)
	{
		// Only care about player equip events
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!a_event.actor || a_event.actor.get() != player)
			return RE::BSEventNotifyControl::kContinue;

		// Check if this is a weapon equip/unequip event
		auto baseObject = RE::TESForm::GetFormByID(a_event.baseObject);
		if (!baseObject)
			return RE::BSEventNotifyControl::kContinue;

		auto weapon = baseObject->As<RE::TESObjectWEAP>();
		if (!weapon)
			return RE::BSEventNotifyControl::kContinue;

		if (a_event.equipped) {
			// When a weapon is equipped, check for scope configuration
			logger::info("Weapon equipped: FormID {:08X}", weapon->formID);

			// Get current weapon info including available modifications
			auto weaponInfo = DataPersistence::GetCurrentWeaponInfo();

			if (weaponInfo.currentConfig) {
				// Found a configuration for this weapon or its modifications
				logger::info("Found scope configuration for weapon");
				logger::info("Config source: {}", weaponInfo.configSource);

				if (weaponInfo.selectedModForm) {
					logger::info("Config from modification: FormID {:08X}",
						weaponInfo.selectedModForm->GetLocalFormID());
				}

				// Enable scope functionality
				s_IsScopeActive = true;
				s_EquippedWeaponFormID = weapon->formID;

				// Setup scope based on configuration
				SetupScopeForWeapon(weaponInfo);

			} else {
				// No configuration found - do nothing as requested
				logger::info("No scope configuration found for this weapon - no action taken");
				s_IsScopeActive = false;
			}

		} else {
			// Weapon unequipped, clean up resources
			logger::info("Weapon unequipped, cleaning up scope resources");
			s_IsScopeActive = false;
			s_EquippedWeaponFormID = 0;
			CleanupScopeResources();
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	bool EquipWatcher::HasScope(RE::TESObjectWEAP* weapon)
	{
		if (!weapon)
			return false;

		return false;
	}

	void EquipWatcher::SetupScopeForWeapon(const DataPersistence::WeaponInfo& weaponInfo)
	{
		if (!weaponInfo.currentConfig) {
			logger::warn("No configuration provided for scope setup");
			return;
		}

		const auto& config = *weaponInfo.currentConfig;
		logger::info("Setting up scope with model: {}", config.modelName);

		// 1. 加载NIF模型（如果指定了modelName）
		if (!config.modelName.empty()) {
			CleanupScopeResources();
			std::string fullPath = "Meshes\\TTS\\ScopeShape\\" + config.modelName;
			auto scopeNode = m_NIFLoader->LoadNIF(fullPath.c_str());
			auto weaponnode = PlayerCharacter::GetSingleton()->Get3D(true)->GetObjectByName("Weapon")->IsNode();
			weaponnode->AttachChild(scopeNode, true);
			scopeNode->local.translate = NiPoint3(0, 0, 10);
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
				s_CurrentScopeNode = scopeNode;
				logger::info("Successfully loaded and positioned scope model: {}", config.modelName);
			} else {
				logger::error("Failed to load scope model: {}", config.modelName);
			}
		}

		// 2. 应用瞄准镜设置到D3DHooks
		ApplyScopeSettings(config);

		logger::info("Scope setup completed for weapon");
	}

	void EquipWatcher::CleanupScopeResources()
	{
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D()) {
			return;
		}

		auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
		if (!weaponNode || !weaponNode->IsNode()) {
			return;
		}

		auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
		auto existingTTSNode = weaponNiNode->GetObjectByName("TTSNode");

		if (existingTTSNode) {
			logger::info("Removing existing TTSNode");
			weaponNiNode->DetachChild(existingTTSNode);

			// 更新节点
			RE::NiUpdateData updateData{};
			updateData.camera = ScopeCamera::GetScopeCamera();
			if (updateData.camera) {
				weaponNiNode->Update(updateData);
			}

			logger::info("Existing TTSNode removed");
		}
		logger::info("Scope resources cleaned up");
	}

	void EquipWatcher::ApplyScopeSettings(const DataPersistence::ScopeConfig& config)
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

	void EquipWatcher::ApplyScopeTransform(RE::NiNode* scopeNode, const DataPersistence::CameraAdjustments& adjustments)
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
}
