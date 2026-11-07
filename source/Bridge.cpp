#include "PCH.h"

#include "Bridge.h"

#include "Editor.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <map>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// The RaceSex Menu, read and driven.
//
// Reading (Milestone 1): the movie is walked from _root; every array of objects is recorded with its path, size, the
// members of its first element and each element's string and number members. _global.skse.plugins is listed. The game's
// own GameDelegate handlers for the menu are listed by name (read-only - they are never called directly: past mistake
// 2026-08-28, a handler called with made-up arguments crashes).
//
// The model (Milestone 2) is chosen from that record: the race list (the list whose entries carry raceID), the slider list
// (the list whose entries carry a value, a minimum and a maximum), the category list, and which member names hold what.
// Every choice is logged once per probe. CONFIRM IN THE TEST RUN: the member names and the lists picked.
//
// Writing: a slider sets its entry's value, then calls RaceMenu's documented RSM_SetSliderParameters on the panels clip
// (RaceMenuBase.psc's SetSliderParameters), then sends RaceMenu's documented mod event RSM_SliderChange(callback, value),
// which RaceMenu's own script and every RaceMenuBase slider plugin listen to. A slider whose callback is one of the game's
// own handlers is also passed through the movie's GameDelegate when bGameSliderCalls = 1 (off by default until the test
// run shows the argument shape). A race or another mod's choice goes through the movie's own onItemPress({index, entry})
// (past mistake 2026-10-04: never ChangeRace).

namespace bridge
{
	namespace
	{
		constexpr int kMaxDepth = 7;
		constexpr std::uint32_t kMaxNodes = 6000;
		constexpr std::uint32_t kMaxLists = 64;
		constexpr int kProbeTries = 20;
		constexpr auto kProbeInterval = std::chrono::milliseconds(300);

		std::mutex g_lock;
		Snapshot g_snap;                                      // guarded by g_lock
		std::uint64_t g_serial = 0;                           // guarded by g_lock
		std::unordered_map<std::string, float> g_openValues;  // guarded by g_lock: callback -> value when first seen this session
		std::atomic<bool> g_menuOpen{ false };
		std::atomic<std::uint32_t> g_generation{ 0 };
		std::atomic<bool> g_probeQueued{ false };

		std::mutex g_pendingLock;
		std::map<std::string, float> g_pendingSliders;        // guarded by g_pendingLock: coalesced writes, applied in one UI task
		std::atomic<bool> g_sliderTaskQueued{ false };

		void Touch() { g_snap.serial = ++g_serial; }          // caller holds g_lock

		std::string Utf8(const RE::GFxValue& a_v)
		{
			if (a_v.IsString())
			{
				const char* s = a_v.GetString();
				return s ? s : "";
			}
			if (a_v.IsStringW())
			{
				const wchar_t* w = a_v.GetStringW();
				if (!w) { return {}; }
				const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
				if (len <= 1) { return {}; }
				std::string out(static_cast<std::size_t>(len - 1), '\0');
				WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
				return out;
			}
			return {};
		}

		bool Skip(const char* a_name)
		{
			if (!a_name || !*a_name) { return true; }
			static const std::unordered_set<std::string> skip = { "_parent", "_root", "_global", "_level0", "this", "prototype", "__proto__",
				"constructor", "__constructor__", "_lockroot", "super", "_entryClipManager", "listState", "listEnumeration", "constraints", "scrollbar",
				"_clipPool", "SliderInstance" };
			return skip.contains(a_name);
		}

		const double* Num(const Entry& a_e, const std::string& a_name)
		{
			for (const auto& [k, v] : a_e.nums)
			{
				if (k == a_name) { return &v; }
			}
			return nullptr;
		}

		const std::string* Str(const Entry& a_e, const std::string& a_name)
		{
			for (const auto& [k, v] : a_e.strs)
			{
				if (k == a_name) { return &v; }
			}
			return nullptr;
		}

		struct Walker
		{
			std::vector<List> lists;
			std::uint32_t nodes = 0;
			bool capped = false;

			void RecordArray(const RE::GFxValue& a_array, const std::string& a_path)
			{
				const std::uint32_t size = a_array.GetArraySize();
				if (size == 0 || lists.size() >= kMaxLists) { return; }
				for (const auto& l : lists)
				{
					if (l.path == a_path) { return; }
				}
				RE::GFxValue first;
				if (!a_array.GetElement(0, &first) || !(first.IsObject() || first.IsDisplayObject())) { return; }

				List list;
				list.path = a_path;
				list.size = size;
				first.VisitMembers([&list](const char* a_name, const RE::GFxValue&) {
					if (a_name && list.fieldNames.size() < 48) { list.fieldNames.emplace_back(a_name); }
				});
				const std::uint32_t take = std::min(size, kMaxEntries);
				list.entries.reserve(take);
				for (std::uint32_t i = 0; i < take; ++i)
				{
					RE::GFxValue element;
					Entry entry;
					if (a_array.GetElement(i, &element) && (element.IsObject() || element.IsDisplayObject()))
					{
						RE::GFxValue text;
						if (element.GetMember("text", &text)) { entry.label = Utf8(text); }
						element.VisitMembers([&entry](const char* a_name, const RE::GFxValue& a_member) {
							if (!a_name) { return; }
							if (a_member.IsString() || a_member.IsStringW())
							{
								std::string s = Utf8(a_member);
								if (entry.label.empty()) { entry.label = s; }
								if (entry.strs.size() < 24) { entry.strs.emplace_back(a_name, std::move(s)); }
							}
							else if (a_member.IsNumber() || a_member.IsBool())
							{
								const double v = a_member.IsBool() ? (a_member.GetBool() ? 1.0 : 0.0) : a_member.GetNumber();
								if (entry.nums.size() < 24) { entry.nums.emplace_back(a_name, v); }
								if (entry.numbers.size() < 160)
								{
									if (!entry.numbers.empty()) { entry.numbers += ' '; }
									entry.numbers += std::format("{}={:g}", a_name, v);
								}
							}
						});
					}
					list.entries.push_back(std::move(entry));
				}
				lists.push_back(std::move(list));
			}

