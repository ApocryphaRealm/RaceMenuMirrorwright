#include "PCH.h"

#include "Editor.h"

#include "SKSEMenuFramework.h"

#include "Bridge.h"
#include "Name.h"
#include "PreciseSlider.h"
#include "Presets.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

// The editor (Milestone 2, the playable minimum, plus the comforts of Milestone 4 that cost little):
//   left window  - search, changed count, the category rail, the race grid / sliders / other mods' choices, Undo / Redo /
//                  Reset / Done (with confirmations);
//   right window - presets (list, load, save, delete, each confirmed) and RaceMenu's light;
//   the last rail entry, Discovery, keeps Milestone 1's raw view of what was read from RaceMenu, for the test run.
//
// Threads: the render callbacks run on the render thread and read only the bridge's snapshot copy and atomics; every
// change goes to the bridge (UI tasks) or the main thread (past mistake 2026-08-29).

namespace editor
{
	namespace
	{
		SKSEMenuFramework::Model::WindowInterface* g_window = nullptr;
		std::atomic<bool> g_available{ false };
		std::atomic<std::int64_t> g_lastSwitchMs{ 0 };
		std::atomic<std::uint32_t> g_switchCount{ 0 };
		std::atomic<std::uint32_t> g_framesDrawn{ 0 };
		std::atomic<int> g_category{ 0 };       // rail index
		std::atomic<bool> g_finishRequested{ false };
		std::atomic<bool> g_showWhenConsoleCloses{ false };
		std::atomic<bool> g_showWhenBoxCloses{ false };
		std::atomic<bool> g_finishing{ false };   // ConfirmDone sent: the game's own finish / name boxes own the input
		std::atomic<int> g_toolAnswer{ 0 };      // 1 confirm, 2 cancel: DevBench's answer to the open question
		std::atomic<bool> g_questionOpen{ false };
		std::string g_questionId;               // guarded by g_whyLock
		std::mutex g_toolSliderLock;
		std::vector<std::pair<std::string, float>> g_toolSliders;
		struct Chosen { std::string label, description; bool race = false; };
		std::optional<Chosen> g_toolChosen;   // guarded by g_toolSliderLock: a DevBench choice, for the panel and undo   // guarded by g_toolSliderLock: DevBench slider changes for the render thread
		std::string g_pendingCategory;          // guarded by g_whyLock: a rail entry asked for by name (DevBench)
		std::atomic<std::uint32_t> g_closeUps{ 0 };
		std::string g_lastWhy;
		std::mutex g_whyLock;
		std::string g_settingsStatus;
		std::mutex g_statusLock;

		// Render-thread state.
		struct Change
		{
			std::string callback;
			float before = 0.0F;
			float after = 0.0F;
			std::int64_t at = 0;
		};
		std::vector<Change> g_undo, g_redo;
		char g_search[128] = {};
		char g_presetName[128] = {};
		std::string g_message;                  // the editor's status line
		std::vector<presets::Preset> g_presets;
		int g_selectedPreset = -1;
		bool g_presetsDirty = true;
		bool g_focusSearch = false;

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void SetStatus(const std::string& a_text)
		{
			std::scoped_lock guard(g_statusLock);
			g_settingsStatus = a_text;
		}

