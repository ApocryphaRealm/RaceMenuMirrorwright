#pragma once

// Likeness - the editor window, an Apocrypha Menu Framework (or SKSE Menu Framework 3) consumer window drawn over the
// RaceSex Menu, and the mod's settings page inside AMF.

#include <string>

namespace editor
{
	// kDataLoaded: probe the framework's exports, add the window and the settings page. Safe without a framework.
	void Register();

	bool Available();    // the window exists (a framework with every export we call)
	bool IsShown();
	void SetShown(bool a_shown, const char* a_why);

	// Main thread, from the bridge's menu sink.
	void OnRaceMenuOpened();
	void OnRaceMenuClosed();

	// The switch key from the game's input (used while the window is hidden; the window reads it itself while shown).
	void OnSwitchKey(const char* a_source);

	std::string StateJson();
}