			void Walk(const RE::GFxValue& a_value, const std::string& a_path, int a_depth)
			{
				if (a_depth > kMaxDepth) { return; }
				if (++nodes > kMaxNodes)
				{
					capped = true;
					return;
				}
				std::vector<std::pair<std::string, RE::GFxValue>> children;
				a_value.VisitMembers([&children](const char* a_name, const RE::GFxValue& a_member) {
					if (Skip(a_name)) { return; }
					if (a_member.IsArray() || a_member.IsObject() || a_member.IsDisplayObject()) { children.emplace_back(a_name, a_member); }
				});
				for (auto& [name, child] : children)
				{
					const std::string path = a_path + "." + name;
					if (child.IsArray()) { RecordArray(child, path); }
					else { Walk(child, path, a_depth + 1); }
					if (capped) { return; }
				}
			}
		};

		std::vector<Plugin> ReadPlugins(RE::GFxMovieView* a_movie)
		{
			std::vector<Plugin> out;
			RE::GFxValue plugins;
			if (!a_movie->GetVariable(&plugins, "_global.skse.plugins") || !(plugins.IsObject() || plugins.IsDisplayObject()))
			{
				logger::warn("probe: _global.skse.plugins is not there; RaceMenu's Scaleform functions cannot be listed");
				return out;
			}
			plugins.VisitMembers([&out](const char* a_name, const RE::GFxValue& a_member) {
				if (!a_name) { return; }
				Plugin p;
				p.name = a_name;
				if (a_member.IsObject() || a_member.IsDisplayObject())
				{
					a_member.VisitMembers([&p](const char* a_fn, const RE::GFxValue&) {
						if (a_fn) { p.members.emplace_back(a_fn); }
					});
				}
				out.push_back(std::move(p));
			});
			return out;
		}

		// Object-typed (not display object, not array) members of a clip: its functions and plain objects.
		std::vector<std::string> FunctionsOf(RE::GFxMovieView* a_movie, const char* a_path)
		{
			std::vector<std::string> out;
			RE::GFxValue clip;
			if (!a_movie->GetVariable(&clip, a_path) || !(clip.IsObject() || clip.IsDisplayObject())) { return out; }
			clip.VisitMembers([&out, a_path](const char* a_name, const RE::GFxValue& a_member) {
				if (a_name && a_member.GetType() == RE::GFxValue::ValueType::kObject) { out.push_back(std::string(a_path) + "." + a_name); }
			});
			return out;
		}

		std::vector<std::string> DelegateCallbacks(RE::IMenu* a_menu)
		{
			std::vector<std::string> out;
			if (!a_menu || !a_menu->fxDelegate) { return out; }
			for (const auto& node : a_menu->fxDelegate->callbacks) { out.emplace_back(node.first.c_str()); }
			std::sort(out.begin(), out.end());
			return out;
		}

		std::string FirstPresent(const List& a_list, std::initializer_list<const char*> a_names)
		{
			for (const char* n : a_names)
			{
				if (std::find(a_list.fieldNames.begin(), a_list.fieldNames.end(), n) != a_list.fieldNames.end()) { return n; }
			}
			return {};
		}

		std::string Pretty(std::string a_label)
		{
			// RaceMenu's labels can be translation keys ("$HEAD"); the translated text lives in RaceMenu's BSA, which cannot be
			// read here. Show the key without its '$', and an all-capitals key in title case ("$HEAD" -> "Head").
			if (!a_label.empty() && a_label.front() == '$')
			{
				a_label.erase(0, 1);
				const bool caps = std::none_of(a_label.begin(), a_label.end(), [](char c) { return c >= 'a' && c <= 'z'; });
				if (caps)
				{
					bool start = true;
					for (char& c : a_label)
					{
						if (c == '_' || c == ' ') { c = ' '; start = true; continue; }
						if (!start && c >= 'A' && c <= 'Z') { c = static_cast<char>(c - 'A' + 'a'); }
						start = false;
					}
				}
			}
			return a_label;
		}

		bool IsSliderEntry(const Entry& a_e) { return Num(a_e, "sliderMin") && Num(a_e, "sliderMax") && Num(a_e, "position"); }

