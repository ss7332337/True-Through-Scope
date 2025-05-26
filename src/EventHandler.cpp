// EventHandler.cpp
#include "EventHandler.h"
#include "DataPersistence.h"
#include <NiFLoader.h>

#include "ScopeCamera.h"

namespace ThroughScope
{
	using namespace RE;
	static bool isQuerySpawnNode = false;

	// Initialize static members
	EquipWatcher* EquipWatcher::s_Instance = nullptr;
	bool EquipWatcher::s_IsScopeActive = false;
	std::string EquipWatcher::s_LastAnimEvent = "";
	

	AnimationGraphEventWatcher* AnimationGraphEventWatcher::s_Instance = nullptr;
	std::unordered_map<uint64_t, AnimationGraphEventWatcher::FnProcessEvent> AnimationGraphEventWatcher::fnHash;

	NIFLoader* m_NIFLoader = NIFLoader::GetSington();

	

	template <class Ty>
	Ty SafeWrite64Function(uintptr_t addr, Ty data)
	{
		DWORD oldProtect;
		void* _d[2];
		memcpy(_d, &data, sizeof(data));
		size_t len = sizeof(_d[0]);

		VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
		Ty olddata;
		memset(&olddata, 0, sizeof(Ty));
		memcpy(&olddata, (void*)addr, len);
		memcpy((void*)addr, &_d[0], len);
		VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
		return olddata;
	}

	void CleanupScopeResources()
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


	EquipWatcher* EquipWatcher::GetSingleton()
	{
		if (!s_Instance) {
			s_Instance = new EquipWatcher();
		}
		return s_Instance;
	}

	bool EquipWatcher::Initialize()
	{
		isQuerySpawnNode = false;

		auto handler = GetSingleton();
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

				if (weaponInfo.instanceData->flags.any(WEAPON_FLAGS::kHasScope)) {
					weaponInfo.instanceData->flags.set(false, WEAPON_FLAGS::kHasScope);
				}

				isQuerySpawnNode = true;
				s_IsScopeActive = true;
				ScopeCamera::s_EquippedWeaponFormID = weapon->formID;

			} else {
				// No configuration found - do nothing as requested
				logger::info("No scope configuration found for this weapon - no action taken");
				s_IsScopeActive = false;
			}

		} else {
			// Weapon unequipped, clean up resources
			logger::info("Weapon unequipped, cleaning up scope resources");
			s_IsScopeActive = false;
			ScopeCamera::s_EquippedWeaponFormID = 0;
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

	


	bool AnimationGraphEventWatcher::Initialize()
	{
		((AnimationGraphEventWatcher*)((uint64_t)PlayerCharacter::GetSingleton() + 0x38))->RegisterForEvents();
		isQuerySpawnNode = false;
		return true;
	}

	AnimationGraphEventWatcher* AnimationGraphEventWatcher::GetSingleton()
	{
		if (!s_Instance) {
			s_Instance = new AnimationGraphEventWatcher();
		}
		return s_Instance;
	}


	RE::BSEventNotifyControl AnimationGraphEventWatcher::hkProcessEvent(RE::BSAnimationGraphEvent& evn, RE::BSTEventSource<RE::BSAnimationGraphEvent>* src)
	{
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		std::string eventName = evn.tag.data();
		// logger::info("Event Name: {}", eventName.c_str());
		// Get current weapon info including available modifications
		
		if (std::strcmp(eventName.c_str(), "weaponDraw") == 0)
		{
			CleanupScopeResources();
			auto weaponInfo = DataPersistence::GetCurrentWeaponInfo();
			if (weaponInfo.currentConfig) {
				ScopeCamera::SetupScopeForWeapon(weaponInfo);
				isQuerySpawnNode = false;
			} else {
				// No configuration found - do nothing as requested
				logger::info("No scope configuration found for this weapon - no action taken");
			}
		}
		

		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	void AnimationGraphEventWatcher::RegisterForEvents()
	{
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			AnimationGraphEventWatcher::FnProcessEvent fn = SafeWrite64Function(vtable + 0x8, &ThroughScope::AnimationGraphEventWatcher::hkProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
		}
	}

	void AnimationGraphEventWatcher::UnregisterForEvents()
	{
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable + 0x8, it->second);
		fnHash.erase(it);
	}
}
