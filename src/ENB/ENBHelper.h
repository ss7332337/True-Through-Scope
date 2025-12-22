#pragma once

#ifndef ENB_HELPER_H
#define ENB_HELPER_H

/**
 * ENB Helper - Wrapper utilities for ENBSeries SDK integration
 * 
 * Provides a simplified C++ interface for accessing ENBSeries functionality
 * through dynamic function loading from the d3d11.dll proxy.
 */

#include <Windows.h>
#include <psapi.h>
#include "enbseries.h"

namespace ENBHelper
{
	// SDK version constants
	constexpr long SDK_VERSION_1_0 = 1000;
	constexpr long SDK_VERSION_1_01 = 1001;
	constexpr long SDK_VERSION_1_02 = 1002;

	/**
	 * C++ wrapper class for ENB SDK operations
	 * Handles dynamic function resolution and provides type-safe interface
	 */
	class ENBInterface
	{
	public:
		explicit ENBInterface(HMODULE module) : m_module(module) {}

		// Core SDK functions
		long GetSDKVersion() const
		{
			auto fn = GetFunction<_ENBGetSDKVersion>("ENBGetSDKVersion");
			return fn ? fn() : 0;
		}

		long GetVersion() const
		{
			auto fn = GetFunction<_ENBGetVersion>("ENBGetVersion");
			return fn ? fn() : 0;
		}

		long GetGameIdentifier() const
		{
			auto fn = GetFunction<_ENBGetGameIdentifier>("ENBGetGameIdentifier");
			return fn ? fn() : 0;
		}

		// Callback management
		void SetCallbackFunction(ENBCallbackFunction callback)
		{
			auto fn = GetFunction<_ENBSetCallbackFunction>("ENBSetCallbackFunction");
			if (fn) fn(callback);
		}

		// Parameter access
		bool GetParameter(const char* filename, const char* category, const char* keyname, ENBParameter* outParam)
		{
			auto fn = GetFunction<_ENBGetParameter>("ENBGetParameter");
			return fn ? fn(const_cast<char*>(filename), const_cast<char*>(category), const_cast<char*>(keyname), outParam) : false;
		}

		bool SetParameter(const char* filename, const char* category, const char* keyname, ENBParameter* inParam)
		{
			auto fn = GetFunction<_ENBSetParameter>("ENBSetParameter");
			return fn ? fn(const_cast<char*>(filename), const_cast<char*>(category), const_cast<char*>(keyname), inParam) : false;
		}

		// Advanced functions (SDK v1001+)
		ENBRenderInfo* GetRenderInfo()
		{
			auto fn = GetFunction<_ENBGetRenderInfo>("ENBGetRenderInfo");
			return fn ? fn() : nullptr;
		}

		long GetState(ENBStateType state)
		{
			auto fn = GetFunction<_ENBStateType>("ENBGetState");
			return fn ? fn(state) : 0;
		}

	private:
		HMODULE m_module;

		template<typename FuncType>
		FuncType GetFunction(const char* name) const
		{
			return reinterpret_cast<FuncType>(GetProcAddress(m_module, name));
		}
	};

	/**
	 * Locates and connects to the ENBSeries d3d11.dll
	 * Searches loaded modules for the ENB SDK export signature
	 * 
	 * @param minVersion Minimum required SDK version (default: 1001)
	 * @return Pointer to ENBInterface if found, nullptr otherwise
	 * @note Caller is responsible for deleting the returned pointer
	 */
	[[nodiscard]] inline ENBInterface* CreateInterface(long minVersion = SDK_VERSION_1_01)
	{
		HANDLE currentProcess = GetCurrentProcess();
		HMODULE moduleList[512];
		DWORD bytesNeeded = 0;

		if (!EnumProcessModules(currentProcess, moduleList, sizeof(moduleList), &bytesNeeded))
			return nullptr;

		const DWORD moduleCount = bytesNeeded / sizeof(HMODULE);

		for (DWORD i = 0; i < moduleCount; ++i)
		{
			if (!moduleList[i])
				continue;

			// Check for ENB SDK export signature
			void* sdkExport = reinterpret_cast<void*>(GetProcAddress(moduleList[i], "ENBGetSDKVersion"));
			if (!sdkExport)
				continue;

			// Found ENB module, verify version compatibility
			auto enbInterface = new ENBInterface(moduleList[i]);
			long sdkVersion = enbInterface->GetSDKVersion();

			// Check major version compatibility (1xxx series)
			if ((sdkVersion / 1000) == (minVersion / 1000) && sdkVersion >= minVersion)
			{
				return enbInterface;
			}

			delete enbInterface;
		}

		return nullptr;
	}
}

#endif // ENB_HELPER_H
