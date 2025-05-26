#pragma once

#include <d3d11.h>
#include <MinHook.h>
#include <Windows.h>
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

namespace ThroughScope::Utilities
{

	using namespace RE;


    inline bool CreateAndEnableHook(void* target, void* hook, void** original, const char* hookName)
    {
        if (MH_CreateHook(target, hook, original) != MH_OK) {
            logger::error("Failed to create {} hook", hookName);
            return false;
        }
        if (MH_EnableHook(target) != MH_OK) {
            logger::error("Failed to enable {} hook", hookName);
            return false;
        }
        return true;
    }

    // Direct3D performance markers
    inline void BeginEvent(const wchar_t* name)
    {
        D3DPERF_BeginEvent(0xffffffff, name);
    }

    inline void EndEvent()
    {
        D3DPERF_EndEvent();
    }

    // Macro for profiling function calls
    #define PROFILE_FUNCTION(func, eventName) \
        ThroughScope::Utilities::BeginEvent(eventName); \
        func; \
        ThroughScope::Utilities::EndEvent(); \
	
	#define D3DEventNode(x, y)             \
	D3DPERF_BeginEvent(0xffffffff, y); \
	x;                                 \
	D3DPERF_EndEvent();\

    // Sleep until conditions are met - for initialization
    inline void WaitForCondition(std::function<bool()> condition, int sleepTimeMs = 1000)
    {
        while (!condition()) {
            Sleep(sleepTimeMs);
        }
    }

	inline std::string PrintNodeHierarchy(RE::NiAVObject* parentNode, bool recursive = true, int maxDepth = 3, int currentDepth = 0, const std::string& prefix = "")
	{
		if (!parentNode) {
			logger::warn("PrintNodeHierarchy: Parent node is null");
			return "NULL NODE";
		}

		std::ostringstream output;

		// Print the parent node info
		std::string nodeName = parentNode->name.empty() ? "[Unnamed]" : parentNode->name.c_str();
		std::string nodeType;

		if (parentNode->IsNode()) {
			nodeType = "NiNode";
		} else if (parentNode->IsTriShape()) {
			nodeType = "TriShape";
		} else {
			nodeType = "AVObject";
		}

		output << prefix << "- " << nodeName << " (" << nodeType << ")";

		// Add position info
		output << " Pos["
			   << std::fixed << std::setprecision(2) << parentNode->local.translate.x << ", "
			   << std::fixed << std::setprecision(2) << parentNode->local.translate.y << ", "
			   << std::fixed << std::setprecision(2) << parentNode->local.translate.z << "]";

		// Cast to NiNode for children
		auto parentAsNode = parentNode->IsNode() ? static_cast<RE::NiNode*>(parentNode) : nullptr;

		// If not a node or reached max depth, don't process children
		if (!parentAsNode || currentDepth >= maxDepth || !recursive) {
			return output.str();
		}

		// Print children
		if (parentAsNode->children.size() > 0) {
			output << " - " << parentAsNode->children.size() << " children:\n";

			for (auto& childPtr : parentAsNode->children) {
				if (childPtr) {
					output << PrintNodeHierarchy(childPtr.get(), true, maxDepth, currentDepth + 1, prefix + "  ") << "\n";
				} else {
					output << prefix << "  - [NULL CHILD]\n";
				}
			}
		} else {
			output << " - No children";
		}

		return output.str();
	}


	inline bool IsInADS(RE::Actor* a)
	{
		return ((a->gunState == RE::GUN_STATE::kSighted || a->gunState == RE::GUN_STATE::kFireSighted) && !RE::UI::GetSingleton()->GetMenuOpen("ScopeMenu"));
	}


	inline RE::TESForm* GetFormFromMod(std::string modname, uint32_t formid)
	{
		if (!modname.length() || !formid)
			return nullptr;
		RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
		RE::TESFile* modFile = nullptr;
		for (auto it = dh->files.begin(); it != dh->files.end(); ++it) {
			RE::TESFile* f = *it;
			if (strcmp(f->filename, modname.c_str()) == 0) {
				modFile = f;
				break;
			}
		}
		if (!modFile)
			return nullptr;
		uint8_t modIndex = modFile->compileIndex;
		uint32_t id = formid;
		if (modIndex < 0xFE) {
			id |= ((uint32_t)modIndex) << 24;
		} else {
			uint16_t lightModIndex = modFile->smallFileCompileIndex;
			if (lightModIndex != 0xFFFF) {
				id |= 0xFE000000 | (uint32_t(lightModIndex) << 12);
			}
		}
		return RE::TESForm::GetFormByID(id);
	}

	inline const wchar_t* GetWC(const char* c)
	{
		size_t len = strlen(c) + 1;
		size_t converted = 0;
		wchar_t* WStr;
		WStr = (wchar_t*)std::malloc(len * sizeof(wchar_t));
		mbstowcs_s(&converted, WStr, len, c, _TRUNCATE);
		return WStr;
	}


