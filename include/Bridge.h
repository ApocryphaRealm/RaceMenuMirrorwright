#pragma once

// Likeness - the bridge to RaceMenu. Everything Likeness knows about the RaceSex Menu comes through here, and only from
// what the running game exposes: the menu's open/close events and the data its movie holds (rule 30 - ask the object).
//
// Threads: the menu event sink runs on the main thread; every Scaleform read runs in an SKSE UI task (never AddTask -
// past mistake 2026-10-25); the editor window draws on the render thread and only ever reads a Snapshot copy.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace bridge
{
	struct Entry
	{
		std::string label;    // the entry's "text" (or first string member)
		std::string numbers;  // a short "name=value" list of its number members, for the discovery view
	};

	struct List
	{
		std::string path;                    // where in the movie it was found ("_root.a.b.entryList")
		std::uint32_t size = 0;              // element count reported by the movie
		std::vector<std::string> fieldNames; // members of the first element
		std::vector<Entry> entries;          // the first kMaxEntries elements
	};

	struct Plugin
	{
		std::string name;                    // a member of _global.skse.plugins
		std::vector<std::string> members;    // what that object offers
	};

	struct Snapshot
	{
		bool menuOpen = false;
		bool probed = false;        // a probe has completed since the menu opened
		std::uint32_t probeCount = 0;
		std::uint32_t nodesVisited = 0;
		std::string status;         // one line: what the last probe found or why it found nothing
		std::vector<List> lists;
		std::vector<Plugin> plugins;
		std::uint64_t serial = 0;   // bumps on every change, so a reader can tell it moved
	};

	inline constexpr std::uint32_t kMaxEntries = 400;

	// kDataLoaded: the RaceSex Menu open/close sink and the switch-key sink.
	void Install();

	bool IsRaceMenuOpen();

	// Queue one probe of the open RaceSex Menu on the UI thread (any thread may call). False if the menu is not open.
	bool RequestProbe();

	Snapshot Copy();

	// Copies only when the snapshot changed since a_serial (the render thread calls this every frame).
	bool CopyIfChanged(std::uint64_t& a_serial, Snapshot& a_out);

	// DevBench answers (JSON). StateJson is safe from any thread.
	std::string StateJson();
	std::string ListJson(std::size_t a_index, std::size_t a_from, std::size_t a_count);
	std::string PluginsJson();
	std::string Escape(const std::string& a_text);
}
