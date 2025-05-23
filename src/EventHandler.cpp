// EventHandler.cpp
#include "EventHandler.h"

namespace ThroughScope
{
	using namespace RE;
	// Initialize static members
	EquipWatcher* EquipWatcher::s_Instance = nullptr;
	bool EquipWatcher::s_IsScopeActive = false;
	TESFormID EquipWatcher::s_EquippedWeaponFormID = 0;
	std::string EquipWatcher::s_LastAnimEvent = "";

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

	bool IsInADS(Actor* a)
	{
		return (a->gunState == GUN_STATE::kSighted || a->gunState == GUN_STATE::kFireSighted);
	}

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
			// When a weapon is equipped, we need to setup the scope quad again
			logger::info("Weapon equipped, setting up scope quad...");

			// We'll wait a few frames before creating the new scope shape
			logger::info("Marked scope quad for setup, will be created shortly");
		} else {
			// Weapon unequipped, clean up resources
			logger::info("Weapon unequipped, removing scope quad");
			s_IsScopeActive = false;
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	bool EquipWatcher::HasScope(RE::TESObjectWEAP* weapon)
	{
		if (!weapon)
			return false;

		return false;
	}
}
