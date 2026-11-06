#include "PCH.h"

#include "Editor.h"

#include "SKSEMenuFramework.h"

#include "Bridge.h"
#include "PreciseSlider.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <atomic>
#include <chrono>
#include <format>
#include <functional>
#include <mutex>
#include <string>

// Milestone 1 of the editor: the window exists, opens over the RaceSex Menu, switches with the switch key, and shows
// what the bridge discovered in RaceMenu's movie (a rail of the lists found, the entries of the chosen one). It edits
// nothing yet - Milestone 2 adds the race grid, sliders and presets on top of the lists chosen from this view.
//
// Threads: RenderWindow / RenderSettings run on the render thread. They read the bridge's snapshot copy and plain
// atomics only - no RE::UI, no BSFixedString, no form lookups (past mistake 2026-08-29). Anything that must touch the game
// is handed to SKSE's task queue.

namespace editor
{
	namespace
	{
		SKSEMenuFramework::Model::WindowInterface* g_window = nullptr;
		std::atomic<bool> g_available{ false };
		std::atomic<std::int64_t> g_lastSwitchMs{ 0 };
		std::atomic<std::uint32_t> g_switchCount{ 0 };
		std::atomic<std::uint32_t> g_framesDrawn{ 0 };
		std::atomic<int> g_selectedList{ 0 };
		std::string g_lastWhy;                 // guarded by g_whyLock
		std::mutex g_whyLock;
		std::string g_settingsStatus;          // guarded by g_statusLock: written by main-thread tasks, read by the render thread
		std::mutex g_statusLock;

		void SetStatus(const char* a_text)
		{
			std::scoped_lock guard(g_statusLock);
			g_settingsStatus = a_text ? a_text : "";
		}

