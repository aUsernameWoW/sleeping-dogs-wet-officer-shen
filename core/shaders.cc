#include "shaders.hh"

#include <Windows.h>

#include <MinHook.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "config.hh"
#include "dxbc.hh"
#include "log.hh"
#include "scan.hh"

namespace shaders
{
	// bool Illusion::StageShader::LoadShader(StageShader* this, void* bin, uint32_t size, Shader::StageType type)
	// (legacy 0x140a1be00): Create*Shader on the device, then D3DReflect. It keeps `bin` as mRawShader.
	using LoadShaderFn = bool(__fastcall*)(void* self, const void* bin, uint32_t size, int stage);
	static LoadShaderFn gLoadShader = nullptr;

	static constexpr int kStagePixel = 1;

	static SRWLOCK gLock = SRWLOCK_INIT;
	// Patched copies by the original's checksum. Never freed: the StageShader holds on to the bytecode.
	static std::map<std::array<uint8_t, 16>, std::vector<uint8_t>> gPatched;
	static int gFailed = 0;

	static const std::vector<uint8_t>* Patched(const void* bin, uint32_t size)
	{
		if (size < 32) {
			return nullptr;
		}
		std::array<uint8_t, 16> key;
		std::memcpy(key.data(), static_cast<const uint8_t*>(bin) + 4, key.size());

		AcquireSRWLockExclusive(&gLock);
		const std::vector<uint8_t>* result = nullptr;
		if (auto it = gPatched.find(key); it != gPatched.end()) {
			result = &it->second;
		}
		else {
			std::vector<uint8_t> out;
			std::string why;
			const dxbc::WetDefaults defaults{ gConfig.mShine, gConfig.mGloss };
			switch (dxbc::PatchWetShader(static_cast<const uint8_t*>(bin), size, defaults, out, why)) {
			case dxbc::Result::Patched:
				LOG("shaders: wet/sweat pixel shader #%zu patched (%u -> %zu bytes, %s)", gPatched.size() + 1, size, out.size(), why.c_str());
				result = &(gPatched[key] = std::move(out));
				break;
			case dxbc::Result::Failed:
				++gFailed;
				LOG("shaders: wet/sweat pixel shader left as is: %s", why.c_str());
				break;
			case dxbc::Result::NotWet:
				break;
			}
		}
		ReleaseSRWLockExclusive(&gLock);
		return result;
	}

	static bool __fastcall LoadShaderHook(void* self, const void* bin, uint32_t size, int stage)
	{
		if (stage == kStagePixel && bin) {
			if (const std::vector<uint8_t>* patched = Patched(bin, size)) {
				if (gLoadShader(self, patched->data(), static_cast<uint32_t>(patched->size()), stage)) {
					return true;
				}
				LOG("shaders: D3D rejected a patched shader, loading the original");
			}
		}
		return gLoadShader(self, bin, size, stage);
	}

	void Install()
	{
		if (!gConfig.mWetLook) {
			LOG("shaders: WetLook = 0, not patching");
			return;
		}

		uint8_t* target = scan::FindUnique("Illusion::StageShader::LoadShader",
			"48 8B C4 44 89 48 20 57 41 54 41 55 41 56 41 57 48 83 EC 40 48 C7 40 C8 FE FF FF FF "
			"48 89 58 08 48 89 68 10 48 89 70 18 4D 63 E9 41 8B F8 48 8B F2 48 8B D9 E8 ? ? ? ? 41 83 FD 05 0F 87");
		if (!target) {
			LOG("shaders: LoadShader MISSING, wet look off");
			return;
		}
		if (MH_CreateHook(target, reinterpret_cast<void*>(&LoadShaderHook), reinterpret_cast<void**>(&gLoadShader)) != MH_OK ||
			MH_EnableHook(target) != MH_OK) {
			LOG("shaders: hooking LoadShader failed, wet look off");
			return;
		}
		LOG("shaders: hooked (Shine %.3f, Gloss %.3f)", gConfig.mShine, gConfig.mGloss);
	}
}