		std::string GetStatus()
		{
			std::scoped_lock guard(g_statusLock);
			return g_settingsStatus;
		}

		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(std::move(a_task)); }
			else { logger::error("no SKSE task interface; an action was dropped"); }
		}

		// A translated string with "{name}" replaced by the display name (Name.h). Render thread only (the cache is not locked).
		const char* NTR(const char* a_key, const char* a_english)
		{
			static std::unordered_map<std::string, std::pair<std::string, std::string>> cache;   // key -> (source, result)
			const char* src = strings::TR(a_key, a_english);
			auto& slot = cache[a_key];
			if (slot.first != src)
			{
				slot.first = src;
				slot.second = src;
				for (std::size_t at; (at = slot.second.find("{name}")) != std::string::npos;) { slot.second.replace(at, 6, name::kDisplayName); }
			}
			return slot.second.c_str();
		}

		template <class F>
		bool Optional(const char* a_export, F a_call)
		{
			if (!GetMenuFrameworkFunction<void*>(a_export)) { return false; }
			a_call();
			return true;
		}

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
				"igOpenPopup_Str", "igBeginPopupModal", "igEndPopup", "igCloseCurrentPopup", "igSetItemDefaultFocus",
				"igInputTextWithHint", "igBeginDisabled", "igEndDisabled", "igGetContentRegionAvail", "igBeginTable", "igTableNextColumn",
				"igEndTable", "igIsWindowAppearing", "igTextColoredV", "igIsItemFocused", "igPushStyleColor_Vec4", "igPopStyleColor", "igPushStyleVar_Float", "igPopStyleVar",
				"igSliderFloat", "igIsKeyDown_Nil", "igIsMouseDown_Nil", "igIsMouseClicked_Bool",
				"igGetCursorScreenPos", "igGetWindowDrawList", "igGetFrameHeight", "igInvisibleButton", "igPushID_Str", "igPopID",
				"ImDrawList_AddRectFilled", "ImDrawList_AddCircleFilled"
			};
			bool ok = true;
			for (const char* fn : required)
			{
				if (!GetMenuFrameworkFunction<void*>(fn))
				{
					logger::warn("the menu framework does not export \"{}\"", fn);
					ok = false;
				}
			}
			return ok;
		}

		void HelpMarker(const char* a_text)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("%s", strings::TR("RMM_HelpMark", "(?)"));
			if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", a_text); }
		}

		// Font Awesome (the owner, 2026-11-06: "Don't forget to use font awesome"), drawn through AMF's own icon font the way our
		// other AMF pages do: the glyph in the icon font, then the label in the page's font, so every language keeps its text.
		// Without the framework's PushSolid export the icons are simply left out.
		namespace fa
		{
			inline constexpr unsigned kRace = 0xf0c0, kBody = 0xf183, kHead = 0xf007, kFace = 0xf118, kEyes = 0xf06e, kBrow = 0xf11a, kMouth = 0xf580,
									  kHair = 0xf0c4, kExtra = 0xf1de, kPaint = 0xf1fc, kOther = 0xf192, kChoices = 0xf03a, kDiscovery = 0xf188,
									  kUndo = 0xf0e2, kRedo = 0xf01e, kReset = 0xf2ea, kDone = 0xf00c, kLoad = 0xf07c, kSave = 0xf0c7, kDelete = 0xf1f8,
									  kRefresh = 0xf021, kLight = 0xf0eb, kSearch = 0xf002, kSwitch = 0xf362, kTurnLeft = 0xf2ea, kTurnRight = 0xf2f9, kCloseUp = 0xf00e;
		}

		const std::string& Glyph(unsigned a_cp)
		{
			static std::unordered_map<unsigned, std::string> cache;
			auto it = cache.find(a_cp);
			if (it == cache.end()) { it = cache.emplace(a_cp, FontAwesome::UnicodeToUtf8(a_cp)).first; }
			return it->second;
		}

		void Icon(unsigned a_cp)
		{
			static const bool have = GetMenuFrameworkFunction<void*>("PushSolid") && GetMenuFrameworkFunction<void*>("Pop");
			if (!have) { return; }
			FontAwesome::PushSolid();
			ImGuiMCP::TextUnformatted(Glyph(a_cp).c_str());
			FontAwesome::Pop();
			ImGuiMCP::SameLine();
		}

		// An icon, then a button with the label.
		bool IconButton(unsigned a_cp, const char* a_label)
		{
			Icon(a_cp);
			return ImGuiMCP::Button(a_label);
		}

		unsigned CategoryIcon(std::uint32_t a_flag, const std::string& a_label)
		{
			switch (a_flag)
			{
			case 2: return fa::kRace;
			case 4: return fa::kBody;
			case 8: return fa::kHead;
			case 16: return fa::kFace;
			case 32: return fa::kEyes;
			case 64: return fa::kBrow;
			case 128: return fa::kMouth;
			case 256: return fa::kHair;
			case 512: return fa::kExtra;
			default: break;
			}
			std::string up = a_label;
			for (char& c : up) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
			if (up.find("PAINT") != std::string::npos || up.find("MAKEUP") != std::string::npos || up.find("COLOR") != std::string::npos) { return fa::kPaint; }
			return fa::kOther;
		}

		std::string ShortName(const std::string& a_path)
		{
			const auto last = a_path.rfind('.');
			if (last == std::string::npos || last == 0) { return a_path; }
			const auto before = a_path.rfind('.', last - 1);
			return before == std::string::npos ? a_path : a_path.substr(before + 1);
		}

		bool ContainsNoCase(const std::string& a_hay, const char* a_needle)
		{
			if (!*a_needle) { return true; }
			const std::size_t n = std::strlen(a_needle);
			if (n > a_hay.size()) { return false; }
			for (std::size_t i = 0; i + n <= a_hay.size(); ++i)
			{
				if (_strnicmp(a_hay.c_str() + i, a_needle, n) == 0) { return true; }
			}
			return false;
		}

		const char* FormatFor(float a_step)
		{
			if (a_step >= 1.0F) { return "%.0f"; }
			if (a_step >= 0.1F) { return "%.1f"; }
			return "%.2f";
		}

		// ---- the rail ---------------------------------------------------------------------------------------------------
		struct RailItem
		{
			enum class Kind { kRace, kSliders, kChoices, kDiscovery } kind;
			std::string label;
			std::uint32_t flag = 0;
		};

		// The rail, in RaceMenu's category order (test run 2026-11-06: its category list names every category, Apprentice's
		// included). A category whose rows are races is the race grid; one whose rows are another mod's choices is a choice
		// grid; the rest are sliders. A choice flag no category names gets one entry after Race.
		std::vector<RailItem> BuildRail(const bridge::Snapshot& a_s)
		{
			std::vector<RailItem> rail;
			const auto& m = a_s.model;
			std::set<std::uint32_t> covered;
			for (const auto& c : m.categories)
			{
				const bool races = std::any_of(m.races.begin(), m.races.end(), [&](const bridge::Race& r) { return !r.other && (r.flag & c.flag); });
				const bool choices = std::any_of(m.races.begin(), m.races.end(), [&](const bridge::Race& r) { return r.other && (r.flag & c.flag); });
				const bool sliders = std::any_of(m.sliders.begin(), m.sliders.end(), [&](const bridge::Slider& sl) { return sl.flag & c.flag; });
				if (races || (c.flag & bridge::kCategoryRace)) { rail.push_back({ RailItem::Kind::kRace, c.label, c.flag }); }
				else if (choices) { rail.push_back({ RailItem::Kind::kChoices, c.label, c.flag }); }
				else if (sliders) { rail.push_back({ RailItem::Kind::kSliders, c.label, c.flag }); }
				else { continue; }   // a category with nothing in it for this character
				covered.insert(c.flag);
			}
			// Other mods' choice categories (Apprentice's Class and Trait) come right after Race (the owner, 2026-11-06 20:58).
			{
				std::vector<RailItem> ordered;
				std::vector<RailItem> choices;
				for (const auto& r : rail)
				{
					if (r.kind == RailItem::Kind::kChoices) { choices.push_back(r); }
				}
				for (const auto& r : rail)
				{
					if (r.kind == RailItem::Kind::kChoices) { continue; }
					ordered.push_back(r);
					if (r.kind == RailItem::Kind::kRace) { ordered.insert(ordered.end(), choices.begin(), choices.end()); choices.clear(); }
				}
				ordered.insert(ordered.end(), choices.begin(), choices.end());
				rail.swap(ordered);
			}
			std::set<std::uint32_t> orphans;
			for (const auto& r : m.races)
			{
				if (r.other && !std::any_of(covered.begin(), covered.end(), [&](std::uint32_t f) { return (r.flag & f) != 0; })) { orphans.insert(r.flag); }
			}
			for (const auto flag : orphans) { rail.insert(rail.begin() + (rail.empty() ? 0 : 1), { RailItem::Kind::kChoices, std::format("Choices {}", flag), flag }); }
			rail.push_back({ RailItem::Kind::kDiscovery, NTR("RMM_Discovery", "Discovery"), 0 });
			return rail;
		}

		// ---- edits ------------------------------------------------------------------------------------------------------
		void Apply(const std::string& a_callback, float a_before, float a_after, bool a_record)
		{
			bridge::SetSlider(a_callback, a_after);   // a_callback is the slider's key
			if (!a_record) { return; }
			const auto now = NowMs();
			if (!g_undo.empty() && g_undo.back().callback == a_callback && now - g_undo.back().at < 600)
			{
				g_undo.back().after = a_after;   // one drag or one held nudge is one undo step
				g_undo.back().at = now;
			}
			else { g_undo.push_back({ a_callback, a_before, a_after, now }); }
			if (g_undo.size() > 500) { g_undo.erase(g_undo.begin()); }
			g_redo.clear();
		}

		void Undo()
		{
			if (g_undo.empty()) { return; }
			Change c = g_undo.back();
			g_undo.pop_back();
			bridge::SetSlider(c.callback, c.before);
			g_redo.push_back(c);
			logger::debug("undo {} -> {:.3f}", c.callback, c.before);
		}

		void Redo()
		{
			if (g_redo.empty()) { return; }
			Change c = g_redo.back();
			g_redo.pop_back();
			bridge::SetSlider(c.callback, c.after);
			c.at = 0;
			g_undo.push_back(c);
			logger::debug("redo {} -> {:.3f}", c.callback, c.after);
		}

		// Groups of a category (M4), from what the row does (its game callback; the test run's rows carry no group of their own),
		// each with its own muted hue.
		int GroupOf(const bridge::Slider& a_s)
		{
			const std::string& c = a_s.callback;
			if (c == "ChangeDoubleMorph" || c == "ChangeMorph" || c == "ChangeWeight") { return 0; }
			if (c == "ChangeHeadPart" || c == "ChangeHeadPreset" || c == "ChangePreset" || c == "ChangeFaceDetails") { return 1; }
			if (c == "ChangeTintingMask" || c == "ChangeMaskColor" || c == "ChangeHairColorPreset" || a_s.colour) { return 2; }
			if (c == "ChangeMask") { return 3; }
			return 4;   // RaceMenu plugin sliders and the rest
		}

		const char* GroupName(int a_group)
		{
			switch (a_group)
			{
			case 0: return NTR("RMM_GroupShape", "Shape");
			case 1: return NTR("RMM_GroupParts", "Parts");
			case 2: return NTR("RMM_GroupColours", "Colours");
			case 3: return NTR("RMM_GroupPaint", "Paint");
			default: return NTR("RMM_GroupMore", "More");
			}
		}

		ImGuiMCP::ImVec4 GroupHue(int a_group)
		{
			static const ImGuiMCP::ImVec4 hues[] = { { 0.56F, 0.64F, 0.71F, 1.0F }, { 0.71F, 0.60F, 0.50F, 1.0F }, { 0.56F, 0.69F, 0.65F, 1.0F },
				{ 0.66F, 0.62F, 0.75F, 1.0F }, { 0.64F, 0.70F, 0.57F, 1.0F } };
			return hues[std::clamp(a_group, 0, 4)];
		}

		void SliderRow(const bridge::Slider& a_s)
		{
			ImGuiMCP::PushID(a_s.key.c_str());
			float v = a_s.value;
			const bool changed = !std::isnan(a_s.openValue) && a_s.value != a_s.openValue;
			const std::string label = (changed ? "* " : "") + a_s.label;
			if (a_s.colour)
			{
				const ImGuiMCP::ImVec4 c(((a_s.fill >> 16) & 0xFF) / 255.0F, ((a_s.fill >> 8) & 0xFF) / 255.0F, (a_s.fill & 0xFF) / 255.0F, 1.0F);
				// a Font Awesome square in the row's colour (the icon font always has it)
				static const bool haveFa = GetMenuFrameworkFunction<void*>("PushSolid") != nullptr;
				if (haveFa) { FontAwesome::PushSolid(); }
				ImGuiMCP::TextColored(c, "%s", Glyph(0xf0c8).c_str());
				if (haveFa) { FontAwesome::Pop(); }
				ImGuiMCP::SameLine();
			}
			ImGuiMCP::TextUnformatted(label.c_str());
			if (settings::showInternalNames) { ImGuiMCP::TextDisabled("%s", a_s.callback.c_str()); }
			const float lo = settings::allowPastRange ? std::min(a_s.min, a_s.max * -2.0F) : a_s.min;
			const float hi = settings::allowPastRange ? a_s.max * 2.0F : a_s.max;
			ImGuiMCP::PushItemWidth(-1.0F);
			if (precise::SliderFloat("##v", &v, lo, hi, FormatFor(a_s.step)) && v != a_s.value) { Apply(a_s.key, a_s.value, v, true); }
			ImGuiMCP::PopItemWidth();
			if (changed && ImGuiMCP::IsItemHovered())
			{
				ImGuiMCP::SetTooltip("%s", std::format("{} {:.2f}", NTR("RMM_WasValue", "When the menu opened:"), a_s.openValue).c_str());
			}
			ImGuiMCP::PopID();
		}

		// The race panel (SPEC 2.3, first part): the description of the race or choice last hovered or chosen, below the grid.
		std::string g_panelTitle, g_panelText;

		void RaceGrid(const bridge::Snapshot& a_s, bool a_others, std::uint32_t a_flag)
		{
			const float avail = ImGuiMCP::GetContentRegionAvail().x;
			const int columns = std::max(1, static_cast<int>(avail / (150.0F * std::clamp(settings::scale, 0.5F, 2.0F))));
			if (!ImGuiMCP::BeginTable("##grid", columns)) { return; }
			for (const auto& r : a_s.model.races)
			{
				if (a_others ? (!r.other || r.flag != a_flag) : r.other) { continue; }
				if (!ContainsNoCase(r.label, g_search)) { continue; }
				ImGuiMCP::TableNextColumn();
				ImGuiMCP::PushID(static_cast<int>(r.index));
				if (ImGuiMCP::Button(r.label.c_str(), ImGuiMCP::ImVec2(-1.0F, 0.0F)))
				{
					bridge::ChooseEntry(r.list, r.index);
					if (!r.other) { g_undo.clear(); g_redo.clear(); }
					g_panelTitle = r.label;
					g_panelText = r.description;
					g_message = std::format("{} {}", NTR("RMM_Chose", "Chosen:"), r.label);
				}
				if (ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused())
				{
					g_panelTitle = r.label;
					g_panelText = r.details.empty() ? r.description : r.description + "\n\n" + r.details;
				}
				ImGuiMCP::PopID();
			}
			ImGuiMCP::EndTable();
			if (!g_panelTitle.empty())
			{
				ImGuiMCP::SeparatorText(g_panelTitle.c_str());
				if (g_panelText.empty()) { ImGuiMCP::TextDisabled("%s", NTR("RMM_NoDescription", "No description.")); }
				else { ImGuiMCP::TextWrapped("%s", g_panelText.c_str()); }
			}
		}

		void DiscoveryView(const bridge::Snapshot& a_s)
		{
			static int selected = 0;
			ImGuiMCP::TextWrapped("%s", NTR("RMM_DiscoveryNote", "What {name} read from RaceMenu, for bug reports."));
			ImGuiMCP::TextDisabled("%s", a_s.status.c_str());
			if (ImGuiMCP::Button(NTR("RMM_Refresh", "Read RaceMenu again"))) { bridge::RequestProbe(); }
			for (int i = 0; i < static_cast<int>(a_s.lists.size()); ++i)
			{
				const auto& l = a_s.lists[static_cast<std::size_t>(i)];
				const std::string label = std::format("{} ({})##l{}", ShortName(l.path), l.size, i);
				if (ImGuiMCP::Selectable(label.c_str(), i == selected)) { selected = i; }
				if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", l.path.c_str()); }
			}
			if (selected >= 0 && selected < static_cast<int>(a_s.lists.size()))
			{
				ImGuiMCP::Separator();
				for (const auto& e : a_s.lists[static_cast<std::size_t>(selected)].entries)
				{
					ImGuiMCP::TextUnformatted(e.label.empty() ? "-" : e.label.c_str());
					if (!e.numbers.empty() && ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", e.numbers.c_str()); }
				}
			}
		}

		// ---- confirmations ------------------------------------------------------------------------------------------------
		// One choice of a confirmation box, drawn as its own filled box: grey with light text, or white with dark text when it has
		// the focus (the owner, 2026-11-06 21:00 and 21:01). Returns true when clicked; hovering focuses it.
		bool Choice(const char* a_label, bool a_focused, bool& a_hovered)
		{
			using namespace ImGuiMCP;
			const ImVec4 white(1.0F, 1.0F, 1.0F, 1.0F);
			const ImVec4 dark(0.05F, 0.05F, 0.05F, 1.0F);
			const ImVec4 grey(0.22F, 0.22F, 0.22F, 1.0F);   // the owner, 21:01: a filled box, never an outline
			const ImVec4 light(0.9F, 0.9F, 0.9F, 1.0F);
			PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
			PushStyleColor(ImGuiCol_Border, grey);
			PushStyleColor(ImGuiCol_Button, a_focused ? white : grey);
			PushStyleColor(ImGuiCol_ButtonHovered, a_focused ? white : grey);
			PushStyleColor(ImGuiCol_ButtonActive, white);
			PushStyleColor(ImGuiCol_Text, a_focused ? dark : light);
			const float w = std::max(120.0F, GetFrameHeight() * 5.0F);
			const bool clicked = Button(a_label, ImVec2(w, GetFrameHeight() * 1.4F));
			a_hovered = IsItemHovered();
			PopStyleColor(5);
			PopStyleVar(1);
			return clicked;
		}

		// A confirmation box: the confirming choice and Cancel. Focus starts on the confirming choice; Left / Right, the D-pad
		// and Tab move it, the mouse focuses what it hovers; Enter / Space / A take the focused choice; Esc / B cancel. The box
		// reads its own keys (test run 3: the focused-button route did not answer). DevBench answers with op=confirm / cancel.
		bool Confirm(const char* a_id, const char* a_text, const char* a_yes)
		{
			static std::unordered_map<std::string, int> focus;   // per box: 0 = the confirming choice, 1 = Cancel
			bool yes = false;
			if (ImGuiMCP::BeginPopupModal(a_id, nullptr, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize))
			{
				using namespace ImGuiMCP;
				g_questionOpen = true;
				{
					std::scoped_lock guard(g_whyLock);
					g_questionId = a_id;
				}
				const bool appearing = IsWindowAppearing();
				int& f = focus[a_id];
				if (appearing) { f = 0; }
				TextUnformatted(a_text);
				Spacing();

				if (!appearing)
				{
					if (IsKeyPressed(ImGuiKey_LeftArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadLeft, false)) { f = 0; }
					if (IsKeyPressed(ImGuiKey_RightArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadRight, false)) { f = 1; }
					if (IsKeyPressed(ImGuiKey_Tab, false)) { f = 1 - f; }
				}
				const bool keyTake = !appearing && (IsKeyPressed(ImGuiKey_Enter, false) || IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
													   IsKeyPressed(ImGuiKey_Space, false) || IsKeyPressed(ImGuiKey_GamepadFaceDown, false));
				const bool keyNo = !appearing && (IsKeyPressed(ImGuiKey_Escape, false) || IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
				const int tool = g_toolAnswer.exchange(0);

				bool hoverYes = false, hoverNo = false;
				const bool clickYes = Choice((std::string(a_yes) + "##yes").c_str(), f == 0, hoverYes);
				SameLine();
				const bool clickNo = Choice((std::string(NTR("RMM_Cancel", "Cancel")) + "##no").c_str(), f == 1, hoverNo);
				if (hoverYes) { f = 0; }
				if (hoverNo) { f = 1; }

				const char* how = nullptr;
				int answer = 0;   // 1 yes, 2 no
				if (tool) { answer = tool; how = "devbench"; }
				else if (clickYes) { answer = 1; how = "click"; }
				else if (clickNo) { answer = 2; how = "click"; }
				else if (keyNo) { answer = 2; how = "key"; }
				else if (keyTake) { answer = f == 0 ? 1 : 2; how = "key"; }
				if (answer == 1)
				{
					yes = true;
					logger::info("question {}: answered yes ({})", a_id, how);
				}
				else if (answer == 2) { logger::info("question {}: cancelled ({})", a_id, how); }
				if (answer)
				{
					CloseCurrentPopup();
					g_questionOpen = false;
				}
				EndPopup();
			}
			return yes;
		}

		void DoFinish()
		{
			logger::info("Done confirmed");
			if (bridge::Finish())
			{
				// The game now asks its own questions ("Finish and name your character?", the name box): the editor hides and gives
				// RaceMenu's panels back so those get the keys (test run 4: the game's box was stuck under the editor). Cancelled,
				// the switch key brings the editor back.
				g_finishing = true;
				SetShown(false, "Done: the game's own finish");
				return;
			}
			{
			{
				g_message = NTR("RMM_NoDone", "Finish from RaceMenu's own interface: switched to it.");
				SetShown(false, "Done without a known Done function");
			}
			}
		}

		// ---- the windows --------------------------------------------------------------------------------------------------
		void EditorWindow(const bridge::Snapshot& a_s, float a_width, float a_height)
		{
			ImGuiMCP::SetNextWindowPos(ImGuiMCP::ImVec2(0.0F, 0.0F), ImGuiMCP::ImGuiCond_Always);
			ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2(a_width, a_height), ImGuiMCP::ImGuiCond_Always);
			const auto flags = ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoResize |
			                   ImGuiMCP::ImGuiWindowFlags_NoSavedSettings;
			const std::string title = std::string(NTR("RMM_Title", "{name}")) + "###MirrorwrightEditor";
			if (ImGuiMCP::Begin(title.c_str(), nullptr, flags))
			{
				ImGuiMCP::SetWindowFontScale(std::clamp(settings::scale, 0.5F, 2.0F));
				auto* io = ImGuiMCP::GetIO();
				const bool ctrl = io && io->KeyCtrl;
				if (const auto key = KeyFromScanCode(settings::switchKey); key != ImGuiMCP::ImGuiKey_None && ImGuiMCP::IsKeyPressed(key, false)) { OnSwitchKey("editor window"); }
				if (ctrl && ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Z, false)) { Undo(); }
				if (ctrl && ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Y, false)) { Redo(); }
				if (ctrl && ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_F, false)) { g_focusSearch = true; }
				const bool typing = io && io->WantTextInput;
				if (!typing && !ctrl)
				{
					// Keyboard: Q / E turn, L light, R Done (SPEC 8). Pad: right stick turns, X Done.
					float turn = 0.0F;
					if (ImGuiMCP::IsKeyDown(ImGuiMCP::ImGuiKey_Q)) { turn -= 2.0F; }
					if (ImGuiMCP::IsKeyDown(ImGuiMCP::ImGuiKey_E)) { turn += 2.0F; }
					if (ImGuiMCP::IsKeyDown(ImGuiMCP::ImGuiKey_GamepadRStickLeft)) { turn -= 2.5F; }
					if (ImGuiMCP::IsKeyDown(ImGuiMCP::ImGuiKey_GamepadRStickRight)) { turn += 2.5F; }
					if (turn != 0.0F) { bridge::TurnBy(turn * settings::turnSpeed / 0.35F); }
					if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_L, false)) { bridge::ToggleLight(); }
					if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Z, false) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_GamepadR3, false)) { bridge::ToggleCloseUp(); }
					if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_R, false) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_GamepadFaceLeft, false)) { g_finishRequested = true; }
				}

				// Search and the changed count.
				if (g_focusSearch)
				{
					Optional("igSetKeyboardFocusHere", []() { ImGuiMCP::SetKeyboardFocusHere(0); });
					g_focusSearch = false;
				}
				Icon(fa::kSearch);
				ImGuiMCP::PushItemWidth(a_width * 0.50F);
				ImGuiMCP::InputTextWithHint("##search", NTR("RMM_SearchHint", "Search every slider (Ctrl+F)"), g_search, sizeof(g_search));
				ImGuiMCP::PopItemWidth();
				const auto changed = std::count_if(a_s.model.sliders.begin(), a_s.model.sliders.end(),
					[](const bridge::Slider& s) { return !std::isnan(s.openValue) && s.value != s.openValue; });
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("%s", std::format("{} {}", changed, NTR("RMM_Changed", "changed")).c_str());
				ImGuiMCP::SameLine();
				if (IconButton(fa::kSwitch, NTR("RMM_ToRaceMenu", "RaceMenu's interface"))) { SetShown(false, "button"); }
				if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", NTR("RMM_HelpToRaceMenu", "Hides {name} and leaves RaceMenu's own interface. The switch key (F4) brings {name} back.")); }
				ImGuiMCP::Separator();

				const float footer = ImGuiMCP::GetFrameHeightWithSpacing() * 2.2F;
				const auto rail = BuildRail(a_s);
				{
					std::scoped_lock guard(g_whyLock);
					if (!g_pendingCategory.empty())
					{
						for (int i = 0; i < static_cast<int>(rail.size()); ++i)
						{
							if (_stricmp(rail[static_cast<std::size_t>(i)].label.c_str(), g_pendingCategory.c_str()) == 0) { g_category = i; }
						}
						g_pendingCategory.clear();
					}
				}
				int category = std::clamp(g_category.load(), 0, static_cast<int>(rail.size()) - 1);
				const bool searching = g_search[0] != '\0';

				if (ImGuiMCP::BeginChild("##rail", ImGuiMCP::ImVec2(a_width * 0.30F, -footer), ImGuiMCP::ImGuiChildFlags_Border))
				{
					for (int i = 0; i < static_cast<int>(rail.size()); ++i)
					{
						const auto& item = rail[static_cast<std::size_t>(i)];
						Icon(item.kind == RailItem::Kind::kDiscovery ? fa::kDiscovery : item.kind == RailItem::Kind::kChoices ? fa::kChoices : CategoryIcon(item.flag, item.label));
						const std::string label = item.label + "##rail" + std::to_string(i);
						if (ImGuiMCP::Selectable(label.c_str(), !searching && i == category))
						{
							category = i;
							g_category = i;
							g_search[0] = '\0';
						}
					}
				}
				ImGuiMCP::EndChild();
				ImGuiMCP::SameLine();
				if (ImGuiMCP::BeginChild("##content", ImGuiMCP::ImVec2(0.0F, -footer), ImGuiMCP::ImGuiChildFlags_Border))
				{
					if (!a_s.probed) { ImGuiMCP::TextWrapped("%s", NTR("RMM_Reading", "Reading RaceMenu...")); }
					else if (searching)
					{
						for (const auto& c : a_s.model.categories)
						{
							bool header = false;
							for (const auto& s : a_s.model.sliders)
							{
								if (!(s.flag & c.flag) || !(ContainsNoCase(s.label, g_search) || (settings::showInternalNames && ContainsNoCase(s.callback, g_search)))) { continue; }
								if (!header)
								{
									ImGuiMCP::SeparatorText(c.label.c_str());
									header = true;
								}
								SliderRow(s);
							}
						}
						RaceGrid(a_s, false, 0);
					}
					else
					{
						const auto& item = rail[static_cast<std::size_t>(category)];
						switch (item.kind)
						{
						case RailItem::Kind::kRace:
							if (a_s.model.races.empty()) { ImGuiMCP::TextWrapped("%s", NTR("RMM_NoRaces", "No race list was found. Send the log.")); }
							RaceGrid(a_s, false, 0);
							break;
						case RailItem::Kind::kChoices:
							RaceGrid(a_s, true, item.flag);
							break;
						case RailItem::Kind::kDiscovery:
							DiscoveryView(a_s);
							break;
						case RailItem::Kind::kSliders:
							{
								bool any = false;
								for (int g = 0; g < (settings::groupHeaders ? 5 : 1); ++g)
								{
									bool header = false;
									for (const auto& s : a_s.model.sliders)
									{
										if (!(s.flag & item.flag) || (settings::groupHeaders && GroupOf(s) != g)) { continue; }
										if (settings::groupHeaders && !header)
										{
											ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, GroupHue(g));
											ImGuiMCP::SeparatorText(GroupName(g));
											ImGuiMCP::PopStyleColor(1);
											header = true;
										}
										SliderRow(s);
										any = true;
									}
								}
								if (!any) { ImGuiMCP::TextDisabled("%s", NTR("RMM_Empty", "Nothing here for this character.")); }
							}
							break;
						}
					}
				}
				ImGuiMCP::EndChild();

				// Footer: Undo, Redo, Reset, Done.
				ImGuiMCP::BeginDisabled(g_undo.empty());
				if (IconButton(fa::kUndo, NTR("RMM_Undo", "Undo"))) { Undo(); }
				ImGuiMCP::EndDisabled();
				ImGuiMCP::SameLine();
				ImGuiMCP::BeginDisabled(g_redo.empty());
				if (IconButton(fa::kRedo, NTR("RMM_Redo", "Redo"))) { Redo(); }
				ImGuiMCP::EndDisabled();
				ImGuiMCP::SameLine();
				ImGuiMCP::BeginDisabled(changed == 0);
				if (IconButton(fa::kReset, NTR("RMM_Reset", "Reset"))) { ImGuiMCP::OpenPopup("##confirmReset"); }
				ImGuiMCP::EndDisabled();
				ImGuiMCP::SameLine();
				if (IconButton(fa::kDone, NTR("RMM_Done", "Done")) || g_finishRequested.exchange(false))
				{
					if (settings::askBeforeDone) { ImGuiMCP::OpenPopup("##confirmDone"); }
					else { DoFinish(); }
				}
				if (!g_message.empty()) { ImGuiMCP::TextDisabled("%s", g_message.c_str()); }

				if (Confirm("##confirmReset", NTR("RMM_AskReset", "Put every slider back to its value when the menu opened?"), NTR("RMM_Reset", "Reset")))
				{
					for (const auto& s : a_s.model.sliders)
					{
						if (!std::isnan(s.openValue) && s.value != s.openValue) { Apply(s.key, s.value, s.openValue, false); }
					}
					g_undo.clear();
					g_redo.clear();
					g_message = NTR("RMM_ResetDone", "Every slider is back to its value when the menu opened.");
				}
				if (Confirm("##confirmDone", NTR("RMM_AskDone", "Finish character creation?"), NTR("RMM_Finish", "Finish"))) { DoFinish(); }
			}
			ImGuiMCP::End();
		}

		void ScenePanel(float a_x, float a_width, float a_height)
		{
			ImGuiMCP::SetNextWindowPos(ImGuiMCP::ImVec2(a_x, 0.0F), ImGuiMCP::ImGuiCond_Always);
			ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2(a_width, a_height), ImGuiMCP::ImGuiCond_Always);
			const auto flags = ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoResize |
			                   ImGuiMCP::ImGuiWindowFlags_NoSavedSettings;
			const std::string title = std::string(NTR("RMM_Scene", "Scene and presets")) + "###MirrorwrightScene";
			if (ImGuiMCP::Begin(title.c_str(), nullptr, flags))
			{
				ImGuiMCP::SetWindowFontScale(std::clamp(settings::scale, 0.5F, 2.0F));
				if (IconButton(fa::kLight, NTR("RMM_Light", "Light")))
				{
					// RaceMenu's documented RSM_ToggleLight event (RaceMenu.psc OnToggleLight).
					bridge::ToggleLight();
				}
				ImGuiMCP::SameLine();
				if (IconButton(fa::kCloseUp, NTR("RMM_CloseUp", "Close-up"))) { bridge::ToggleCloseUp(); }
				ImGuiMCP::SameLine();
				if (IconButton(fa::kTurnLeft, NTR("RMM_TurnLeft", "Turn left"))) { bridge::TurnBy(-15.0F); }
				ImGuiMCP::SameLine();
				if (IconButton(fa::kTurnRight, NTR("RMM_TurnRight", "Turn right"))) { bridge::TurnBy(15.0F); }
				ImGuiMCP::SeparatorText(NTR("RMM_Presets", "Presets"));
				if (g_presetsDirty)
				{
					g_presets = presets::List();
					g_presetsDirty = false;
				}
				const float listHeight = a_height * 0.55F;
				if (ImGuiMCP::BeginChild("##presets", ImGuiMCP::ImVec2(0.0F, listHeight), ImGuiMCP::ImGuiChildFlags_Border))
				{
					if (g_presets.empty()) { ImGuiMCP::TextDisabled("%s", NTR("RMM_NoPresets", "No presets saved yet.")); }
					for (int i = 0; i < static_cast<int>(g_presets.size()); ++i)
					{
						const auto& p = g_presets[static_cast<std::size_t>(i)];
						ImGuiMCP::BeginDisabled(!p.loadable);
						if (ImGuiMCP::Selectable((p.name + "##p" + std::to_string(i)).c_str(), i == g_selectedPreset))
						{
							g_selectedPreset = i;
							std::snprintf(g_presetName, sizeof(g_presetName), "%s", p.name.c_str());
						}
						ImGuiMCP::EndDisabled();
						if (!p.loadable && ImGuiMCP::IsItemHovered())
						{
							ImGuiMCP::SetTooltip("%s", NTR("RMM_PresetUnreadable", "RaceMenu cannot open this name on this Windows (its letters are outside the system code page)."));
						}
					}
				}
				ImGuiMCP::EndChild();
				ImGuiMCP::PushItemWidth(-1.0F);
				ImGuiMCP::InputTextWithHint("##presetName", NTR("RMM_PresetName", "Preset name"), g_presetName, sizeof(g_presetName));
				ImGuiMCP::PopItemWidth();

				const bool haveSel = g_selectedPreset >= 0 && g_selectedPreset < static_cast<int>(g_presets.size()) && g_presets[static_cast<std::size_t>(g_selectedPreset)].loadable;
				ImGuiMCP::BeginDisabled(!haveSel);
				if (IconButton(fa::kLoad, NTR("RMM_Load", "Load"))) { ImGuiMCP::OpenPopup("##confirmLoad"); }
				ImGuiMCP::EndDisabled();
				ImGuiMCP::SameLine();
				ImGuiMCP::BeginDisabled(g_presetName[0] == '\0');
				if (IconButton(fa::kSave, NTR("RMM_SavePreset", "Save")))
				{
					if (presets::Exists(g_presetName)) { ImGuiMCP::OpenPopup("##confirmOverwrite"); }
					else
					{
						g_message = presets::Save(g_presetName) ? NTR("RMM_PresetSaved", "Preset saved.") : NTR("RMM_PresetBadName", "That name cannot be used for a preset on this Windows.");
						g_presetsDirty = true;
					}
				}
				ImGuiMCP::EndDisabled();
				ImGuiMCP::SameLine();
				ImGuiMCP::BeginDisabled(g_selectedPreset < 0 || g_selectedPreset >= static_cast<int>(g_presets.size()));
				if (IconButton(fa::kDelete, NTR("RMM_Delete", "Delete"))) { ImGuiMCP::OpenPopup("##confirmDelete"); }
				ImGuiMCP::EndDisabled();
				ImGuiMCP::TextWrapped("%s", g_message.c_str());

				if (Confirm("##confirmLoad", NTR("RMM_AskLoad", "Load this preset? Unsaved changes are replaced."), NTR("RMM_Load", "Load")) && haveSel)
				{
					g_message = presets::Load(g_presets[static_cast<std::size_t>(g_selectedPreset)]) ? NTR("RMM_PresetLoading", "Loading the preset...") : NTR("RMM_PresetFailed", "The preset could not be loaded. See the log.");
					g_undo.clear();
					g_redo.clear();
				}
				if (Confirm("##confirmOverwrite", NTR("RMM_AskOverwrite", "A preset with this name exists. Replace it?"), NTR("RMM_Replace", "Replace")))
				{
					g_message = presets::Save(g_presetName) ? NTR("RMM_PresetSaved", "Preset saved.") : NTR("RMM_PresetBadName", "That name cannot be used for a preset on this Windows.");
					g_presetsDirty = true;
				}
				if (Confirm("##confirmDelete", NTR("RMM_AskDelete", "Delete this preset file? This cannot be undone."), NTR("RMM_Delete", "Delete")) &&
					g_selectedPreset >= 0 && g_selectedPreset < static_cast<int>(g_presets.size()))
				{
					g_message = presets::Remove(g_presets[static_cast<std::size_t>(g_selectedPreset)]) ? NTR("RMM_PresetDeleted", "Preset deleted.") : NTR("RMM_PresetDeleteFailed", "The preset could not be deleted. See the log.");
					g_selectedPreset = -1;
					g_presetsDirty = true;
				}
				if (IconButton(fa::kRefresh, NTR("RMM_RefreshPresets", "Refresh the list"))) { g_presetsDirty = true; }
			}
			ImGuiMCP::End();
		}

		void __stdcall RenderWindow()
		{
			strings::Tick();
			if (!g_window || !g_window->IsOpen) { return; }

			static bridge::Snapshot snap;
			static std::uint64_t serial = ~0ull;
			bridge::CopyIfChanged(serial, snap);
			++g_framesDrawn;
			{
				std::vector<std::pair<std::string, float>> pending;
				{
					std::scoped_lock guard(g_toolSliderLock);
					pending.swap(g_toolSliders);
					if (g_toolChosen)
					{
						g_panelTitle = g_toolChosen->label;
						g_panelText = g_toolChosen->description;
						if (g_toolChosen->race) { g_undo.clear(); g_redo.clear(); }
						g_toolChosen.reset();
					}
				}
				for (const auto& [key, value] : pending)
				{
					const auto it = std::find_if(snap.model.sliders.begin(), snap.model.sliders.end(), [&](const bridge::Slider& s) { return s.key == key; });
					if (it == snap.model.sliders.end())
					{
						logger::warn("devbench slider: no slider with key '{}'", key);
						continue;
					}
					Apply(key, it->value, value, true);
					g_undo.back().at = 0;   // each tool change is its own undo step
				}
			}

			auto* io = ImGuiMCP::GetIO();
			const float displayW = io ? io->DisplaySize.x : 1920.0F;
			const float displayH = io ? io->DisplaySize.y : 1080.0F;
			const float width = std::clamp(settings::panelWidth, 0.2F, 0.5F) * displayW;
			ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_NavHighlight, ImGuiMCP::ImVec4(0.0F, 0.0F, 0.0F, 0.0F));   // run 6: no blue nav outline
			EditorWindow(snap, width, displayH);

			// Mouse drag on the character (the owner, 2026-11-06 20:59): a left press on the open space outside every editor
			// window starts a drag; while held, the character turns by the horizontal movement times fTurnSpeed (degrees per
			// pixel). Mirrorwright's own rotation (Actor::SetHeading); while the editor is up it holds the input, so RaceMenu's
			// or another mod's rotation does not see the same drag.
			{
				static bool dragging = false;
				const bool down = ImGuiMCP::IsMouseDown(ImGuiMCP::ImGuiMouseButton_Left);
				if (!down) { dragging = false; }
				else if (!dragging && ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Left, false) && io && !io->WantCaptureMouse)
				{
					dragging = true;
					logger::debug("drag-turn started at x={:.0f}", io->MousePos.x);
				}
				if (dragging && io && io->MouseDelta.x != 0.0F) { bridge::TurnBy(io->MouseDelta.x * settings::turnSpeed); }
			}
			ImGuiMCP::PopStyleColor(1);
			if (settings::scenePanel)
			{
				const float sceneW = displayW * 0.20F;
				ScenePanel(displayW - sceneW, sceneW, displayH * 0.75F);
			}
		}

		void FloatRow(const char* a_key, const char* a_label, float* a_value, float a_min, float a_max, const char* a_helpKey, const char* a_help)
		{
			precise::SliderFloat(NTR(a_key, a_label), a_value, a_min, a_max, "%.2f");
			HelpMarker(NTR(a_helpKey, a_help));
		}

		void ToggleRow(const char* a_key, const char* a_label, bool* a_value, const char* a_helpKey, const char* a_help)
		{
			ImGuiMCP::Toggle(NTR(a_key, a_label), a_value);
			HelpMarker(NTR(a_helpKey, a_help));
		}

		void __stdcall RenderSettings()
		{
			strings::Tick();
			ImGuiMCP::TextWrapped("%s", NTR("RMM_SettingsIntro", "Changes apply as soon as you make them. Press Save to keep them for the next time you play."));
			ImGuiMCP::PushItemWidth(260.0F);

			ImGuiMCP::SeparatorText(NTR("RMM_SecGeneral", "General"));
			ToggleRow("RMM_Enabled", "Draw {name} over RaceMenu", &settings::enabled, "RMM_HelpEnabled", "Off: RaceMenu shows only its own interface.");
			ToggleRow("RMM_StartInEditor", "Start in {name}", &settings::startInEditor, "RMM_HelpStartInEditor",
				"Off: RaceMenu opens in its own interface and the switch key brings {name} up.");
			ToggleRow("RMM_HidePanels", "Hide RaceMenu's own panels", &settings::hideRaceMenuPanels, "RMM_HelpHidePanels",
				"While {name} is up, RaceMenu's own panels are hidden so only the character shows beside it.");

			ImGuiMCP::SeparatorText(NTR("RMM_SecLayout", "Layout"));
			FloatRow("RMM_Scale", "Size", &settings::scale, 0.5F, 2.0F, "RMM_HelpScale", "Size of the editor's text and controls.");
			FloatRow("RMM_PanelWidth", "Panel width", &settings::panelWidth, 0.2F, 0.5F, "RMM_HelpPanelWidth", "Share of the screen width the editor takes.");
			ToggleRow("RMM_ScenePanel", "Scene and presets panel", &settings::scenePanel, "RMM_HelpScenePanel", "The panel on the right with the light and presets.");

			ImGuiMCP::SeparatorText(NTR("RMM_SecEditor", "Editor"));
			ToggleRow("RMM_AskBeforeDone", "Ask before finishing", &settings::askBeforeDone, "RMM_HelpAskBeforeDone", "Done asks before it finishes the character.");
			ToggleRow("RMM_GroupHeaders", "Group headers", &settings::groupHeaders, "RMM_HelpGroupHeaders", "Splits long categories into coloured groups.");
			ToggleRow("RMM_HideOtherBody", "Hide the other body's sliders", &settings::hideOtherBodySliders, "RMM_HelpHideOtherBody",
				"Hides body sliders that clearly belong to the other body type.");
			ToggleRow("RMM_AllowPastRange", "Allow values past the range", &settings::allowPastRange, "RMM_HelpAllowPastRange",
				"Sliders reach past the range a custom body morph registered.");
			ToggleRow("RMM_InternalNames", "Show internal names", &settings::showInternalNames, "RMM_HelpInternalNames", "Shows each slider's internal name, for mod authors.");

			ImGuiMCP::SeparatorText(NTR("RMM_SecCamera", "Camera"));
			FloatRow("RMM_TurnSpeed", "Turn speed", &settings::turnSpeed, 0.05F, 2.0F, "RMM_HelpTurnSpeed", "Degrees the character turns per pixel of drag.");

			ImGuiMCP::SeparatorText(NTR("RMM_SecControls", "Controls"));
			ImGuiMCP::Text("%s: 0x%02X", NTR("RMM_SwitchKey", "Switch key (scan code)"), settings::switchKey);
			HelpMarker(NTR("RMM_HelpSwitchKey", "Switches between {name} and RaceMenu's own interface while RaceMenu is open. F4 is 0x3E. Set it in the INI."));

			ImGuiMCP::PopItemWidth();
			ImGuiMCP::SeparatorText("");

			if (ImGuiMCP::Button(NTR("RMM_Save", "Save")))
			{
				SetStatus(NTR("RMM_Saving", "Saving..."));
				const std::string ok = NTR("RMM_Saved", "Settings saved.");
				const std::string bad = NTR("RMM_SaveFailed", "Could not write the INI. See the log.");
				OnMainThread([ok, bad]() { SetStatus(settings::Save() ? ok : bad); });
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(NTR("RMM_Reload", "Reload from INI")))
			{
				const std::string ok = NTR("RMM_Reloaded", "Settings reloaded from the INI.");
				const std::string bad = NTR("RMM_ReloadFailed", "Could not read the INI. See the log.");
				OnMainThread([ok, bad]() { SetStatus(settings::Reload() ? ok : bad); });
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(NTR("RMM_Restore", "Restore defaults")))
			{
				OnMainThread([]() { settings::RestoreDefaults(); });
				SetStatus(NTR("RMM_Restored", "Defaults restored. Press Save to keep them."));
			}
			if (const std::string status = GetStatus(); !status.empty()) { ImGuiMCP::TextWrapped("%s", status.c_str()); }
			ImGuiMCP::TextDisabled("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::warn("no menu framework (Apocrypha Menu Framework or SKSE Menu Framework 3) is installed; {} stands down and RaceMenu keeps its own interface", name::kDisplayName);
			return;
		}
		if (!HasRequiredExports())
		{
			logger::warn("the installed menu framework lacks exports the editor draws with (listed above); {} stands down", name::kDisplayName);
			return;
		}
		SKSEMenuFramework::SetSection(name::kSection);
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
		if (was != a_shown)
		{
			logger::info("editor {} ({})", a_shown ? "shown" : "hidden", a_why ? a_why : "?");
			if (bridge::IsRaceMenuOpen() && settings::hideRaceMenuPanels) { bridge::SetPanelsHidden(a_shown); }
		}
	}

	void OnRaceMenuOpened()
	{
		g_framesDrawn = 0;
		if (!g_available)
		{
			logger::info("RaceMenu opened; no editor window (see the registration lines above)");
			return;
		}
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME))
		{
			// showracemenu from the console: the console keeps the keys; the editor comes up when it closes (SPEC 2.4).
			g_showWhenConsoleCloses = settings::enabled && settings::startInEditor;
			logger::info("RaceMenu opened under the console; the editor waits for the console to close");
			return;
		}
		if (settings::enabled && settings::startInEditor) { SetShown(true, "RaceMenu opened"); }
		else { logger::info("RaceMenu opened; the editor waits for the switch key (enabled={} startInEditor={})", settings::enabled, settings::startInEditor); }
	}

	void OnRaceMenuClosed()
	{
		g_showWhenConsoleCloses = false;
		g_showWhenBoxCloses = false;
		g_finishing = false;
		SetShown(false, "RaceMenu closed");
	}

	void OnGameBox(bool a_opening)
	{
		if (a_opening)
		{
			if (IsShown())
			{
				g_showWhenBoxCloses = !g_finishing;
				SetShown(false, "a game message box opened");
			}
			return;
		}
		if (g_showWhenBoxCloses.exchange(false) && !g_finishing) { SetShown(true, "the game message box closed"); }
	}

	void OnConsoleClosed()
	{
		if (g_showWhenConsoleCloses.exchange(false)) { SetShown(true, "console closed"); }
	}

	void ToolSlider(const std::string& a_key, float a_value)
	{
		std::scoped_lock guard(g_toolSliderLock);
		g_toolSliders.emplace_back(a_key, a_value);
	}

	void ToolAnswer(bool a_yes) { g_toolAnswer = a_yes ? 1 : 2; }

	void NoteChosen(const std::string& a_label, const std::string& a_description, bool a_race)
	{
		std::scoped_lock guard(g_toolSliderLock);
		g_toolChosen = { a_label, a_description, a_race };
	}

	void SelectCategoryByName(const std::string& a_name)
	{
		std::scoped_lock guard(g_whyLock);
		g_pendingCategory = a_name;
	}

	void OnSwitchKey(const char* a_source)
	{
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
		g_finishing = false;   // the switch key after a cancelled finish brings the editor back
		SetShown(!IsShown(), a_source);
	}

	void RequestFinish() { g_finishRequested = true; }

	void SelectCategory(int a_index) { g_category = a_index; }

	std::string StateJson()
	{
		std::string why;
		{
			std::scoped_lock guard(g_whyLock);
			why = g_lastWhy;
		}
		std::string question;
		{
			std::scoped_lock guard(g_whyLock);
			question = g_questionOpen ? g_questionId : std::string();
		}
		return std::format(R"({{"available":{},"shown":{},"lastWhy":"{}","switches":{},"framesDrawn":{},"category":{},"enabled":{},"startInEditor":{},"switchKey":{},"question":"{}"}})",
			g_available.load(), IsShown(), bridge::Escape(why), g_switchCount.load(), g_framesDrawn.load(), g_category.load(), settings::enabled, settings::startInEditor,
			settings::switchKey, bridge::Escape(question));
	}
}