		Model BuildModel(const std::vector<List>& a_lists, std::vector<std::string> a_engine, std::vector<std::string> a_functions)
		{
			Model m;
			m.engineCallbacks = std::move(a_engine);
			m.baseFunctions = std::move(a_functions);

			// The item list: the documented path when present (test run 2026-11-06), else the largest list carrying raceID.
			std::size_t best = 0;
			for (std::size_t i = 0; i < a_lists.size(); ++i)
			{
				const auto& l = a_lists[i];
				if (l.path == kItemList)
				{
					m.raceList = static_cast<int>(i);
					break;
				}
				if (!FirstPresent(l, { "raceID" }).empty() && l.size > best)
				{
					best = l.size;
					m.raceList = static_cast<int>(i);
				}
			}
			for (std::size_t i = 0; i < a_lists.size(); ++i)
			{
				if (a_lists[i].path == kCategoryListPath) { m.categoryList = static_cast<int>(i); }
			}
			if (m.categoryList < 0)
			{
				for (std::size_t i = 0; i < a_lists.size(); ++i)
				{
					if (a_lists[i].path.find("categoryList") != std::string::npos && !FirstPresent(a_lists[i], { "flag" }).empty())
					{
						m.categoryList = static_cast<int>(i);
						break;
					}
				}
			}
			m.sliderList = m.raceList;   // one list holds both (test run 2026-11-06)
			m.valueField = "position";
			m.minField = "sliderMin";
			m.maxField = "sliderMax";
			m.stepField = "interval";
			m.callbackField = "callbackName";
			m.flagField = "filterFlag";

			if (m.raceList >= 0)
			{
				const auto& l = a_lists[static_cast<std::size_t>(m.raceList)];
				for (std::uint32_t i = 0; i < l.entries.size(); ++i)
				{
					const auto& e = l.entries[i];
					const double* f = Num(e, "filterFlag");
					const std::uint32_t flag = f ? static_cast<std::uint32_t>(*f) : 0;
					if (IsSliderEntry(e))
					{
						Slider s;
						s.label = Pretty(e.label);
						s.list = static_cast<std::uint32_t>(m.raceList);
						s.index = i;
						s.flag = flag;
						s.value = static_cast<float>(*Num(e, "position"));
						s.min = static_cast<float>(*Num(e, "sliderMin"));
						s.max = static_cast<float>(*Num(e, "sliderMax"));
						if (const double* st = Num(e, "interval"); st && *st > 0.0) { s.step = static_cast<float>(*st); }
						if (const std::string* cb = Str(e, "callbackName")) { s.callback = *cb; }
						if (const double* id = Num(e, "sliderID")) { s.sliderID = *id; }
						s.key = s.callback + "|" + (std::isnan(s.sliderID) ? e.label : std::format("{:g}", s.sliderID));
						s.engine = std::binary_search(m.engineCallbacks.begin(), m.engineCallbacks.end(), s.callback);
						m.sliders.push_back(std::move(s));
						continue;
					}
					if (e.label.empty() || !Num(e, "raceID")) { continue; }   // empty rows were the blank tiles of the test run
					Race r;
					r.label = Pretty(e.label);
					r.list = static_cast<std::uint32_t>(m.raceList);
					r.index = i;
					r.flag = flag;
					r.raceID = *Num(e, "raceID");
					r.other = (flag & kCategoryRace) == 0;   // filed under another mod's category (Apprentice)
					if (const std::string* d = Str(e, "raceDescription")) { r.description = *d; }
					else if (const std::string* d2 = Str(e, "description")) { r.description = *d2; }
					m.races.push_back(std::move(r));
				}
			}

			if (m.categoryList >= 0)
			{
				const auto& l = a_lists[static_cast<std::size_t>(m.categoryList)];
				for (const auto& e : l.entries)
				{
					const double* f = Num(e, "flag");
					if (!f || e.label.empty() || e.label == "$ALL") { continue; }
					const auto flag = static_cast<std::uint32_t>(*f);
					if (flag == 0 || flag == 0xFFFFFFFF) { continue; }
					m.categories.push_back({ Pretty(e.label), flag });
				}
			}
			if (m.categories.empty())
			{
				m.categories = { { "Race", 2 }, { "Body", 4 }, { "Head", 8 }, { "Face", 16 }, { "Eyes", 32 }, { "Brow", 64 }, { "Mouth", 128 }, { "Hair", 256 },
					{ "Extra", 512 } };
			}

			// Done: sDoneFunction when set; otherwise the game's own ConfirmDone handler (found in the test run), called through
			// the movie's GameDelegate like the movie's own Done.
			m.doneFunction = settings::doneFunction;
			if (m.doneFunction.empty() && std::binary_search(m.engineCallbacks.begin(), m.engineCallbacks.end(), std::string("ConfirmDone")))
			{
				m.doneFunction = "GameDelegate:ConfirmDone";
			}
			return m;
		}