		std::string GetStatus()
		{
			std::scoped_lock guard(g_statusLock);
			return g_settingsStatus;
		}

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(std::move(a_task)); }
			else { logger::error("no SKSE task interface; a settings action was dropped"); }
		}

		// The switch key as an ImGui key, for reading it inside the window. Function keys only; another scan code is still
		// honoured by the game-input sink while the window is hidden, and logged once here.
		ImGuiMCP::ImGuiKey KeyFromScanCode(std::uint32_t a_code)
		{
			using namespace ImGuiMCP;
			switch (a_code)
			{
			case 0x3B: return ImGuiKey_F1;
			case 0x3C: return ImGuiKey_F2;
			case 0x3D: return ImGuiKey_F3;
			case 0x3E: return ImGuiKey_F4;
			case 0x3F: return ImGuiKey_F5;
			case 0x40: return ImGuiKey_F6;
			case 0x41: return ImGuiKey_F7;
			case 0x42: return ImGuiKey_F8;
			case 0x43: return ImGuiKey_F9;
			case 0x44: return ImGuiKey_F10;
			case 0x57: return ImGuiKey_F11;
			case 0x58: return ImGuiKey_F12;
			default: break;
			}
			static std::atomic<std::uint32_t> warned{ 0xFFFFFFFF };
			if (warned.exchange(a_code) != a_code && a_code != 0)
			{
				logger::warn("switch key 0x{:02X} is not a function key; inside the editor it is not read, use the button", a_code);
			}
			return ImGuiKey_None;
		}

		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddWindow", "AddSectionItem",
				"igBegin", "igEnd", "igBeginChild_Str", "igEndChild", "igSetNextWindowPos", "igSetNextWindowSize", "igGetIO",
				"igSetWindowFontScale", "igSelectable_Bool", "igTextUnformatted", "igTextV", "igTextDisabledV", "igTextWrappedV",
				"igButton", "igSameLine", "igSpacing", "igSeparator", "igSeparatorText", "igGetFrameHeightWithSpacing",
				"igIsKeyPressed_Bool", "igIsItemHovered", "igSetTooltipV", "igPushItemWidth", "igPopItemWidth",
				// precise::SliderFloat (rule 68)
				"igSliderFloat", "igIsKeyDown_Nil", "igIsMouseDown_Nil",
				// ImGuiMCP::Toggle (rule 32)
				"igGetCursorScreenPos", "igGetWindowDrawList", "igGetFrameHeight", "igInvisibleButton", "igPushID_Str", "igPopID",
				"ImDrawList_AddRectFilled", "ImDrawList_AddCircleFilled"
			};
			bool ok = true;
			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("the menu framework does not export \"{}\"", name);
					ok = false;
				}
			}
			return ok;
		}

		void HelpMarker(const char* a_text)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("%s", strings::TR("LKN_HelpMark", "(?)"));
			if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", a_text); }
		}

		// The short name of a discovered list for the rail: its last two path parts ("racePanel.entryList").
		std::string ShortName(const std::string& a_path)
		{
			const auto last = a_path.rfind('.');
			if (last == std::string::npos || last == 0) { return a_path; }
			const auto before = a_path.rfind('.', last - 1);
			return before == std::string::npos ? a_path : a_path.substr(before + 1);
		}

		void __stdcall RenderWindow()
		{
			strings::Tick();
			if (!g_window || !g_window->IsOpen) { return; }

			static bridge::Snapshot snap;
			static std::uint64_t serial = ~0ull;
			bridge::CopyIfChanged(serial, snap);
			++g_framesDrawn;

			auto* io = ImGuiMCP::GetIO();
			const float displayW = io ? io->DisplaySize.x : 1920.0F;
			const float displayH = io ? io->DisplaySize.y : 1080.0F;
			const float width = std::clamp(settings::panelWidth, 0.2F, 0.5F) * displayW;

			ImGuiMCP::SetNextWindowPos(ImGuiMCP::ImVec2(0.0F, 0.0F), ImGuiMCP::ImGuiCond_Always);
			ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2(width, displayH), ImGuiMCP::ImGuiCond_Always);
			const auto flags = ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoResize |
			                   ImGuiMCP::ImGuiWindowFlags_NoSavedSettings;
			const std::string title = std::string(strings::TR("LKN_Title", "Likeness")) + "###LikenessEditor";
			if (ImGuiMCP::Begin(title.c_str(), nullptr, flags))
			{
				ImGuiMCP::SetWindowFontScale(std::clamp(settings::scale, 0.5F, 2.0F));

				// The switch key, read here while the window has the keys.
				if (const auto key = KeyFromScanCode(settings::switchKey); key != ImGuiMCP::ImGuiKey_None && ImGuiMCP::IsKeyPressed(key, false))
				{
					OnSwitchKey("editor window");
				}

				ImGuiMCP::TextWrapped("%s", strings::TR("LKN_Milestone", "Discovery view: what Likeness reads from RaceMenu. Editing comes in the next build."));
				ImGuiMCP::TextDisabled("%s", snap.status.c_str());

				if (ImGuiMCP::Button(strings::TR("LKN_Refresh", "Read RaceMenu again")))
				{
					bridge::RequestProbe();
				}
				HelpMarker(strings::TR("LKN_HelpRefresh", "Reads RaceMenu's lists again, for example after another mod added sliders."));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(strings::TR("LKN_ToRaceMenu", "RaceMenu's interface")))
				{
					SetShown(false, "button");
				}
				HelpMarker(strings::TR("LKN_HelpToRaceMenu", "Hides Likeness and leaves RaceMenu's own interface. The switch key (F4) brings Likeness back."));
				ImGuiMCP::Separator();

				const float footer = ImGuiMCP::GetFrameHeightWithSpacing() * 2.0F;
				const float railWidth = width * 0.38F;

				if (snap.lists.empty())
				{
					ImGuiMCP::TextWrapped("%s", strings::TR("LKN_NothingYet", "No lists found in RaceMenu yet. If this stays, press Read RaceMenu again and send the log."));
				}
				else
				{
					int selected = std::clamp(g_selectedList.load(), 0, static_cast<int>(snap.lists.size()) - 1);
					if (ImGuiMCP::BeginChild("##rail", ImGuiMCP::ImVec2(railWidth, -footer), ImGuiMCP::ImGuiChildFlags_Border))
					{
						for (int i = 0; i < static_cast<int>(snap.lists.size()); ++i)
						{
							const auto& list = snap.lists[static_cast<std::size_t>(i)];
							const std::string label = std::format("{} ({})##list{}", ShortName(list.path), list.size, i);
							if (ImGuiMCP::Selectable(label.c_str(), i == selected))
							{
								selected = i;
								g_selectedList = i;
							}
							if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", list.path.c_str()); }
						}
					}
					ImGuiMCP::EndChild();
					ImGuiMCP::SameLine();
					if (ImGuiMCP::BeginChild("##content", ImGuiMCP::ImVec2(0.0F, -footer), ImGuiMCP::ImGuiChildFlags_Border))
					{
						const auto& list = snap.lists[static_cast<std::size_t>(selected)];
						ImGuiMCP::TextDisabled("%s", list.path.c_str());
						for (const auto& entry : list.entries)
						{
							ImGuiMCP::TextUnformatted(entry.label.empty() ? "-" : entry.label.c_str());
							if (settings::showInternalNames && !entry.numbers.empty()) { ImGuiMCP::TextDisabled("%s", entry.numbers.c_str()); }
							else if (!entry.numbers.empty() && ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", entry.numbers.c_str()); }
						}
						if (list.size > list.entries.size())
						{
							ImGuiMCP::TextDisabled("%s", std::format("... {} more", list.size - list.entries.size()).c_str());
						}
					}
					ImGuiMCP::EndChild();
				}
				ImGuiMCP::Separator();
				ImGuiMCP::TextDisabled("%s", strings::TR("LKN_Footer", "RaceMenu does all the real work underneath."));
			}
			ImGuiMCP::End();
		}

		void FloatRow(const char* a_key, const char* a_label, float* a_value, float a_min, float a_max, const char* a_helpKey, const char* a_help)
		{
			precise::SliderFloat(strings::TR(a_key, a_label), a_value, a_min, a_max, "%.2f");
			HelpMarker(strings::TR(a_helpKey, a_help));
		}

		void ToggleRow(const char* a_key, const char* a_label, bool* a_value, const char* a_helpKey, const char* a_help)
		{
			ImGuiMCP::Toggle(strings::TR(a_key, a_label), a_value);
			HelpMarker(strings::TR(a_helpKey, a_help));
		}

		void __stdcall RenderSettings()
		{
			strings::Tick();
			ImGuiMCP::TextWrapped("%s", strings::TR("LKN_SettingsIntro", "Changes apply as soon as you make them. Press Save to keep them for the next time you play."));
			ImGuiMCP::PushItemWidth(260.0F);

			ImGuiMCP::SeparatorText(strings::TR("LKN_SecGeneral", "General"));
			ToggleRow("LKN_Enabled", "Draw Likeness over RaceMenu", &settings::enabled, "LKN_HelpEnabled", "Off: RaceMenu shows only its own interface.");
			ToggleRow("LKN_StartInEditor", "Start in Likeness", &settings::startInEditor, "LKN_HelpStartInEditor",
				"Off: RaceMenu opens in its own interface and the switch key brings Likeness up.");

			ImGuiMCP::SeparatorText(strings::TR("LKN_SecLayout", "Layout"));
			FloatRow("LKN_Scale", "Size", &settings::scale, 0.5F, 2.0F, "LKN_HelpScale", "Size of the editor's text and controls.");
			FloatRow("LKN_PanelWidth", "Panel width", &settings::panelWidth, 0.2F, 0.5F, "LKN_HelpPanelWidth", "Share of the screen width the editor takes.");
			ToggleRow("LKN_ScenePanel", "Scene and presets panel", &settings::scenePanel, "LKN_HelpScenePanel", "The panel on the right with the camera, light and presets.");

			ImGuiMCP::SeparatorText(strings::TR("LKN_SecEditor", "Editor"));
			ToggleRow("LKN_AskBeforeDone", "Ask before finishing", &settings::askBeforeDone, "LKN_HelpAskBeforeDone", "Done asks before it finishes the character.");
			ToggleRow("LKN_GroupHeaders", "Group headers", &settings::groupHeaders, "LKN_HelpGroupHeaders", "Splits long categories into coloured groups.");
			ToggleRow("LKN_HideOtherBody", "Hide the other body's sliders", &settings::hideOtherBodySliders, "LKN_HelpHideOtherBody",
				"Hides body sliders that clearly belong to the other body type.");
			ToggleRow("LKN_AllowPastRange", "Allow typed values past the range", &settings::allowPastRange, "LKN_HelpAllowPastRange",
				"A typed value may go past the range a custom body morph registered.");
			ToggleRow("LKN_InternalNames", "Show internal names", &settings::showInternalNames, "LKN_HelpInternalNames", "Shows each slider's internal name and numbers, for mod authors.");

			ImGuiMCP::SeparatorText(strings::TR("LKN_SecCamera", "Camera"));
			FloatRow("LKN_TurnSpeed", "Turn speed", &settings::turnSpeed, 0.05F, 2.0F, "LKN_HelpTurnSpeed", "Degrees the character turns per pixel of drag.");

			ImGuiMCP::SeparatorText(strings::TR("LKN_SecControls", "Controls"));
			ImGuiMCP::Text("%s: 0x%02X", strings::TR("LKN_SwitchKey", "Switch key (scan code)"), settings::switchKey);
			HelpMarker(strings::TR("LKN_HelpSwitchKey", "Switches between Likeness and RaceMenu's own interface while RaceMenu is open. F4 is 0x3E. Set it in the INI."));

			ImGuiMCP::PopItemWidth();
			ImGuiMCP::SeparatorText("");

			if (ImGuiMCP::Button(strings::TR("LKN_Save", "Save")))
			{
				SetStatus(strings::TR("LKN_Saving", "Saving..."));
				OnMainThread([]() {
					SetStatus(settings::Save() ? strings::TR("LKN_Saved", "Settings saved.") : strings::TR("LKN_SaveFailed", "Could not write the INI. See the log."));
				});
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(strings::TR("LKN_Reload", "Reload from INI")))
			{
				OnMainThread([]() {
					SetStatus(settings::Reload() ? strings::TR("LKN_Reloaded", "Settings reloaded from the INI.") : strings::TR("LKN_ReloadFailed", "Could not read the INI. See the log."));
				});
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(strings::TR("LKN_Restore", "Restore defaults")))
			{
				OnMainThread([]() { settings::RestoreDefaults(); });
				SetStatus(strings::TR("LKN_Restored", "Defaults restored. Press Save to keep them."));
			}
			if (const std::string status = GetStatus(); !status.empty()) { ImGuiMCP::TextWrapped("%s", status.c_str()); }
			ImGuiMCP::TextDisabled("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::warn("no menu framework (Apocrypha Menu Framework or SKSE Menu Framework 3) is installed; Likeness stands down and RaceMenu keeps its own interface");
			return;
		}
		if (!HasRequiredExports())
		{
			logger::warn("the installed menu framework lacks exports Likeness draws with (listed above); Likeness stands down");
			return;
		}
		SKSEMenuFramework::SetSection("Likeness");
		SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
		g_window = SKSEMenuFramework::AddWindow(RenderWindow, true);
		if (!g_window)
		{
			logger::error("the menu framework returned no window; the settings page is there but the editor is not");
			return;
		}
		g_window->IsOpen = false;
		g_available = true;
		logger::info("registered the editor window and the settings page with the menu framework");
	}

	bool Available() { return g_available; }

	bool IsShown() { return g_window && g_window->IsOpen; }

	void SetShown(bool a_shown, const char* a_why)
	{
		if (!g_window)
		{
			logger::debug("SetShown({}) from {}: no window", a_shown, a_why ? a_why : "?");
			return;
		}
		if (a_shown && !bridge::IsRaceMenuOpen())
		{
			logger::debug("SetShown(true) from {}: RaceMenu is not open, ignored", a_why ? a_why : "?");
			return;
		}
		const bool was = g_window->IsOpen.exchange(a_shown);
		{
			std::scoped_lock guard(g_whyLock);
			g_lastWhy = a_why ? a_why : "?";
		}
		if (was != a_shown) { logger::info("editor {} ({})", a_shown ? "shown" : "hidden", a_why ? a_why : "?"); }
	}

	void OnRaceMenuOpened()
	{
		g_framesDrawn = 0;
		if (!g_available)
		{
			logger::info("RaceMenu opened; no editor window (see the registration lines above)");
			return;
		}
		if (settings::enabled && settings::startInEditor) { SetShown(true, "RaceMenu opened"); }
		else { logger::info("RaceMenu opened; the editor waits for the switch key (enabled={} startInEditor={})", settings::enabled, settings::startInEditor); }
	}

	void OnRaceMenuClosed() { SetShown(false, "RaceMenu closed"); }

	void OnSwitchKey(const char* a_source)
	{
		// One press can reach both the game sink and the window during the frame the window changes state; take one.
		const auto now = NowMs();
		if (now - g_lastSwitchMs.load() < 300)
		{
			logger::debug("switch key from {} ignored (within 300 ms of the last switch)", a_source);
			return;
		}
		if (!settings::enabled)
		{
			logger::debug("switch key from {} ignored: bEnabled = 0", a_source);
			return;
		}
		g_lastSwitchMs = now;
		++g_switchCount;
		SetShown(!IsShown(), a_source);
	}

	std::string StateJson()
	{
		std::string why;
		{
			std::scoped_lock guard(g_whyLock);
			why = g_lastWhy;
		}
		return std::format(R"({{"available":{},"shown":{},"lastWhy":"{}","switches":{},"framesDrawn":{},"selectedList":{},"enabled":{},"startInEditor":{},"switchKey":{}}})",
			g_available.load(), IsShown(), bridge::Escape(why), g_switchCount.load(), g_framesDrawn.load(), g_selectedList.load(), settings::enabled, settings::startInEditor,
			settings::switchKey);
	}
}
