#include <Windows.h>

#include <MinHook.h>

#include <string>

#include "core/config.hh"
#include "core/crash.hh"
#include "core/footprints.hh"
#include "core/log.hh"
#include "core/shaders.hh"
#include "core/umbrella.hh"
#include "core/wetness.hh"

static std::wstring GetModuleDirectory(HMODULE module)
{
	wchar_t path[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(module, path, ARRAYSIZE(path));
	if (length == 0 || length >= ARRAYSIZE(path)) {
		return L".";
	}

	std::wstring dir(path, length);
	const size_t slash = dir.find_last_of(L"\\/");
	return slash == std::wstring::npos ? L"." : dir.substr(0, slash);
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);

		// Pin ourselves: the hooks point into this module.
		HMODULE pinned;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&DllMain), &pinned);

		const std::wstring dir = GetModuleDirectory(module);
		config::Load(dir);

		if (gConfig.mLogging) {
			logger::Open(dir + L"\\SDWet.log");
			crash::Install(dir);
		}

		LOG("SDWet loaded (WetLook=%d Shine=%.3f Gloss=%.3f ActionWetness=%d WetFootprints=%.0f Umbrella=%d LogWetnessTracks=%d)",
			gConfig.mWetLook, gConfig.mShine, gConfig.mGloss, gConfig.mActionWetness, gConfig.mWetFootprints, gConfig.mUmbrella,
			gConfig.mLogWetnessTracks);

		// Before the game's main runs (the ASI loader loads us from dinput8.dll, a static import), so the
		// shader hook is in place before data\shaders\shaders.temp.bin is loaded.
		const MH_STATUS status = MH_Initialize();
		if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
			LOG("MinHook failed to initialize (%d), nothing hooked", status);
			return TRUE;
		}
		shaders::Install();
		wetness::Install();
		footprints::Install();
		umbrella::Install();
	}

	return TRUE;
}
