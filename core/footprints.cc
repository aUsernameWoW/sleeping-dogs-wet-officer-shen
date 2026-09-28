#include "footprints.hh"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "config.hh"
#include "hash.hh"
#include "log.hh"
#include "scan.hh"

namespace footprints
{
	// CharacterEffectsComponent::HandleFootstep(this, foot) (legacy 0x140533e90) runs from the
	// FootStep{Left,Right}EffectTask of the walk and run animations. For every PhysicsVolumeProperties the
	// character is in (placed phantom volumes, the ground's surface, rain) it places the volume's footstep
	// effect and left/right decal, and keeps placing the decals for mFootStepDecalCountdown seconds after the
	// character left it; last it places mFootstepOverride[foot] (+0x1A4, what the scripts'
	// set_footstep_override_effect writes) unless that's -1 or the decal a volume just placed. Blood pools,
	// mud and puddles leave prints this way (5 s after), but `Water` only splashes, so climbing out of the sea
	// leaves none. The wet footprint decals exist (`PhysVol_WetFootPrints`, used by puddles and a few placed
	// 2 m patches): while a character is soaked from swimming we put them in mFootstepOverride for the length
	// of each HandleFootstep call, so nothing stays behind in the component.
	static constexpr uint32_t kWetPrint[2] = { hash::Upper32("HK_WetFootPrintLeft_Effect"), hash::Upper32("HK_WetFootPrintRight_Effect") };
	static_assert(kWetPrint[0] == 0x823906F4 && kWetPrint[1] == 0xD439BE30, "the IDs PhysVol_WetFootPrints uses");
	static constexpr uint32_t kNoEffect = 0xFFFFFFFF;

	// Field offsets, checked against HandleFootstep's own instructions in Install.
	static constexpr size_t kSimObject = 0x28;         // SimComponent::m_pSimObject
	static constexpr size_t kPhysics = 0xB0;           // mCharacterPhysicsComponent.m_pSimComponent
	static constexpr size_t kFootstepOverride = 0x1A4; // uint32_t[2]: left, right

	// CharacterLookComponent::Update dries a character at 1/60 per second when it isn't raining.
	static constexpr float kDrySecondsPerWetness = 60.0f;

	using HandleFootstepFn = void(__fastcall*)(void* self, int foot);
	using IsInWaterFn = bool(__fastcall*)(const void* physics);
	static HandleFootstepFn gHandleFootstep = nullptr;
	static IsInWaterFn gIsInWater = nullptr;
	static bool gInstalled = false;
	static float gSeconds = 0.0f;
	static float gMinWetness = 1.0f; // prints only while the character is wetter than this

	struct Soaked
	{
		float mDrying = 0.0f; // game seconds since the last track soaked it
		float mWetness = 1.0f;
		DWORD mLastSeen = 0;  // tick of the last look update
		int mPrints = 0;
		int mInWater = 0;     // steps in shallow water, no print
		int mTaken = 0;       // steps where a script's override was set, left alone
	};
	static SRWLOCK gLock = SRWLOCK_INIT;
	static std::unordered_map<const void*, Soaked> gSoaked; // by sim object
	static std::atomic<bool> gAny{ false };

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

	void OnTrackApplied(const void* sim, float wetness)
	{
		if (!gInstalled || !sim || wetness <= gMinWetness) {
			return;
		}
		AcquireSRWLockExclusive(&gLock);
		const auto [it, inserted] = gSoaked.try_emplace(sim);
		it->second.mDrying = 0.0f;
		it->second.mWetness = wetness;
		it->second.mLastSeen = GetTickCount();
		gAny = true;
		ReleaseSRWLockExclusive(&gLock);
		if (inserted) {
			LOG("footprints: sim object %p soaked (wetness %.2f): wet footprints for %.0f s out of the water", sim, wetness, gSeconds);
		}
	}

