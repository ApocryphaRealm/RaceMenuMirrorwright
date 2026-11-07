#include "PCH.h"

#include "Settings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

namespace settings
{
	namespace
	{
		std::string iniPath;

		std::string Lower(std::string a_s)
		{
			for (char& c : a_s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return a_s;
		}

		std::string Trim(const std::string& a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) { return {}; }
			const auto e = a_s.find_last_not_of(" \t\r\n");
			return a_s.substr(b, e - b + 1);
		}

		bool Flag(const std::string& a_value)
		{
			const std::string v = Lower(a_value);
			return v == "1" || v == "true" || v == "on";
		}

		// One row per key: section and key as written in the INI, and how to read / write / reset it.
		struct BoolKey { const char* section; const char* key; bool* target; bool def; };
		struct FloatKey { const char* section; const char* key; float* target; float def; float lo; float hi; };
		struct UIntKey { const char* section; const char* key; std::uint32_t* target; std::uint32_t def; std::uint32_t lo; std::uint32_t hi; bool hex; };

		const std::vector<BoolKey>& Bools()
		{
			static const std::vector<BoolKey> keys = {
				{ "General", "bEnabled", &enabled, true },
				{ "General", "bStartInEditor", &startInEditor, true },
				{ "General", "bHideRaceMenuPanels", &hideRaceMenuPanels, true },
				{ "Advanced", "bGameSliderCalls", &engineSliderCalls, true },
				{ "Layout", "bScenePanel", &scenePanel, true },
				{ "Editor", "bAskBeforeDone", &askBeforeDone, true },
				{ "Editor", "bGroupHeaders", &groupHeaders, true },
				{ "Editor", "bHideOtherBodySliders", &hideOtherBodySliders, true },
				{ "Editor", "bAllowPastRange", &allowPastRange, false },
				{ "Editor", "bShowInternalNames", &showInternalNames, false },
			};
			return keys;
		}

		const std::vector<FloatKey>& Floats()
		{
			static const std::vector<FloatKey> keys = {
				{ "Layout", "fScale", &scale, 1.0F, 0.5F, 2.0F },
				{ "Layout", "fPanelWidth", &panelWidth, 0.30F, 0.20F, 0.50F },
				{ "Camera", "fTurnSpeed", &turnSpeed, 0.35F, 0.05F, 2.0F },
			};
			return keys;
		}

		const std::vector<UIntKey>& UInts()
		{
			static const std::vector<UIntKey> keys = {
				{ "Controls", "iSwitchKey", &switchKey, 0x3E, 0, 0xFF, true },
				{ "Debug", "uLogLevel", &logLevel, 2, 0, 6, false },
			};
			return keys;
		}

