// EventHandler.h
#pragma once

#include "RenderUtilities.h"
#include "ScopeCamera.h"
#include <RE/Bethesda/BSAnimationGraph.hpp>
#include <RE/Bethesda/BSTEvent.hpp>
#include <RE/Bethesda/Events.hpp>

#include "DataPersistence.h"

namespace ThroughScope
{
	class EquipWatcher :
		public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		static EquipWatcher* GetSingleton();
		static bool Initialize();
		static void Shutdown();
		static bool IsScopeActive() { return s_IsScopeActive; }
		static RE::TESFormID GetEquippedWeaponFormID() { return s_EquippedWeaponFormID; }

		virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override;
		void SetupScopeForWeapon(const DataPersistence::WeaponInfo& weaponInfo);
		void CleanupScopeResources();

	private:
		EquipWatcher() = default;
		~EquipWatcher() = default;

		void RegisterForEvents();
		void UnregisterForEvents();

		// Checks if the given weapon has a scope
		bool HasScope(RE::TESObjectWEAP* weapon);
		// 模型和配置相关
		RE::NiNode* LoadScopeModel(const std::string& modelName);
		void ApplyScopeTransform(RE::NiNode* scopeNode, const DataPersistence::CameraAdjustments& adjustments);
		void ApplyScopeSettings(const DataPersistence::ScopeConfig& config);

		// Static instance
		static EquipWatcher* s_Instance;
		static RE::NiNode* s_CurrentScopeNode;

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
