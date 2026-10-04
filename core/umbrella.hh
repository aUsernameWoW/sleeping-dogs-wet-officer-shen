#pragma once

namespace umbrella
{
	// Open an umbrella Wei holds as a melee weapon. The game has everything but the trigger: the umbrella prop opens
	// and closes by its own action tree, and pedestrians open and carry theirs with upper body nodes in
	// GlobalActions. With an umbrella in hand, holding E (the game's Action button) plays those nodes on Wei in an
	// action controller of our own (like the game's SpawnTask), which opens the prop, unless the game has a use for
	// that E (a counter, talking, a vehicle, a taxi); F7 does the same, F9 logs the state. While it is open Wei walks
	// (a brisk walk while the sprint input is held), can't sprint, fight, change weapons or climb, E only closes it,
	// all through the game's own switches, and the umbrella in his hand ignores the rain (which would reopen it).
	// Hooks ActionTreeComponent::update (everything happens on the player's update) and IsRainingCondition::Match.
	void Install();
}