		bool LoadFileValues()
		{
			std::ifstream in(iniPath);
			if (!in)
			{
				logger::warn("INI not found at {}; keeping the compiled defaults", iniPath);
				return false;
			}
			std::string line, section;
			while (std::getline(in, line))
			{
				const std::string t = Trim(line);
				if (t.empty() || t[0] == ';' || t[0] == '#') { continue; }
				if (t.front() == '[' && t.back() == ']') { section = Lower(t.substr(1, t.size() - 2)); continue; }
				const auto eq = t.find('=');
				if (eq == std::string::npos) { continue; }
				const std::string key = Lower(Trim(t.substr(0, eq)));
				std::string value = Trim(t.substr(eq + 1));
				if (const auto sc = value.find(';'); sc != std::string::npos) { value = Trim(value.substr(0, sc)); }
				bool known = false;
				try
				{
					for (const auto& k : Bools())
					{
						if (section == Lower(k.section) && key == Lower(k.key)) { *k.target = Flag(value); known = true; }
					}
					for (const auto& k : Floats())
					{
						if (section == Lower(k.section) && key == Lower(k.key)) { *k.target = std::clamp(std::stof(value), k.lo, k.hi); known = true; }
					}
					for (const auto& k : UInts())
					{
						if (section == Lower(k.section) && key == Lower(k.key))
						{
							*k.target = std::clamp(static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)), k.lo, k.hi);
							known = true;
						}
					}
				}
				catch (...)
				{
					logger::warn("INI value \"{}\" for [{}] {} is not valid; keeping the current value", value, section, key);
					continue;
				}
				if (section == "advanced" && key == "sdonefunction") { doneFunction = value; known = true; }
				if (!known) { logger::debug("INI key [{}] {} is not one of ours; left alone", section, key); }
			}
			logger::info("settings read from {}", iniPath);
			logger::info("  enabled={} startInEditor={} scale={:.2f} panelWidth={:.2f} scenePanel={}", enabled, startInEditor, scale, panelWidth, scenePanel);
			logger::info("  askBeforeDone={} groupHeaders={} hideOtherBodySliders={} allowPastRange={} showInternalNames={}", askBeforeDone, groupHeaders,
				hideOtherBodySliders, allowPastRange, showInternalNames);
			logger::info("  turnSpeed={:.2f} switchKey=0x{:02X} logLevel={} hidePanels={} gameSliderCalls={} doneFunction='{}'", turnSpeed, switchKey, logLevel,
				hideRaceMenuPanels, engineSliderCalls, doneFunction);
			return true;
		}

		// Replaces the key's value in place, so comments survive; adds the key when it is missing.
		void WriteKey(std::vector<std::string>& a_lines, const char* a_section, const char* a_key, const std::string& a_value)
		{
			const std::string wantSection = Lower(a_section);
			const std::string wantKey = Lower(a_key);
			std::string section;
			std::size_t lastInSection = std::string::npos;
			for (std::size_t i = 0; i < a_lines.size(); ++i)
			{
				const std::string t = Trim(a_lines[i]);
				if (!t.empty() && t.front() == '[' && t.back() == ']')
				{
					section = Lower(t.substr(1, t.size() - 2));
					if (section == wantSection) { lastInSection = i; }
					continue;
				}
				if (section != wantSection) { continue; }
				if (!t.empty()) { lastInSection = i; }
				const auto eq = t.find('=');
				if (eq != std::string::npos && t[0] != ';' && Lower(Trim(t.substr(0, eq))) == wantKey)
				{
					a_lines[i] = std::string(a_key) + " = " + a_value;
					return;
				}
			}
			const std::string newLine = std::string(a_key) + " = " + a_value;
			if (lastInSection != std::string::npos) { a_lines.insert(a_lines.begin() + static_cast<std::ptrdiff_t>(lastInSection + 1), newLine); }
			else
			{
				a_lines.push_back(std::string("[") + a_section + "]");
				a_lines.push_back(newLine);
			}
		}
	}

	void Init(const char* a_iniFileName)
	{
		iniPath = (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / a_iniFileName).string();
		LoadFileValues();
	}

	bool Reload()
	{
		const bool ok = LoadFileValues();
		ApplyLogLevel();
		return ok;
	}

	bool Save()
	{
		std::vector<std::string> lines;
		{
			std::ifstream in(iniPath);
			std::string line;
			while (in && std::getline(in, line)) { lines.push_back(line); }
		}
		for (const auto& k : Bools()) { WriteKey(lines, k.section, k.key, *k.target ? "1" : "0"); }
		for (const auto& k : Floats()) { WriteKey(lines, k.section, k.key, std::format("{:.2f}", *k.target)); }
		WriteKey(lines, "Advanced", "sDoneFunction", doneFunction);
		for (const auto& k : UInts()) { WriteKey(lines, k.section, k.key, k.hex ? std::format("0x{:02X}", *k.target) : std::to_string(*k.target)); }

		std::ofstream out(iniPath, std::ios::trunc);
		if (!out)
		{
			logger::error("Save: could not open {} for writing", iniPath);
			return false;
		}
		for (const auto& line : lines) { out << line << '\n'; }
		logger::info("settings saved to {}", iniPath);
		return true;
	}

	void RestoreDefaults()
	{
		for (const auto& k : Bools()) { *k.target = k.def; }
		doneFunction.clear();
		for (const auto& k : Floats()) { *k.target = k.def; }
		for (const auto& k : UInts())
		{
			if (k.target != &logLevel) { *k.target = k.def; }   // the log level is a reporter's choice, left as the INI says
		}
		logger::info("settings restored to their defaults (not saved yet)");
	}

	void ApplyLogLevel()
	{
		const auto lvl = static_cast<spdlog::level::level_enum>(std::clamp<std::uint32_t>(logLevel, 0u, 6u));
		SKSE::log::set_level(lvl, lvl);
	}

	const std::string& GetIniPath() { return iniPath; }
}
