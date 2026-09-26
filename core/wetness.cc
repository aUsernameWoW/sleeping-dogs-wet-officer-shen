#include "wetness.hh"

#include <Windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "config.hh"
#include "log.hh"
#include "scan.hh"

namespace wetness
{
	// void ApplyWetnessOrSweatTask::Begin(ApplyWetnessOrSweatTask* this, ActionContext* context) (legacy
	// 0x1403ff1d0). The task's track (+0x18) holds mSweatLevel (+0x38) and mWetnessLevel (+0x3C), -1 = leave
	// as is; Begin copies them into the character's CharacterLookComponent (+0xB8 / +0xBC).
	using BeginFn = void(__fastcall*)(void* task, void* context);
	static BeginFn gBegin = nullptr;

	static std::atomic<int> gLogged{ 0 };
	static constexpr int kMaxLogged = 500;

	static float ReadFloat(const uint8_t* p)
	{
		float value;
		std::memcpy(&value, p, sizeof(value));
		return value;
	}

	static void __fastcall BeginHook(void* task, void* context)
	{
		gBegin(task, context);

		if (gLogged.fetch_add(1) >= kMaxLogged) {
			return;
		}
		const auto* track = *reinterpret_cast<const uint8_t* const*>(static_cast<const uint8_t*>(task) + 0x18);
		const void* simObject = context ? *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(context) + 0x10) : nullptr;
		if (track) {
			LOG("wetness track: sweat %.2f, wetness %.2f on sim object %p", ReadFloat(track + 0x38), ReadFloat(track + 0x3C), simObject);
		}
	}

	void Install()
	{
		if (!gConfig.mLogWetnessTracks) {
			return;
		}

		// The whole function: its first 30 bytes are shared by 18 other tasks' Begin.
		uint8_t* target = scan::FindUnique("ApplyWetnessOrSweatTask::Begin",
			"40 53 48 83 EC 20 48 8B D9 48 89 51 28 48 8B 4A 10 48 85 C9 0F 84 ? ? ? ? 0F B7 51 4C "
			"0F B7 C2 66 C1 E8 0E A8 01 74 0D 8B 15 ? ? ? ? E8 ? ? ? ? EB ? "
			"0F B7 C2 66 C1 E8 0F A8 01 74 0D 8B 15 ? ? ? ? E8 ? ? ? ? EB ? "
			"0F B7 C2 66 C1 E8 0D A8 01 74 0D 8B 15 ? ? ? ? E8 ? ? ? ? EB ? "
			"66 C1 EA 0C F6 C2 01 8B 15 ? ? ? ? 74 07 E8 ? ? ? ? EB 05 E8 ? ? ? ? "
			"48 8B C8 48 85 C0 74 ? 48 8B 43 18 F3 0F 10 0D ? ? ? ? F3 0F 10 50 38 F3 0F 10 40 3C "
			"0F 2F D1 76 08 F3 0F 11 91 B8 00 00 00");
		if (!target) {
			LOG("wetness: ApplyWetnessOrSweatTask::Begin MISSING, not logging wetness tracks");
			return;
		}
		if (MH_CreateHook(target, reinterpret_cast<void*>(&BeginHook), reinterpret_cast<void**>(&gBegin)) != MH_OK ||
			MH_EnableHook(target) != MH_OK) {
			LOG("wetness: hooking ApplyWetnessOrSweatTask::Begin failed");
			return;
		}
		LOG("wetness: logging wetness tracks (first %d)", kMaxLogged);
	}
}
