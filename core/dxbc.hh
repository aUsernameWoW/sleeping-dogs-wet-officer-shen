#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Shader bytecode (DXBC containers, Shader Model 4 token streams) as the game hands it to D3D11.
namespace dxbc
{
	// The MD5 variant D3D checks against bytes 4..19 of a container; any edit has to recompute it.
	void Checksum(const uint8_t* blob, size_t size, uint8_t out[16]);

	// What the patched shaders use where a character's specular map has no wet mask (both channels empty).
	struct WetDefaults
	{
		// texSpecular.x: wetness × this × 10 is added to the glossiness.
		float mShine = 0.05f;
		// texSpecular.z: a further, smaller gloss term.
		float mGloss = 0.10f;
	};

	enum class Result
	{
		NotWet,  // not a wet/sweat character pixel shader: use the original
		Patched, // `out` holds the patched copy
		Failed,  // looked like one but its layout was unexpected: use the original (`why` says what)
	};

	// Recognizes the wet/sweat character pixel shaders (the _WS permutations of HK_CHARACTER and
	// HK_CHARACTERNIS, which read cbSceneryInstance.Mask.y (sweat) and .z (wetness)) and returns a copy
	// that falls back to `defaults` where texSpecular.x and .z are both zero. Dry, the two channels are
	// unused, so the patch only shows while a character is wet or sweaty.
	Result PatchWetShader(const uint8_t* blob, size_t size, const WetDefaults& defaults, std::vector<uint8_t>& out, std::string& why);
}
