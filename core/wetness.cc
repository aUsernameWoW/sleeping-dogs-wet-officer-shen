#include "wetness.hh"

#include <Windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "config.hh"
#include "footprints.hh"
#include "log.hh"
#include "scan.hh"

namespace wetness
{
	// ApplyWetnessOrSweatTask (legacy Begin 0x1403ff1d0, Update 0x140401b70, End 0x1404004f0). The task keeps
	// its ActionContext at +0x28 (context +0x10: the sim object) and its track at +0x18: mSweatLevel +0x38,
	// mWetnessLevel +0x3C, -1 = leave as is. Begin and Update copy them into the character's
	// CharacterLookComponent (+0xB8 / +0xBC); End puts sweat back to -1 if the track set it.
	//
	// They find the component by CharacterLookComponent::_TypeUID, but the player's look component isn't
	// registered under that type in its sim object's component array, so the lookup comes back empty and
	// swimming never makes the player wet. Instead of a lookup we queue the track's values per sim object and
	// let the next CharacterLookComponent::Update of that sim object's look apply them: the component knows
	// its own sim object, and no component pointer is kept across frames.
	using BeginFn = void(__fastcall*)(void* task, void* context);
	using UpdateFn = bool(__fastcall*)(void* task, float delta);
	using EndFn = void(__fastcall*)(void* task);
	static BeginFn gBegin = nullptr;
	static UpdateFn gUpdate = nullptr;
	static EndFn gEnd = nullptr;

	// void CharacterLookComponent::Update(UpdateInterface* this, float delta) (legacy 0x14058d600): `this` is
	// the component + 0x48, so the sim object is at -0x20 and sweat/wetness at +0x70/+0x74.
	using LookUpdateFn = void(__fastcall*)(void* self, float delta);
	static LookUpdateFn gLookUpdate = nullptr;

	// CharacterLookComponent::_TypeUID, read through Begin's own RIP-relative operand (for the log only).
	static const uint32_t* gLookTypeUid = nullptr;

	struct Pending
	{
		bool mSetSweat = false;
		bool mSetWetness = false;
		float mSweat = 0.0f;
		float mWetness = 0.0f;
		DWORD mQueuedAt = 0;
	};
	static SRWLOCK gPendingLock = SRWLOCK_INIT;
	static std::unordered_map<const void*, Pending> gPending; // by sim object
	static std::atomic<bool> gAnyPending{ false };

	static std::atomic<int> gLogged{ 0 };
	static constexpr int kMaxLogged = 2000;
	static std::atomic<const void*> gTrackTarget{ nullptr }; // the sim object of the last wetness track (log)
	static std::unordered_map<const void*, DWORD> gLoggedLooks; // component -> last periodic log

	template <typename T>
	static T Read(const void* p, size_t offset)
	{
		T value;
		std::memcpy(&value, static_cast<const uint8_t*>(p) + offset, sizeof(value));
		return value;
	}

	template <typename T>
	static void Write(void* p, size_t offset, T value)
	{
		std::memcpy(static_cast<uint8_t*>(p) + offset, &value, sizeof(value));
	}

	static bool MayLog()
	{
		return gLogged.fetch_add(1) < kMaxLogged;
	}

	static const void* SimOf(void* task)
	{
		const void* context = Read<const void*>(task, 0x28);
		return context ? Read<const void*>(context, 0x10) : nullptr;
	}

	static void Queue(const void* sim, const Pending& values)
	{
		AcquireSRWLockExclusive(&gPendingLock);
		Pending& pending = gPending[sim];
		if (values.mSetSweat) {
			pending.mSetSweat = true;
			pending.mSweat = values.mSweat;
		}
		if (values.mSetWetness) {
			pending.mSetWetness = true;
			pending.mWetness = values.mWetness;
		}
		pending.mQueuedAt = GetTickCount();
		gAnyPending = true;
		ReleaseSRWLockExclusive(&gPendingLock);
	}

	static void FromTrack(void* task, bool begin)
	{
		const void* sim = SimOf(task);
		const void* track = Read<const void*>(task, 0x18);
		if (!sim || !track) {
			return;
		}
		Pending values;
		values.mSweat = Read<float>(track, 0x38);
		values.mWetness = Read<float>(track, 0x3C);
		values.mSetSweat = values.mSweat > -1.0f;
		values.mSetWetness = values.mWetness > -1.0f;
		if (begin) {
			gTrackTarget = sim;
			if (gConfig.mLogWetnessTracks && MayLog()) {
				LOG("wetness track: sweat %.2f, wetness %.2f on sim object %p", values.mSweat, values.mWetness, sim);
			}
		}
		if (gConfig.mActionWetness && (values.mSetSweat || values.mSetWetness)) {
			Queue(sim, values);
		}
	}

