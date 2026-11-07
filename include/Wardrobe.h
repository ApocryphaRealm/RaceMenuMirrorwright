#pragma once

// Milestone 5: the undress toggle (SPEC 2.2 "Clothes") and the pose check. The editor's own, never saved:
// - Undress records every armour the player wears (object + its extra data) and unequips each, quietly, on the main thread.
// - Dress re-equips exactly that list and clears it. It also runs on Done, when the RaceSex Menu closes and on a game load,
//   so the player always leaves the menu dressed as they entered it. TNG is never touched (the owner's rule).
// - The pose check sends one animation event to the player and reports whether the graph accepted it (RaceMenu exposes no
//   pose function; this asks the game whether the menu's character animates at all).

#include <string>

namespace wardrobe
{
	void Undress();                      // any thread: queues to the main thread
	void Dress(const char* a_why);       // any thread: queues to the main thread
	bool IsUndressed();
	std::string StateJson();
	std::string PoseCheckJson(const std::string& a_event);   // DevBench thread: waits for the main thread (5 s)
}
