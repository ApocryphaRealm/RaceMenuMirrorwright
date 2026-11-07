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

	// DevBench: press Done (the confirmation opens as from the button) and pick a rail entry.
	void RequestFinish();
	void SelectCategory(int a_index);
	void SelectCategoryByName(const std::string& a_name);   // applied at the next frame
	void OnConsoleClosed();
	void OnGameBox(bool a_opening);
	void OnPadZoom(const char* a_source);   // the pad's R3, from the game's input or the editor's own read   // a game MessageBoxMenu opened / closed while RaceMenu is open
	// DevBench: a slider change through the editor's own path (so it can be undone), and an answer to the open question.
	void ToolSlider(const std::string& a_key, float a_value);
	void ToolAnswer(bool a_yes);
	void NoteChosen(const std::string& a_label, const std::string& a_description, bool a_race);   // DevBench choose

	std::string StateJson();
}
