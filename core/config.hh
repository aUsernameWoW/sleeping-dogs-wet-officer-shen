#pragma once

#include <string>

struct Config
{
	// Patch the wet/sweat character shaders (see dxbc::PatchWetShader).
	bool mWetLook = true;

	// Defaults for clothing and skin whose specular map carries no wet mask (dxbc::WetDefaults).
	float mShine = 0.05f;
	float mGloss = 0.10f;

	// Let the action trees' wetness tracks reach the character (wet after swimming; see core/wetness.cc).
	bool mActionWetness = true;

	// Seconds of wet footprints after a wetness track soaked a character (swimming), 0 = off; see
	// core/footprints.cc. Needs mActionWetness.
	float mWetFootprints = 25.0f;

	// Prototype, off by default until it is finished: open an umbrella held as a weapon with F7 (see
	// core/umbrella.cc).
	bool mUmbrellaPrototype = false;

	bool mLogging = true;

	// Log every ApplyWetnessOrSweatTrack the action trees run (swimming, cutscenes, pedestrians' umbrellas) and the
	// wetness of the character it was on.
	bool mLogWetnessTracks = false;
};

extern Config gConfig;

namespace config
{
	// Reads SDWet.ini from `dir`, writing a commented default first if there is none.
	void Load(const std::wstring& dir);
}
