#pragma once

// The mod's names, in ONE place (the owner, 2026-11-06: the name may change; text takes it from here). Shown text uses
// kDisplayName through the "{name}" token of the translation files (see editor::NTR); file and tool names use the rest.

namespace name
{
	inline constexpr const char* kDisplayName = "RaceMenu Mirrorwright";
	inline constexpr const char* kFileStem = "RaceMenuMirrorwright";       // DLL, INI, log, translation files
	inline constexpr const char* kIniFile = "RaceMenuMirrorwright.ini";
	inline constexpr const char* kDevBenchTool = "mirrorwright.control";
	inline constexpr const char* kSection = "RaceMenu Mirrorwright";       // the framework's menu section
}