	static void __fastcall BeginHook(void* task, void* context)
	{
		gBegin(task, context);
		FromTrack(task, true);
	}

	static bool __fastcall UpdateHook(void* task, float delta)
	{
		const bool result = gUpdate(task, delta);
		FromTrack(task, false);
		return result;
	}

	static void __fastcall EndHook(void* task)
	{
		gEnd(task);
		const void* sim = SimOf(task);
		const void* track = Read<const void*>(task, 0x18);
		if (gConfig.mActionWetness && sim && track && Read<float>(track, 0x38) > -1.0f) {
			Pending values;
			values.mSetSweat = true;
			values.mSweat = -1.0f;
			Queue(sim, values);
		}
	}

	// Once per sim object: where its look component sits in the component array (m_Components.size +0x60,
	// holders {component, type UID} of 16 bytes at +0x68, fixed table entries +0x80), i.e. why the game's
	// lookup failed. The player's is registered as 0xCC000001 (CompositeLookComponent), looked for as 0xCC000005.
	static void LogRegistration(const void* sim, const void* component)
	{
		static std::unordered_set<const void*> logged;
		if (!logged.insert(sim).second || !MayLog()) {
			return;
		}
		const uint32_t size = Read<uint32_t>(sim, 0x60);
		const auto* holders = Read<const uint8_t*>(sim, 0x68);
		int index = -1;
		uint32_t type = 0;
		for (uint32_t i = 0; holders && i < size && i < 256; ++i) {
			if (Read<const void*>(holders, i * 16) == component) {
				index = static_cast<int>(i);
				type = Read<uint32_t>(holders, i * 16 + 8);
			}
		}
		LOG("wetness: applied a track to look %p of sim object %p (flags 0x%04X): component #%d of %u (fixed table %u), registered as type "
			"0x%08X; the game looks for 0x%08X", component, sim, Read<uint16_t>(sim, 0x4C), index, size, Read<uint32_t>(sim, 0x80), type,
			gLookTypeUid ? *gLookTypeUid : 0);
	}

	static void __fastcall LookUpdateHook(void* self, float delta)
	{
		auto* bytes = static_cast<uint8_t*>(self);
		const void* sim = Read<const void*>(bytes - 0x20, 0);
		void* component = bytes - 0x48;

		if (gAnyPending.load() && sim) {
			Pending pending;
			bool found = false;
			AcquireSRWLockExclusive(&gPendingLock);
			if (auto it = gPending.find(sim); it != gPending.end()) {
				pending = it->second;
				found = true;
				gPending.erase(it);
			}
			// Tracks on objects without a look component (or that went away) must not pile up.
			const DWORD now = GetTickCount();
			for (auto it = gPending.begin(); it != gPending.end();) {
				it = now - it->second.mQueuedAt > 2000 ? gPending.erase(it) : std::next(it);
			}
			gAnyPending = !gPending.empty();
			ReleaseSRWLockExclusive(&gPendingLock);

			if (found) {
				if (pending.mSetSweat) {
					Write(bytes, 0x70, pending.mSweat);
				}
				if (pending.mSetWetness) {
					Write(bytes, 0x74, pending.mWetness);
					footprints::OnTrackApplied(sim, pending.mWetness);
				}
				LogRegistration(sim, component);
			}
		}

		gLookUpdate(self, delta);
		footprints::OnLookUpdated(sim, Read<float>(bytes, 0x74), delta);

		if (gConfig.mLogWetnessTracks && sim && sim == gTrackTarget.load()) {
			const float wetness = Read<float>(bytes, 0x74);
			DWORD& last = gLoggedLooks[component];
			const DWORD now = GetTickCount();
			if (wetness > 0.0f && now - last >= 2000 && MayLog()) {
				last = now;
				LOG("look %p (sim %p): wetness %.3f, sweat %.2f", component, sim, wetness, Read<float>(bytes, 0x70));
			}
		}
	}

	static bool Hook(const char* name, uint8_t* target, void* detour, void** original)
	{
		if (!target || MH_CreateHook(target, detour, original) != MH_OK || MH_EnableHook(target) != MH_OK) {
			LOG("wetness: %s not hooked", name);
			return false;
		}
		return true;
	}