		void LogModel(const Snapshot& a_s)
		{
			const auto& m = a_s.model;
			auto path = [&a_s](int i) { return i >= 0 ? a_s.lists[static_cast<std::size_t>(i)].path : std::string("(none)"); };
			logger::info("model: race list {} ({} rows), slider list {} ({} sliders), category list {} ({} categories)", path(m.raceList), m.races.size(), path(m.sliderList),
				m.sliders.size(), path(m.categoryList), m.categories.size());
			logger::info("model: fields value='{}' min='{}' max='{}' step='{}' callback='{}' flag='{}'", m.valueField, m.minField, m.maxField, m.stepField, m.callbackField,
				m.flagField);
			const auto engine = std::count_if(m.sliders.begin(), m.sliders.end(), [](const Slider& s) { return s.engine; });
			logger::info("model: {} game handlers registered for the menu; {} sliders use one", m.engineCallbacks.size(), engine);
			logger::info("model: Done function '{}' ({} function members looked at)", m.doneFunction.empty() ? "(not found)" : m.doneFunction, m.baseFunctions.size());
			for (const auto& f : m.baseFunctions) { logger::debug("model: function {}", f); }
			for (const auto& c : m.engineCallbacks) { logger::debug("model: game handler {}", c); }
		}

		// UI thread only.
		void ProbeNow()
		{
			g_probeQueued = false;
			auto* ui = RE::UI::GetSingleton();
			if (!ui)
			{
				logger::warn("probe: no UI singleton");
				return;
			}
			auto menu = ui->GetMenu(RE::RaceSexMenu::MENU_NAME);
			if (!menu || !menu->uiMovie)
			{
				logger::debug("probe: the RaceSex Menu or its movie is not there (open={})", g_menuOpen.load());
				return;
			}
			RE::GFxMovieView* movie = menu->uiMovie.get();
			RE::GFxValue root;
			if (!movie->GetVariable(&root, "_root") || !(root.IsObject() || root.IsDisplayObject()))
			{
				logger::warn("probe: the RaceSex Menu movie has no _root");
				return;
			}

			const auto started = std::chrono::steady_clock::now();
			Walker walker;
			for (const char* known : { kItemList, kCategoryListPath })
			{
				RE::GFxValue array;
				if (movie->GetVariable(&array, known) && array.IsArray()) { walker.RecordArray(array, known); }
				else { logger::warn("probe: {} is not there (RaceMenu version or reskin differs) - falling back to the walk", known); }
			}
			walker.Walk(root, "_root", 0);
			auto plugins = ReadPlugins(movie);
			auto functions = FunctionsOf(movie, kBaseRoot);
			auto panelFns = FunctionsOf(movie, kMenuRoot);
			functions.insert(functions.end(), panelFns.begin(), panelFns.end());
			Model model = BuildModel(walker.lists, DelegateCallbacks(menu.get()), std::move(functions));
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();

			std::string status = std::format("{} races, {} sliders, {} categories ({} lists, {} nodes, {} ms{})", model.races.size(), model.sliders.size(),
				model.categories.size(), walker.lists.size(), walker.nodes, ms, walker.capped ? ", node limit reached" : "");

			std::uint32_t count = 0;
			{
				std::scoped_lock guard(g_lock);
				for (auto& s : model.sliders)
				{
					auto [it, inserted] = g_openValues.try_emplace(s.key, s.value);
					s.openValue = it->second;
				}
				g_snap.lists = std::move(walker.lists);
				g_snap.plugins = std::move(plugins);
				g_snap.model = std::move(model);
				g_snap.nodesVisited = walker.nodes;
				g_snap.probed = true;
				count = ++g_snap.probeCount;
				g_snap.status = status;
				Touch();
				for (const auto& l : g_snap.lists) { logger::debug("probe: {} [{}]", l.path, l.size); }
				for (const auto& p : g_snap.plugins) { logger::info("probe: skse.plugins.{} has {} members", p.name, p.members.size()); }
				LogModel(g_snap);
			}
			logger::info("probe {}: {}", count, status);
		}

		bool QueueProbe()
		{
			if (!g_menuOpen) { return false; }
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks)
			{
				logger::error("no SKSE task interface; cannot read the RaceSex Menu");
				return false;
			}
			if (g_probeQueued.exchange(true)) { return true; }
			tasks->AddUITask([]() { ProbeNow(); });
			return true;
		}

		void StartPacer()
		{
			const std::uint32_t generation = g_generation.load();
			std::thread([generation]() {
				for (int i = 0; i < kProbeTries; ++i)
				{
					std::this_thread::sleep_for(kProbeInterval);
					if (g_generation.load() != generation || !g_menuOpen) { return; }
					bool found = false;
					{
						std::scoped_lock guard(g_lock);
						found = g_snap.probed && (!g_snap.model.sliders.empty() || !g_snap.model.races.empty());
					}
					if (found) { return; }
					QueueProbe();
				}
				logger::warn("probe: no races or sliders found in the RaceSex Menu after {} tries", kProbeTries);
			}).detach();
		}

