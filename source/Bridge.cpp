#include "PCH.h"

#include "Bridge.h"

#include "Editor.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <thread>
#include <unordered_set>

// Discovery of the RaceSex Menu's data (Milestone 1). Nothing here assumes where RaceMenu keeps its lists: the movie is
// walked from _root, every array whose elements are objects is recorded with its path, its size, the members of its
// first element and the label of each element, and _global.skse.plugins is listed member by member. Milestone 2 picks
// the lists it needs from this record. The walk is bounded (depth, node count) and skips the back-references that
// would loop (_parent, _root, _global, prototypes).

namespace bridge
{
	namespace
	{
		constexpr int kMaxDepth = 7;
		constexpr std::uint32_t kMaxNodes = 6000;
		constexpr std::uint32_t kMaxLists = 64;
		constexpr int kProbeTries = 20;                       // rule 17: the movie may fill its lists after the open event
		constexpr auto kProbeInterval = std::chrono::milliseconds(300);

		std::mutex g_lock;
		Snapshot g_snap;                                      // guarded by g_lock
		std::atomic<bool> g_menuOpen{ false };
		std::atomic<std::uint32_t> g_generation{ 0 };         // bumps on every open and close; a pacer of an older one stops
		std::atomic<bool> g_probeQueued{ false };
		std::uint64_t g_serial = 0;                           // guarded by g_lock; every change of g_snap takes the next one

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
				"constructor", "__constructor__", "_lockroot", "super" };
			return skip.contains(a_name);
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
				RE::GFxValue first;
				if (!a_array.GetElement(0, &first) || !(first.IsObject() || first.IsDisplayObject())) { return; }   // only lists of objects

				List list;
				list.path = a_path;
				list.size = size;
				first.VisitMembers([&list](const char* a_name, const RE::GFxValue&) {
					if (a_name && list.fieldNames.size() < 40) { list.fieldNames.emplace_back(a_name); }
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
							if (entry.label.empty() && (a_member.IsString() || a_member.IsStringW())) { entry.label = Utf8(a_member); }
							if (a_member.IsNumber() && entry.numbers.size() < 160)
							{
								if (!entry.numbers.empty()) { entry.numbers += ' '; }
								entry.numbers += std::format("{}={:g}", a_name, a_member.GetNumber());
							}
						});
					}
					list.entries.push_back(std::move(entry));
				}
				logger::debug("probe: list {} size {} fields {}", list.path, list.size, list.fieldNames.size());
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
			walker.Walk(root, "_root", 0);
			auto plugins = ReadPlugins(movie);
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();

			std::size_t entries = 0;
			for (const auto& l : walker.lists) { entries += l.entries.size(); }
			std::string status = std::format("{} lists, {} entries, {} plugin objects, {} nodes in {} ms{}", walker.lists.size(), entries, plugins.size(), walker.nodes,
				ms, walker.capped ? " (node limit reached)" : "");

			std::uint32_t count = 0;
			{
				std::scoped_lock guard(g_lock);
				g_snap.lists = std::move(walker.lists);
				g_snap.plugins = std::move(plugins);
				g_snap.nodesVisited = walker.nodes;
				g_snap.probed = true;
				count = ++g_snap.probeCount;
				g_snap.status = status;
				g_snap.serial = ++g_serial;

				// Ask the object, once per probe, and write it down (rule 30): the paths and their sizes.
				for (const auto& l : g_snap.lists) { logger::info("probe: {} [{}] first fields: {}", l.path, l.size, l.fieldNames.size()); }
				for (const auto& p : g_snap.plugins) { logger::info("probe: skse.plugins.{} has {} members", p.name, p.members.size()); }
				for (const auto& p : g_snap.plugins)
				{
					std::string names;
					for (const auto& m : p.members) { names += (names.empty() ? "" : ", ") + m; }
					logger::debug("probe: skse.plugins.{}: {}", p.name, names);
				}
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
			if (g_probeQueued.exchange(true)) { return true; }   // one is already on its way
			tasks->AddUITask([]() { ProbeNow(); });
			return true;
		}

		// Paces the first probes after an open from outside the UI queue (a UI task never re-queues itself - past mistake
		// 2026-10-25): try until the movie shows lists or the menu closes.
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
						found = g_snap.probed && !g_snap.lists.empty();
					}
					if (found) { return; }
					QueueProbe();
				}
				logger::warn("probe: no lists found in the RaceSex Menu after {} tries", kProbeTries);
			}).detach();
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
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
						g_snap.serial = ++g_serial;
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
						g_snap.status = "closed";
						g_snap.serial = ++g_serial;
					}
					logger::info("RaceSex Menu closed");
					editor::OnRaceMenuClosed();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// The switch key while the window is hidden (the game still gets the keys then). While the window is shown the
		// window reads the key itself, so this sink stands aside.
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
		return std::format(R"({{"menuOpen":{},"probed":{},"probeCount":{},"nodes":{},"status":"{}","serial":{},"lists":[{}],"plugins":{}}})", g_snap.menuOpen,
			g_snap.probed, g_snap.probeCount, g_snap.nodesVisited, Escape(g_snap.status), g_snap.serial, lists, g_snap.plugins.size());
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
			entries += std::format(R"({}{{"i":{},"label":"{}","numbers":"{}"}})", i == a_from ? "" : ",", i, Escape(l.entries[i].label), Escape(l.entries[i].numbers));
		}
		return std::format(R"({{"ok":true,"index":{},"path":"{}","size":{},"from":{},"entries":[{}]}})", a_index, Escape(l.path), l.size, a_from, entries);
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
		return std::format(R"({{"ok":true,"plugins":[{}]}})", out);
	}
}