	void Install()
	{
		if (!gConfig.mActionWetness && !gConfig.mLogWetnessTracks) {
			return;
		}

		// Whole functions: their prologues are shared with 18 other tasks.
		static constexpr char kLookup[] =
			"0F B7 51 4C 0F B7 C2 66 C1 E8 0E A8 01 74 0D 8B 15 ? ? ? ? E8 ? ? ? ? EB ? "
			"0F B7 C2 66 C1 E8 0F A8 01 74 0D 8B 15 ? ? ? ? E8 ? ? ? ? EB ? "
			"0F B7 C2 66 C1 E8 0D A8 01 74 0D 8B 15 ? ? ? ? E8 ? ? ? ? EB ? "
			"66 C1 EA 0C F6 C2 01 8B 15 ? ? ? ? 74 07 E8 ? ? ? ? EB 05 E8 ? ? ? ? 48 8B C8 48 85 C0 74 ? ";
		const std::string begin = std::string("40 53 48 83 EC 20 48 8B D9 48 89 51 28 48 8B 4A 10 48 85 C9 0F 84 ? ? ? ? ") + kLookup +
			"48 8B 43 18 F3 0F 10 0D ? ? ? ? F3 0F 10 50 38 F3 0F 10 40 3C 0F 2F D1 76 08 F3 0F 11 91 B8 00 00 00";
		const std::string update = std::string("40 53 48 83 EC 20 48 8B 41 28 48 8B D9 48 8B 48 10 48 85 C9 0F 84 ? ? ? ? ") + kLookup +
			"48 8B 43 18 F3 0F 10 0D ? ? ? ? F3 0F 10 50 38 F3 0F 10 40 3C 0F 2F D1 76 08 F3 0F 11 91 B8 00 00 00 0F 2F C1 B0 01 76 10";
		const std::string end = std::string("40 53 48 83 EC 20 48 8B 41 28 48 8B D9 48 8B 48 10 48 85 C9 0F 84 ? ? ? ? ") + kLookup +
			"48 8B 43 18 F3 0F 10 05 ? ? ? ? 0F 2F 40 38 73 0A C7 81 B8 00 00 00 00 00 80 BF";

		uint8_t* beginAt = scan::FindUnique("ApplyWetnessOrSweatTask::Begin", begin.c_str());
		uint8_t* updateAt = scan::FindUnique("ApplyWetnessOrSweatTask::Update", update.c_str());
		uint8_t* endAt = scan::FindUnique("ApplyWetnessOrSweatTask::End", end.c_str());
		uint8_t* lookUpdateAt = scan::FindUnique("CharacterLookComponent::Update",
			"40 53 48 83 EC 70 0F 29 74 24 60 0F 29 7C 24 50 44 0F 29 44 24 40 48 8B D9 44 0F 29 4C 24 30 44 0F 28 C1 "
			"44 0F 29 54 24 20 E8 ? ? ? ? 4C 8B 43 E0 F3 44 0F 10 0D ? ? ? ? 0F 57 FF F3 44 0F 10 50 34 48 8B 05 ? ? ? ? "
			"F3 45 0F 5C D1 F3 0F 10 70 70 4D 85 C0 74 6D");
		if (!beginAt || !updateAt || !endAt || !lookUpdateAt) {
			LOG("wetness: ApplyWetnessOrSweatTask or CharacterLookComponent::Update MISSING, swimming stays dry");
			return;
		}
		// Begin + 41: mov edx, dword ptr [CharacterLookComponent::_TypeUID].
		gLookTypeUid = static_cast<const uint32_t*>(scan::RipTarget(beginAt + 43));

		// The look's Update first, so no track is queued without anything applying it.
		if (Hook("CharacterLookComponent::Update", lookUpdateAt, reinterpret_cast<void*>(&LookUpdateHook), reinterpret_cast<void**>(&gLookUpdate)) &&
			Hook("ApplyWetnessOrSweatTask::Begin", beginAt, reinterpret_cast<void*>(&BeginHook), reinterpret_cast<void**>(&gBegin)) &&
			Hook("ApplyWetnessOrSweatTask::Update", updateAt, reinterpret_cast<void*>(&UpdateHook), reinterpret_cast<void**>(&gUpdate)) &&
			Hook("ApplyWetnessOrSweatTask::End", endAt, reinterpret_cast<void*>(&EndHook), reinterpret_cast<void**>(&gEnd))) {
			LOG("wetness: wetness tracks hooked (ActionWetness=%d, logging=%d)", gConfig.mActionWetness, gConfig.mLogWetnessTracks);
		}
	}
}
