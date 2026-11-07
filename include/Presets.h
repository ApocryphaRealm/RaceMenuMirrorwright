#pragma once

// RaceMenu presets, through CharGen.psc's documented global functions (CharGen.SavePreset / CharGen.LoadPreset, called
// on the Papyrus VM) and RaceMenu's documented folder Data\SKSE\Plugins\CharGen\Presets\<name>.jslot. The editor lists
// the folder itself (wide names - past mistake 2026-10-27, no ANSI round trip) and never reads or writes a preset's
// contents; deleting removes the one .jslot file.

#include <string>
#include <vector>

namespace presets
{
	struct Preset
	{
		std::string name;      // UTF-8, as the file is named
		std::string ansi;      // the same name in the system code page, which RaceMenu's file functions take
		bool loadable = true;  // false when the name cannot be written in the system code page
	};

	std::vector<Preset> List();             // any thread; sorted by name
	bool Save(const std::string& a_utf8);   // queues the Papyrus call; false if the name cannot be used
	bool Load(const Preset& a_preset);
	bool Remove(const Preset& a_preset);
	bool Exists(const std::string& a_utf8);
	std::string Folder();
}