		RE::GFxMovieView* Movie()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) { return nullptr; }
			auto menu = ui->GetMenu(RE::RaceSexMenu::MENU_NAME);
			return menu && menu->uiMovie ? menu->uiMovie.get() : nullptr;   // the menu keeps the movie alive while open
		}

		void SendSliderEvent(const std::string& a_callback, float a_value)
		{
			auto* source = SKSE::GetModCallbackEventSource();
			if (!source)
			{
				logger::warn("no mod event source; RSM_SliderChange for {} not sent", a_callback);
				return;
			}
			SKSE::ModCallbackEvent event{ "RSM_SliderChange", RE::BSFixedString(a_callback.c_str()), a_value, nullptr };
			source->SendEvent(&event);
		}

		// UI thread: apply every pending slider value.
		void ApplyPendingSliders()
		{
			g_sliderTaskQueued = false;
			std::map<std::string, float> pending;
			{
				std::scoped_lock guard(g_pendingLock);
				pending.swap(g_pendingSliders);
			}
			RE::GFxMovieView* movie = Movie();
			if (!movie)
			{
				logger::debug("slider apply: the RaceSex Menu is gone; {} change(s) dropped", pending.size());
				return;
			}
			Snapshot snap = Copy();
			RE::GFxValue panels;
			const bool havePanels = movie->GetVariable(&panels, kMenuRoot) && (panels.IsObject() || panels.IsDisplayObject());
			RE::GFxValue setParams;
			const bool haveSetParams = havePanels && panels.GetMember("RSM_SetSliderParameters", &setParams) && setParams.IsObject();
			static bool loggedOnce = false;
			if (!loggedOnce)
			{
				loggedOnce = true;
				logger::info("slider apply: panels clip {}, RSM_SetSliderParameters {}", havePanels ? "found" : "MISSING", haveSetParams ? "found" : "MISSING");
			}

			for (const auto& [callback, value] : pending)
			{
				const auto it = std::find_if(snap.model.sliders.begin(), snap.model.sliders.end(), [&](const Slider& s) { return s.key == callback; });
				if (it == snap.model.sliders.end() || it->list >= snap.lists.size())
				{
					logger::warn("slider apply: no slider '{}' in the model", callback);
					continue;
				}
				const Slider& s = *it;
				RE::GFxValue array;
				RE::GFxValue element;
				if (movie->GetVariable(&array, snap.lists[s.list].path.c_str()) && array.IsArray() && array.GetElement(s.index, &element) &&
					(element.IsObject() || element.IsDisplayObject()) && !snap.model.valueField.empty())
				{
					element.SetMember(snap.model.valueField.c_str(), RE::GFxValue(static_cast<double>(value)));
				}
				else { logger::debug("slider apply: entry {}[{}] not reachable; value member not set", snap.lists[s.list].path, s.index); }

				if (haveSetParams && !s.engine && !s.callback.empty())
				{
					RE::GFxValue params;
					movie->CreateArray(&params);
					for (const std::string& part : { s.callback, std::format("{}", s.min), std::format("{}", s.max), std::format("{}", s.step), std::format("{}", value) })
					{
						RE::GFxValue str;
						movie->CreateString(&str, part.c_str());
						params.PushBack(str);
					}
					panels.Invoke("RSM_SetSliderParameters", nullptr, &params, 1);
				}
				if (!s.engine && !s.callback.empty()) { SendSliderEvent(s.callback, value); }   // RaceMenu plugin sliders

				if (s.engine && settings::engineSliderCalls)
				{
					// Through the movie's own GameDelegate, never the handler directly (past mistake 2026-08-28). The argument
					// shape (the value alone) is a guess until the test run - hence off by default.
					RE::GFxValue delegate;
					if (movie->GetVariable(&delegate, "_global.gfx.io.GameDelegate") && delegate.IsObject())
					{
						RE::GFxValue args[2];
						movie->CreateString(&args[0], s.callback.c_str());
						movie->CreateArray(&args[1]);
						args[1].PushBack(RE::GFxValue(static_cast<double>(value)));
						if (!std::isnan(s.sliderID)) { args[1].PushBack(RE::GFxValue(s.sliderID)); }
						delegate.Invoke("call", nullptr, args, 2);
					}
				}
				logger::debug("slider {} ({}) = {:.3f}", s.label, s.callback, value);
			}
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening && a_event->menuName == RE::Console::MENU_NAME && g_menuOpen) { editor::OnConsoleClosed(); }
				if (!a_event || a_event->menuName != RE::RaceSexMenu::MENU_NAME) { return RE::BSEventNotifyControl::kContinue; }
				if (a_event->opening)
				{
					g_menuOpen = true;
					++g_generation;
					{
						std::scoped_lock guard(g_lock);
						g_snap = Snapshot{};
						g_snap.menuOpen = true;
						g_snap.status = "waiting for the menu's data";
						g_openValues.clear();
						Touch();
					}
					logger::info("RaceSex Menu opened");
					editor::OnRaceMenuOpened();
					StartPacer();
				}
				else
				{
					g_menuOpen = false;
					++g_generation;
					{
						std::scoped_lock guard(g_lock);
						g_snap.menuOpen = false;
						g_snap.lists.clear();
						g_snap.plugins.clear();
						g_snap.model = Model{};
						g_snap.panelsHidden = false;
						g_snap.status = "closed";
						Touch();
					}
					logger::info("RaceSex Menu closed");
					editor::OnRaceMenuClosed();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!a_events || !g_menuOpen || settings::switchKey == 0 || editor::IsShown()) { return RE::BSEventNotifyControl::kContinue; }
				for (auto* e = *a_events; e; e = e->next)
				{
					if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || e->GetDevice() != RE::INPUT_DEVICE::kKeyboard) { continue; }
					const auto* button = e->AsButtonEvent();
					if (!button || !button->IsDown() || button->GetIDCode() != settings::switchKey) { continue; }
					if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME))
					{
						logger::debug("switch key ignored: the console is open");
						continue;
					}
					editor::OnSwitchKey("game input");
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		static MenuSink menuSink;
		static InputSink inputSink;
		static bool installed = false;
		if (installed) { return; }

		auto* ui = RE::UI::GetSingleton();
		if (!ui)
		{
			logger::error("no UI singleton at kDataLoaded; Likeness cannot see RaceMenu open");
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
		logger::info("watching the RaceSex Menu");

		if (auto* input = RE::BSInputDeviceManager::GetSingleton())
		{
			input->AddEventSink(&inputSink);
			logger::info("switch key sink installed (scan code 0x{:02X})", settings::switchKey);
		}
		else { logger::warn("no input device manager; the switch key works only while the editor is shown"); }
		installed = true;
	}

	bool IsRaceMenuOpen() { return g_menuOpen; }

	bool RequestProbe() { return QueueProbe(); }

	Snapshot Copy()
	{
		std::scoped_lock guard(g_lock);
		return g_snap;
	}

	bool CopyIfChanged(std::uint64_t& a_serial, Snapshot& a_out)
	{
		std::scoped_lock guard(g_lock);
		if (g_snap.serial == a_serial) { return false; }
		a_out = g_snap;
		a_serial = g_snap.serial;
		return true;
	}

	void SetSlider(const std::string& a_callback, float a_value)
	{
		if (!g_menuOpen) { return; }
		{
			std::scoped_lock guard(g_lock);
			for (auto& s : g_snap.model.sliders)
			{
				if (s.key == a_callback) { s.value = a_value; }
			}
			Touch();
		}
		{
			std::scoped_lock guard(g_pendingLock);
			g_pendingSliders[a_callback] = a_value;
		}
		if (g_sliderTaskQueued.exchange(true)) { return; }
		if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddUITask([]() { ApplyPendingSliders(); }); }
		else { g_sliderTaskQueued = false; }
	}

	void ChooseEntry(std::uint32_t a_list, std::uint32_t a_index)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks || !g_menuOpen) { return; }
		tasks->AddUITask([a_list, a_index]() {
			RE::GFxMovieView* movie = Movie();
			if (!movie) { return; }
			const Snapshot snap = Copy();
			if (a_list >= snap.lists.size())
			{
				logger::warn("choose: list {} is not in the snapshot", a_list);
				return;
			}
			const std::string& path = snap.lists[a_list].path;
			RE::GFxValue array, element;
			if (!movie->GetVariable(&array, path.c_str()) || !array.IsArray() || !array.GetElement(a_index, &element))
			{
				logger::warn("choose: {}[{}] not reachable", path, a_index);
				return;
			}
			// onItemPress on the nearest owner: the list's clip, its parent, or the documented panels clip.
			std::vector<std::string> owners;
			std::string p = path;
			for (int up = 0; up < 3; ++up)
			{
				const auto dot = p.rfind('.');
				if (dot == std::string::npos) { break; }
				p = p.substr(0, dot);
				owners.push_back(p);
			}
			owners.emplace_back(kMenuRoot);
			for (const auto& owner : owners)
			{
				RE::GFxValue clip, fn;
				if (!movie->GetVariable(&clip, owner.c_str()) || !(clip.IsObject() || clip.IsDisplayObject())) { continue; }
				if (!clip.GetMember("onItemPress", &fn) || !fn.IsObject()) { continue; }
				RE::GFxValue arg;
				movie->CreateObject(&arg);
				arg.SetMember("index", RE::GFxValue(static_cast<double>(a_index)));
				arg.SetMember("entry", element);
				clip.Invoke("onItemPress", nullptr, &arg, 1);
				logger::info("choose: {}[{}] through {}.onItemPress", path, a_index, owner);
				{
					std::scoped_lock guard(g_lock);
					g_snap.model.itemPressOwner = owner;
					Touch();
				}
				ReprobeSoon(600);   // a race change rebuilds RaceMenu's sliders
				return;
			}
			logger::warn("choose: no onItemPress found above {} - CONFIRM IN THE TEST RUN where RaceMenu's list press lives", path);
		});
	}

	void SetPanelsHidden(bool a_hidden)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return; }
		tasks->AddUITask([a_hidden]() {
			RE::GFxMovieView* movie = Movie();
			if (!movie) { return; }
			RE::GFxValue base;
			if (!movie->GetVariable(&base, kBaseRoot) || !base.IsDisplayObject())
			{
				logger::warn("panels: {} is not a clip; RaceMenu's own panels stay as they are", kBaseRoot);
				return;
			}
			base.SetMember("_visible", RE::GFxValue(!a_hidden));
			{
				std::scoped_lock guard(g_lock);
				g_snap.panelsHidden = a_hidden;
				Touch();
			}
			logger::info("RaceMenu's own panels {}", a_hidden ? "hidden" : "shown");
		});
	}

	bool Finish()
	{
		std::string fn;
		{
			std::scoped_lock guard(g_lock);
			fn = g_snap.model.doneFunction;
		}
		if (fn.empty())
		{
			logger::warn("Done: no Done function known (set sDoneFunction in the INI once the test run names it)");
			return false;
		}
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return false; }
		tasks->AddUITask([fn]() {
			RE::GFxMovieView* movie = Movie();
			if (!movie) { return; }
			if (fn.starts_with("GameDelegate:"))
			{
				RE::GFxValue delegate;
				if (!movie->GetVariable(&delegate, "_global.gfx.io.GameDelegate") || !delegate.IsObject())
				{
					logger::warn("Done: the movie has no GameDelegate");
					return;
				}
				RE::GFxValue args[2];
				movie->CreateString(&args[0], fn.substr(13).c_str());
				movie->CreateArray(&args[1]);
				delegate.Invoke("call", nullptr, args, 2);
				logger::info("Done: GameDelegate.call(\"{}\")", fn.substr(13));
				return;
			}
			const auto dot = fn.rfind('.');
			RE::GFxValue owner;
			if (dot == std::string::npos || !movie->GetVariable(&owner, fn.substr(0, dot).c_str()))
			{
				logger::warn("Done: {} not reachable", fn);
				return;
			}
			owner.Invoke(fn.substr(dot + 1).c_str());
			logger::info("Done: called {}", fn);
		});
		return true;
	}

	// Turning (test run 2026-11-06 20:41: CharGen.GetPlayerRotation gave no number, so RaceMenu's Scaleform rotation is not
	// used). The player's own heading is turned on the main thread with Actor::SetHeading. Presses arrive every frame while a
	// key or the stick is held; they add up here and one main-thread task applies the sum.
	namespace
	{
		std::mutex g_turnLock;
		float g_turnPending = 0.0F;          // guarded by g_turnLock, degrees
		std::atomic<bool> g_turnQueued{ false };
	}

	void TurnBy(float a_degrees)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks || !g_menuOpen || a_degrees == 0.0F) { return; }
		{
			std::scoped_lock guard(g_turnLock);
			g_turnPending += a_degrees;
		}
		if (g_turnQueued.exchange(true)) { return; }
		tasks->AddTask([]() {
			g_turnQueued = false;
			float degrees = 0.0F;
			{
				std::scoped_lock guard(g_turnLock);
				degrees = g_turnPending;
				g_turnPending = 0.0F;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || degrees == 0.0F) { return; }
			constexpr float kPi = 3.14159265F;
			const float before = player->GetAngleZ();
			float after = before + degrees * kPi / 180.0F;
			while (after < 0.0F) { after += 2.0F * kPi; }
			while (after >= 2.0F * kPi) { after -= 2.0F * kPi; }
			player->SetHeading(after);
			static bool logged = false;
			if (!logged)
			{
				logged = true;
				logger::info("turn: player heading {:.3f} -> {:.3f} rad (Actor::SetHeading; first turn of the session)", before, after);
			}
			logger::debug("turn: {:.2f} degrees, heading {:.3f} -> {:.3f}", degrees, before, player->GetAngleZ());
		});
	}

	void ToggleLight()
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return; }
		tasks->AddTask([]() {
			if (auto* source = SKSE::GetModCallbackEventSource())
			{
				SKSE::ModCallbackEvent event{ "RSM_ToggleLight", "", 0.0F, nullptr };
				source->SendEvent(&event);
				logger::info("light: RSM_ToggleLight sent");
			}
		});
	}

	void ReprobeSoon(int a_delayMs)
	{
		const std::uint32_t generation = g_generation.load();
		std::thread([generation, a_delayMs]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(a_delayMs));
			if (g_generation.load() == generation) { QueueProbe(); }
		}).detach();
	}

	std::string Escape(const std::string& a_text)
	{
		std::string out;
		out.reserve(a_text.size() + 8);
		for (const char c : a_text)
		{
			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) { out += std::format("\\u{:04x}", static_cast<int>(c)); }
				else { out += c; }
			}
		}
		return out;
	}

	std::string StateJson()
	{
		std::scoped_lock guard(g_lock);
		std::string lists;
		for (std::size_t i = 0; i < g_snap.lists.size(); ++i)
		{
			const auto& l = g_snap.lists[i];
			std::string fields;
			for (const auto& f : l.fieldNames) { fields += std::format("{}\"{}\"", fields.empty() ? "" : ",", Escape(f)); }
			lists += std::format(R"({}{{"index":{},"path":"{}","size":{},"fields":[{}],"first":"{}"}})", i ? "," : "", i, Escape(l.path), l.size, fields,
				Escape(l.entries.empty() ? std::string() : l.entries.front().label));
		}
		const auto& m = g_snap.model;
		const auto changed = std::count_if(m.sliders.begin(), m.sliders.end(), [](const Slider& s) { return !std::isnan(s.openValue) && s.value != s.openValue; });
		const auto otherChoices = std::count_if(m.races.begin(), m.races.end(), [](const Race& r) { return r.other; });
		return std::format(
			R"({{"menuOpen":{},"probed":{},"probeCount":{},"nodes":{},"status":"{}","serial":{},"panelsHidden":{},"model":{{"races":{},"sliders":{},"changed":{},"choices":{},"categories":{},"raceList":{},"sliderList":{},"categoryList":{},"valueField":"{}","callbackField":"{}","flagField":"{}","itemPressOwner":"{}","doneFunction":"{}","engineHandlers":{}}},"lists":[{}],"plugins":{}}})",
			g_snap.menuOpen, g_snap.probed, g_snap.probeCount, g_snap.nodesVisited, Escape(g_snap.status), g_snap.serial, g_snap.panelsHidden, m.races.size(), m.sliders.size(),
			changed, otherChoices, m.categories.size(), m.raceList, m.sliderList, m.categoryList, Escape(m.valueField), Escape(m.callbackField), Escape(m.flagField),
			Escape(m.itemPressOwner), Escape(m.doneFunction), m.engineCallbacks.size(), lists, g_snap.plugins.size());
	}

	std::string ListJson(std::size_t a_index, std::size_t a_from, std::size_t a_count)
	{
		std::scoped_lock guard(g_lock);
		if (a_index >= g_snap.lists.size()) { return std::format(R"({{"ok":false,"error":"no list {}; {} found"}})", a_index, g_snap.lists.size()); }
		const auto& l = g_snap.lists[a_index];
		std::string entries;
		const std::size_t end = std::min(l.entries.size(), a_from + std::min<std::size_t>(a_count, 200));
		for (std::size_t i = a_from; i < end; ++i)
		{
			std::string strs;
			for (const auto& [k, v] : l.entries[i].strs) { strs += std::format("{}\"{}\":\"{}\"", strs.empty() ? "" : ",", Escape(k), Escape(v)); }
			entries += std::format(R"({}{{"i":{},"label":"{}","numbers":"{}","strings":{{{}}}}})", i == a_from ? "" : ",", i, Escape(l.entries[i].label),
				Escape(l.entries[i].numbers), strs);
		}
		return std::format(R"({{"ok":true,"index":{},"path":"{}","size":{},"from":{},"entries":[{}]}})", a_index, Escape(l.path), l.size, a_from, entries);
	}

	std::string ModelJson(std::size_t a_from, std::size_t a_count)
	{
		std::scoped_lock guard(g_lock);
		const auto& m = g_snap.model;
		std::string cats, races, sliders, fns;
		for (const auto& c : m.categories) { cats += std::format(R"({}{{"label":"{}","flag":{}}})", cats.empty() ? "" : ",", Escape(c.label), c.flag); }
		for (const auto& r : m.races)
		{
			races += std::format(R"({}{{"i":{},"label":"{}","flag":{},"raceID":{},"other":{}}})", races.empty() ? "" : ",", r.index, Escape(r.label), r.flag, r.raceID, r.other);
		}
		const std::size_t end = std::min(m.sliders.size(), a_from + std::min<std::size_t>(a_count, 200));
		for (std::size_t i = a_from; i < end; ++i)
		{
			const auto& s = m.sliders[i];
			sliders += std::format(R"({}{{"i":{},"label":"{}","key":"{}","callback":"{}","flag":{},"value":{},"min":{},"max":{},"step":{},"open":{},"engine":{}}})", sliders.empty() ? "" : ",",
				s.index, Escape(s.label), Escape(s.key), Escape(s.callback), s.flag, s.value, s.min, s.max, s.step, std::isnan(s.openValue) ? 0.0F : s.openValue, s.engine);
		}
		for (const auto& f : m.baseFunctions) { fns += std::format("{}\"{}\"", fns.empty() ? "" : ",", Escape(f)); }
		return std::format(R"({{"ok":true,"categories":[{}],"races":[{}],"slidersFrom":{},"sliders":[{}],"functions":[{}]}})", cats, races, a_from, sliders, fns);
	}

	std::string PluginsJson()
	{
		std::scoped_lock guard(g_lock);
		std::string out;
		for (const auto& p : g_snap.plugins)
		{
			std::string members;
			for (const auto& m : p.members) { members += std::format("{}\"{}\"", members.empty() ? "" : ",", Escape(m)); }
			out += std::format(R"({}{{"name":"{}","members":[{}]}})", out.empty() ? "" : ",", Escape(p.name), members);
		}
		std::string engine;
		for (const auto& c : g_snap.model.engineCallbacks) { engine += std::format("{}\"{}\"", engine.empty() ? "" : ",", Escape(c)); }
		return std::format(R"({{"ok":true,"plugins":[{}],"gameHandlers":[{}]}})", out, engine);
	}
}