	void OnLookUpdated(const void* sim, float wetness, float delta)
	{
		if (!gAny.load() || !sim) {
			return;
		}
		AcquireSRWLockExclusive(&gLock);
		const DWORD now = GetTickCount();
		if (auto it = gSoaked.find(sim); it != gSoaked.end()) {
			Soaked& soaked = it->second;
			soaked.mDrying += delta;
			soaked.mWetness = wetness;
			soaked.mLastSeen = now;
			// Both limits: the rain keeps the wetness up, and a later track may set it low at once.
			if (soaked.mDrying >= gSeconds || wetness <= gMinWetness) {
				LOG("footprints: sim object %p %s after %.1f s: %d wet footprints, %d steps in water, %d steps kept a script's footprints",
					sim, wetness <= gMinWetness ? "dried off" : "time's up", soaked.mDrying, soaked.mPrints, soaked.mInWater, soaked.mTaken);
				gSoaked.erase(it);
			}
		}
		// Sim objects that went away while soaked (their address may come back as someone else).
		std::erase_if(gSoaked, [now](const auto& entry) { return now - entry.second.mLastSeen > 10000; });
		gAny = !gSoaked.empty();
		ReleaseSRWLockExclusive(&gLock);
	}

	static void __fastcall HandleFootstepHook(void* self, int foot)
	{
		if (!gAny.load() || (foot != 0 && foot != 1)) {
			gHandleFootstep(self, foot);
			return;
		}

		const void* sim = Read<const void*>(self, kSimObject);
		const void* physics = Read<const void*>(self, kPhysics);
		const bool inWater = physics && gIsInWater(physics);
		const size_t slot = kFootstepOverride + foot * sizeof(uint32_t);
		bool print = false;
		bool first = false;
		AcquireSRWLockExclusive(&gLock);
		if (auto it = gSoaked.find(sim); sim && it != gSoaked.end() && GetTickCount() - it->second.mLastSeen < 3000) {
			Soaked& soaked = it->second;
			if (inWater) {
				++soaked.mInWater;
			}
			else if (Read<uint32_t>(self, slot) != kNoEffect) {
				++soaked.mTaken;
			}
			else {
				print = true;
				first = soaked.mPrints++ == 0;
			}
		}
		ReleaseSRWLockExclusive(&gLock);

		if (!print) {
			gHandleFootstep(self, foot);
			return;
		}
		if (first) {
			LOG("footprints: first wet footprint of sim object %p", sim);
		}
		Write(self, slot, kWetPrint[foot]);
		gHandleFootstep(self, foot);
		if (Read<uint32_t>(self, slot) == kWetPrint[foot]) {
			Write(self, slot, kNoEffect);
		}
	}

	void Install()
	{
		if (gConfig.mWetFootprints <= 0.0f || !gConfig.mActionWetness) {
			LOG("footprints: off (WetFootprints=%.0f, ActionWetness=%d)", gConfig.mWetFootprints, gConfig.mActionWetness);
			return;
		}

		uint8_t* handle = scan::FindUnique("CharacterEffectsComponent::HandleFootstep",
			"48 8B C4 48 89 58 08 48 89 70 18 55 57 41 54 41 56 41 57 48 8D 68 A1");
		// Its instructions that use the fields above: mov rsi, [rdi+0B0h] (physics); IsInWater(rsi), then
		// mov rax, [rdi+28h] (sim object); mov edx, [rdi+r14*4+1A4h] (override).
		if (!handle || !scan::Matches(handle + 0x127, "48 8B B7 B0 00 00 00") ||
			!scan::Matches(handle + 0x1BF, "48 85 F6 0F 84 ? ? ? ? 48 8B CE E8 ? ? ? ? 84 C0 74 ? 48 8B 47 28") ||
			!scan::Matches(handle + 0x301, "42 8B 94 B7 A4 01 00 00 83 FA FF")) {
			LOG("footprints: HandleFootstep MISSING or not as expected, no wet footprints");
			return;
		}
		gIsInWater = reinterpret_cast<IsInWaterFn>(scan::RipTarget(handle + 0x1BF + 13));

		gSeconds = gConfig.mWetFootprints;
		gMinWetness = std::max(0.0f, 1.0f - gSeconds / kDrySecondsPerWetness);
		gInstalled = true;
		if (MH_CreateHook(handle, reinterpret_cast<void*>(&HandleFootstepHook), reinterpret_cast<void**>(&gHandleFootstep)) != MH_OK ||
			MH_EnableHook(handle) != MH_OK) {
			gInstalled = false;
			LOG("footprints: hooking HandleFootstep failed, no wet footprints");
			return;
		}
		LOG("footprints: hooked: wet footprints for %.0f s after swimming (while wetness > %.2f)", gSeconds, gMinWetness);
	}
}
