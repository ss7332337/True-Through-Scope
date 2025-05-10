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
}
