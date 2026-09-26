#pragma once

namespace wetness
{
	// Makes the action trees' ApplyWetnessOrSweatTrack (swimming, umbrellas, scripted scenes) reach the
	// character again: the game's own component lookup misses the player's CharacterLookComponent.
	// With LogWetnessTracks it also logs each track and the player's wetness afterwards.
	void Install();
}