	inline void LogNodeHierarchy(RE::NiAVObject* parentNode, const std::string& nodeName = "Node", bool recursive = true, int maxDepth = 3)
	{
		if (!parentNode) {
			logger::warn("LogNodeHierarchy: Parent node is null");
			return;
		}

		logger::info("===== Begin Node Hierarchy for {} =====", nodeName);
		logger::info("{}", PrintNodeHierarchy(parentNode, recursive, maxDepth));
		logger::info("===== End Node Hierarchy for {} =====", nodeName);
	}

	inline void LogPlayerWeaponNodes()
	{
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D()) {
			logger::error("LogPlayerWeaponNodes: Player character or 3D model not available");
			return;
		}

		// Find weapon node
		auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
		if (!weaponNode) {
			logger::error("LogPlayerWeaponNodes: Weapon node not found");
			return;
		}

		// Log weapon node hierarchy
		LogNodeHierarchy(weaponNode, "Player Weapon", true, 3);

		// Check if scope node exists
		auto scopeNode = weaponNode->IsNode() ?
		                     static_cast<RE::NiNode*>(weaponNode)->GetObjectByName("ScopeNode") :
		                     nullptr;

		if (scopeNode) {
			// Log just the scope node with more depth
			LogNodeHierarchy(scopeNode, "Scope Node", true, 5);
		} else {
			logger::info("No ScopeNode found under weapon node");
		}
	}

	inline void LogPlayerWeaponMods()
	{
		using namespace RE;

		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Get3D()) {
			logger::error("LogPlayerWeaponMods: Player character or 3D model not available");
			return;
		}
		if (!player->currentProcess)
			return;

		auto& eventEquipped = player->currentProcess->middleHigh->equippedItems;

		if (eventEquipped.size() > 0 && eventEquipped[0].item.instanceData && ((TESObjectWEAP::InstanceData*)eventEquipped[0].item.instanceData.get())->type == WEAPON_TYPE::kGun) {
			TESObjectWEAP* equippedWeapon = ((TESObjectWEAP*)eventEquipped[0].item.object);
			TESObjectWEAP::InstanceData* eventInstance = (TESObjectWEAP::InstanceData*)eventEquipped[0].item.instanceData.get();

			logger::info("===== Current Equipped Weapon Mods =====");
			logger::info("Weapon: {} (FormID: {:08X})", equippedWeapon->fullName.c_str(), equippedWeapon->GetFormID());

			auto invItems = player->inventoryList;
			if (!invItems) {
				logger::error("LogPlayerWeaponMods: Failed to get inventory list");
				return;
			}

			for (size_t i = 0; i < invItems->data.size(); i++) {
				auto& item = invItems->data[i];
				if (!item.object || item.object != equippedWeapon) {
					continue;
				}

				// 找到了装备的武器，检查其modifications
				if (item.stackData && item.stackData->extra) {
					auto extraDataList = item.stackData->extra.get();

					// 查找ExtraDataList中的modification数据
					auto objectInstanceExtra = extraDataList->GetByType<BGSObjectInstanceExtra>();
					if (objectInstanceExtra) {
						// 获取武器的TESObjectWEAP数据
						auto weaponForm = equippedWeapon->As<RE::TESObjectWEAP>();
						if (weaponForm && weaponForm->weaponData.equipSlot) {
							logger::info("Weapon has {} attachment slots",
								weaponForm->weaponData.equipSlot->parentSlots.size());
						}

						auto indexData = objectInstanceExtra->GetIndexData();
						if (!indexData.empty()) {
							logger::info("Weapon has {} modifications:", indexData.size());

							for (size_t j = 0; j < indexData.size(); ++j) {
								auto& modData = indexData[j];
								auto modForm = RE::TESForm::GetFormByID(modData.objectID);
								auto modFormLocal = modForm->GetLocalFormID();
								const char* modFormStr = modForm->GetFormEditorID();
								auto modFilename = modForm->GetFile()->filename;
								logger::info("  Mod[{:08X}], EditorID: {}, FileName: {}", modFormLocal, modFormStr, modFilename);
							}
						} else {
							logger::info("No modifications found in index data");
						}
					} else {
						logger::info("No instance data found for weapon");
					}
				}

				break;  // 找到武器后退出循环
			}
		}
		logger::info("===== End Weapon Mods =====");
	}


	inline RE::NiAVObject* FindNodeByName(RE::NiAVObject* parentNode, const std::string& childName, bool recursive = true)
	{
		if (!parentNode)
			return nullptr;

		// Check if this is the node we're looking for
		if (parentNode->name.c_str() == childName) {
			return parentNode;
		}

		// Get the NiNode for traversing children
		auto parentAsNode = parentNode->IsNode() ? static_cast<RE::NiNode*>(parentNode) : nullptr;
		if (!parentAsNode)
			return nullptr;

		// Check immediate children first
		for (auto& childPtr : parentAsNode->children) {
			if (childPtr && childPtr->name.c_str() == childName) {
				return childPtr.get();
			}
		}

		// If recursive, check all children's children
		if (recursive) {
			for (auto& childPtr : parentAsNode->children) {
				if (childPtr) {
					auto foundNode = FindNodeByName(childPtr.get(), childName, true);
					if (foundNode) {
						return foundNode;
					}
				}
			}
		}

		return nullptr;
	}
}
