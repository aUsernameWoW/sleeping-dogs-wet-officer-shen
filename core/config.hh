#pragma once

#include <string>

struct Config
{
	// Patch the wet/sweat character shaders (see dxbc::PatchWetShader).
	bool mWetLook = true;

	// Defaults for clothing and skin whose specular map carries no wet mask (dxbc::WetDefaults).
	float mShine = 0.05f;
	float mGloss = 0.10f;

	bool mLogging = true;

	// Log every ApplyWetnessOrSweatTrack the action trees run (swimming, umbrellas, scripted scenes).
	bool mLogWetnessTracks = true;
};

extern Config gConfig;

namespace config
{
	// Reads SDWet.ini from `dir`, writing a commented default first if there is none.
	void Load(const std::wstring& dir);
}
