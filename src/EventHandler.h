// EventHandler.h
#pragma once

#include "RenderUtilities.h"
#include "ScopeCamera.h"
#include <RE/Bethesda/BSAnimationGraph.hpp>
#include <RE/Bethesda/BSTEvent.hpp>
#include <RE/Bethesda/Events.hpp>
#include <thread>
#include <atomic>
#include <mutex>

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
		static bool IsPendingSetup() { return s_Instance ? s_Instance->m_PendingSetup.load() : false; }

		virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent& a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override;
		//void SetupScopeForWeapon(const DataPersistence::WeaponInfo& weaponInfo);
	private:
		EquipWatcher() = default;
		~EquipWatcher() { 
			CancelPendingSetup();
		}

		void RegisterForEvents();
		void UnregisterForEvents();

		// Checks if the given weapon has a scope
		bool HasScope(RE::TESObjectWEAP* weapon);
		// 模型和配置相关
		RE::NiNode* LoadScopeModel(const std::string& modelName);
		//void ApplyScopeTransform(RE::NiNode* scopeNode, const DataPersistence::CameraAdjustments& adjustments);
		//void ApplyScopeSettings(const DataPersistence::ScopeConfig& config);

		// 延迟执行SetupScopeForWeapon的方法
		void DelayedSetupScopeForWeapon();
		void CancelPendingSetup();

		// Static instance
		static EquipWatcher* s_Instance;
		//static RE::NiNode* s_CurrentScopeNode;

		// State tracking
		static bool s_IsScopeActive;
		//static RE::TESFormID s_EquippedWeaponFormID;
		static std::string s_LastAnimEvent;

		// 延迟执行相关的成员变量
		std::atomic<bool> m_PendingSetup{false};
		std::atomic<bool> m_CancelPending{false};
		std::mutex m_SetupMutex;
		std::thread m_SetupThread;

		// Animation event names to track
		inline static const std::string ANIM_EVENT_WEAPON_FIRE = "weaponFire";
		inline static const std::string ANIM_EVENT_RELOAD_START = "reloadStart";
		inline static const std::string ANIM_EVENT_SCOPE_START = "sightedTransitionStart";
		inline static const std::string ANIM_EVENT_SCOPE_END = "sightedTransitionEnd";
	};

	class AnimationGraphEventWatcher
	{
	public:
		typedef RE::BSEventNotifyControl(AnimationGraphEventWatcher::* FnProcessEvent)(RE::BSAnimationGraphEvent& evn, RE::BSTEventSource<RE::BSAnimationGraphEvent>* dispatcher);
		
		// 改为静态函数
		RE::BSEventNotifyControl hkProcessEvent(RE::BSAnimationGraphEvent& evn, RE::BSTEventSource<RE::BSAnimationGraphEvent>* src);
		
		static AnimationGraphEventWatcher* GetSingleton();
		static bool Initialize();
		static void Shutdown();
		
		void RegisterForEvents();
		void UnregisterForEvents();

	protected:
		static std::unordered_map<uint64_t, FnProcessEvent> fnHash;
		static AnimationGraphEventWatcher* s_Instance; // 添加静态实例指针
	};
}
