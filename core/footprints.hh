#pragma once

namespace footprints
{
	// Wet footprints for a while after a character climbs out of the water: the game has the decals but only
	// puddles and a few placed volumes use them. Hooks CharacterEffectsComponent::HandleFootstep.
	void Install();

	// From core/wetness.cc's CharacterLookComponent::Update hook: a wetness track (swimming) was applied to the
	// look of `sim`, and the look updated by `delta` seconds, leaving `wetness`.
	void OnTrackApplied(const void* sim, float wetness);
	void OnLookUpdated(const void* sim, float wetness, float delta);
}
