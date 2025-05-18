// EventHandler.h
#pragma once

#include "RenderUtilities.h"
#include "ScopeCamera.h"
#include <RE/Bethesda/BSAnimationGraph.hpp>
#include <RE/Bethesda/BSTEvent.hpp>
#include <RE/Bethesda/Events.hpp>

namespace ThroughScope
{
	class EquipWatcher :
		public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		static EquipWatcher* GetSingleton();

		// Initialize the event handler and register for events
		static bool Initialize();

		// Clean up and unregister from events
		static void Shutdown();

		// Check if a weapon scope is active
		static bool IsScopeActive() { return s_IsScopeActive; }

		// Get the equipped weapon form ID (for identifying specific weapons)
		static RE::TESFormID GetEquippedWeaponFormID() { return s_EquippedWeaponFormID; }

		// Event handling implementations
		virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override;

	private:
		EquipWatcher() = default;
		~EquipWatcher() = default;

		// Registers for needed events
		void RegisterForEvents();

		// Unregisters from all events
		void UnregisterForEvents();

		// Checks if the given weapon has a scope
		bool HasScope(RE::TESObjectWEAP* weapon);

		// Static instance
		static EquipWatcher* s_Instance;

		// State tracking
		static bool s_IsScopeActive;
		static RE::TESFormID s_EquippedWeaponFormID;
		static std::string s_LastAnimEvent;

		// Animation event names to track
		inline static const std::string ANIM_EVENT_WEAPON_FIRE = "weaponFire";
		inline static const std::string ANIM_EVENT_RELOAD_START = "reloadStart";
		inline static const std::string ANIM_EVENT_SCOPE_START = "sightedTransitionStart";
		inline static const std::string ANIM_EVENT_SCOPE_END = "sightedTransitionEnd";
	};
}
