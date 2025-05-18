#pragma once

#include <d3d11.h>
#include <MinHook.h>
#include <Windows.h>
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

namespace ThroughScope::Utilities
{
    // Hook creation helpers
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
