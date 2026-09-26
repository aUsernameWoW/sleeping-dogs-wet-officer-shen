#pragma once

namespace shaders
{
	// Hooks Illusion::StageShader::LoadShader, where the game turns shader bytecode from
	// data\shaders\shaders.temp.bin into D3D11 shaders, and swaps in patched wet/sweat pixel shaders.
	// Game-level on purpose: it runs before any D3D11 wrapper (ReShade) and needs no device hooks.
	void Install();
}
