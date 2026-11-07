#pragma once

// Likeness - the bridge to RaceMenu. Everything Likeness knows about the RaceSex Menu comes through here, and only from
// what RaceMenu ships for modders (RaceMenuBase.psc: the menu root "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.",
// its RSM_* functions, the category flags, the RSM_SliderChange mod event; CharGen.psc: the preset functions) and from
// what the running movie shows when asked (rule 30). Where only the game can say (which list holds the sliders, which
// member is the value), the probe decides at run time and logs what it chose - PLAN.md marks each "confirm in the test run".
//
// Threads: the menu sink runs on the main thread; every Scaleform call runs in an SKSE UI task (never AddTask - past
// mistake 2026-10-25); the editor window draws on the render thread and only reads a Snapshot copy and queues commands.

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace bridge
{
	// RaceMenuBase.psc's documented constants.
	inline constexpr const char* kMenuRoot = "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance";
	inline constexpr const char* kBaseRoot = "_root.RaceSexMenuBaseInstance";
	inline constexpr std::uint32_t kCategoryRace = 2;

	struct Entry
	{
		std::string label;    // "text", or the first string member
		std::string numbers;  // "name=value ..." of its number members, for the discovery view
		std::vector<std::pair<std::string, double>> nums;
		std::vector<std::pair<std::string, std::string>> strs;
	};

	struct List
	{
		std::string path;
		std::uint32_t size = 0;
		std::vector<std::string> fieldNames;
		std::vector<Entry> entries;
	};

	struct Plugin
	{
		std::string name;
		std::vector<std::string> members;
	};

	struct Slider
	{
		std::string label;
		std::string callback;
		std::uint32_t list = 0;     // index into Snapshot::lists
		std::uint32_t index = 0;    // element index in that list
		std::uint32_t flag = 0;     // filterFlag: the category bits
		float value = 0.0F, min = 0.0F, max = 1.0F, step = 0.01F;
		float openValue = NAN;      // the value when the menu opened (for the changed count and reset)
		bool engine = false;        // the callback is one of the game's own RaceSex Menu handlers
	};

	struct Race
	{
		std::string label;
		std::uint32_t list = 0;
		std::uint32_t index = 0;
		std::uint32_t flag = 0;
		double raceID = -1;
		bool other = false;         // filed under another mod's category (Apprentice classes/traits) - never a race change
		std::string description;
	};

	struct Category
	{
		std::string label;
		std::uint32_t flag = 0;
	};

	struct Model
	{
		std::vector<Category> categories;  // in RaceMenu's order, Race first
		std::vector<Slider> sliders;
		std::vector<Race> races;
		int raceList = -1;
		int sliderList = -1;
		int categoryList = -1;
		std::string valueField, minField, maxField, stepField, callbackField, flagField;
		std::string itemPressOwner;        // where onItemPress was found for race picks
		std::string doneFunction;          // the AS function Done calls (empty: not found)
		std::vector<std::string> engineCallbacks;
		std::vector<std::string> baseFunctions;    // function members of the menu's base and panels clips
	};

	struct Snapshot
	{
		bool menuOpen = false;
		bool probed = false;
		std::uint32_t probeCount = 0;
		std::uint32_t nodesVisited = 0;
		std::string status;
		std::vector<List> lists;
		std::vector<Plugin> plugins;
		Model model;
		bool panelsHidden = false;
		std::uint64_t serial = 0;
	};

	inline constexpr std::uint32_t kMaxEntries = 600;

	void Install();
	bool IsRaceMenuOpen();
	bool RequestProbe();
	Snapshot Copy();
	bool CopyIfChanged(std::uint64_t& a_serial, Snapshot& a_out);

	// Commands from the window (any thread). They are applied in UI tasks; the snapshot shows the new value at once.
	void SetSlider(const std::string& a_callback, float a_value);
	void ChooseEntry(std::uint32_t a_list, std::uint32_t a_index);   // a race or another mod's choice, through onItemPress
	void SetPanelsHidden(bool a_hidden);                             // RaceMenu's own panels, while the editor is up
	bool Finish();                                                   // RaceMenu's Done; false when no Done function is known
	void ReprobeSoon(int a_delayMs);

	std::string StateJson();
	std::string ListJson(std::size_t a_index, std::size_t a_from, std::size_t a_count);
	std::string PluginsJson();
	std::string ModelJson(std::size_t a_from, std::size_t a_count);
	std::string Escape(const std::string& a_text);
}
