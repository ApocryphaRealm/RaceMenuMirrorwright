#include "PCH.h"

#include "DevBenchTool.h"

#include "Bridge.h"
#include "DevBench/DevBenchAPI.h"
#include "Editor.h"
#include "Name.h"
#include "Presets.h"
#include "Settings.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

#include <chrono>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace DevBenchTool
{
	namespace
	{
		std::optional<std::string_view> ValueStart(std::string_view a_args, const char* a_name)
		{
			const std::string key = std::string(1, '"') + a_name + '"';
			const auto at = a_args.find(key);
			if (at == std::string_view::npos) { return std::nullopt; }
			const auto colon = a_args.find(':', at + key.size());
			if (colon == std::string_view::npos) { return std::nullopt; }
			return a_args.substr(colon + 1);
		}

		// The string value of a field ("op":"state").
		std::string StringFrom(std::string_view a_args, const char* a_name)
		{
			const auto v = ValueStart(a_args, a_name);
			if (!v) { return {}; }
			const auto open = v->find('"');
			if (open == std::string_view::npos) { return {}; }
			const auto close = v->find('"', open + 1);
			if (close == std::string_view::npos) { return {}; }
			return std::string(v->substr(open + 1, close - open - 1));
		}

		std::optional<double> DoubleFrom(std::string_view a_args, const char* a_name)
		{
			auto v = ValueStart(a_args, a_name);
			if (!v) { return std::nullopt; }
			std::size_t p = 0;
			while (p < v->size() && ((*v)[p] == ' ' || (*v)[p] == '"')) { ++p; }
			const std::string rest(v->substr(p, 24));
			char* end = nullptr;
			const double d = std::strtod(rest.c_str(), &end);
			if (end == rest.c_str()) { return std::nullopt; }
			return d;
		}

		std::optional<long> NumberFrom(std::string_view a_args, const char* a_name)
		{
			const auto d = DoubleFrom(a_args, a_name);
			if (!d) { return std::nullopt; }
			return static_cast<long>(*d);
		}

		std::string State()
		{
			return std::format(R"({{"ok":true,"op":"state","editor":{},"bridge":{},"strings":{}}})", editor::StateJson(), bridge::StateJson(), strings::StatusJson());
		}

		// Waits (on DevBench's listener thread, never a game thread) until the snapshot moves or the time runs out.
		std::string WaitForChange(std::uint64_t a_before, int a_ms)
		{
			for (int waited = 0; waited < a_ms; waited += 100)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if (bridge::Copy().serial != a_before) { break; }
			}
			return State();
		}

		std::string PresetsJson()
		{
			std::string out;
			for (const auto& p : presets::List())
			{
				out += std::format(R"({}{{"name":"{}","loadable":{}}})", out.empty() ? "" : ",", bridge::Escape(p.name), p.loadable);
			}
			return std::format(R"({{"ok":true,"folder":"{}","presets":[{}]}})", bridge::Escape(presets::Folder()), out);
		}

		void ControlTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string args = a_argsJson ? a_argsJson : "";
			const std::string op = StringFrom(args, "op");
			std::string out;
			if (op.empty() || op == "state") { out = State(); }
			else if (op == "probe")
			{
				const auto before = bridge::Copy().serial;
				out = bridge::RequestProbe() ? WaitForChange(before, 5000) : R"({"ok":false,"op":"probe","error":"RaceMenu is not open"})";
			}
			else if (op == "list")
			{
				out = bridge::ListJson(static_cast<std::size_t>(std::max(0L, NumberFrom(args, "list").value_or(0))),
					static_cast<std::size_t>(std::max(0L, NumberFrom(args, "from").value_or(0))), static_cast<std::size_t>(std::max(1L, NumberFrom(args, "count").value_or(50))));
			}
			else if (op == "plugins") { out = bridge::PluginsJson(); }
			else if (op == "model")
			{
				out = bridge::ModelJson(static_cast<std::size_t>(std::max(0L, NumberFrom(args, "from").value_or(0))),
					static_cast<std::size_t>(std::max(1L, NumberFrom(args, "count").value_or(100))));
			}
			else if (op == "slider")
			{
				std::string callback = StringFrom(args, "key");
				if (callback.empty()) { callback = StringFrom(args, "callback"); }
				const auto value = DoubleFrom(args, "value");
				if (callback.empty() || !value) { out = R"({"ok":false,"error":"op=slider needs callback and value"})"; }
				else
				{
					editor::ToolSlider(callback, static_cast<float>(*value));   // the editor's path: recorded for Undo (test run 3)
					std::this_thread::sleep_for(std::chrono::milliseconds(300));
					out = State();
				}
			}
			else if (op == "confirm" || op == "cancel")
			{
				editor::ToolAnswer(op == "confirm");
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				out = State();
			}
			else if (op == "choose")
			{
				auto list = NumberFrom(args, "list");
				auto index = NumberFrom(args, "index");
				if (const std::string n = StringFrom(args, "name"); !n.empty())
				{
					const auto snap = bridge::Copy();
					for (const auto& r : snap.model.races)
					{
						if (_stricmp(r.label.c_str(), n.c_str()) == 0)
						{
							editor::NoteChosen(r.label, r.description, !r.other);
							list = static_cast<long>(r.list);
							index = static_cast<long>(r.index);
							break;
						}
					}
					if (!index) { out = std::format(R"({{"ok":false,"error":"no race or choice named '{}'"}})", bridge::Escape(n)); }
				}
				if (!out.empty()) {}
				else if (!list || !index) { out = R"({"ok":false,"error":"op=choose needs name, or list and index"})"; }
				else
				{
					const auto before = bridge::Copy().serial;
					bridge::ChooseEntry(static_cast<std::uint32_t>(*list), static_cast<std::uint32_t>(*index), StringFrom(args, "route"));
					out = WaitForChange(before, 3000);
				}
			}
			else if (op == "category")
			{
				if (const std::string n = StringFrom(args, "name"); !n.empty()) { editor::SelectCategoryByName(n); }
				else { editor::SelectCategory(static_cast<int>(NumberFrom(args, "index").value_or(0))); }
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				out = State();
			}
			else if (op == "done")
			{
				editor::RequestFinish();
				out = State();
			}
			else if (op == "presets") { out = PresetsJson(); }
			else if (op == "members")
			{
				// e.g. path=_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel.itemList._entryList.60 (a slider row)
				out = bridge::MembersJson(StringFrom(args, "path"));
			}
			else if (op == "call")
			{
				// path=<owner.function> a0= a1= a2= a3= (numbers, true/false, or text) - e.g. _global.skse.plugins.CharGen.GetRaceSexCameraPos
				std::vector<std::string> callArgs;
				for (const char* k : { "a0", "a1", "a2", "a3" })
				{
					const auto v = ValueStart(args, k);
					if (!v) { break; }
					std::string text = StringFrom(args, k);
					if (text.empty())
					{
						std::size_t e = 0;
						while (e < v->size() && (*v)[e] != ',' && (*v)[e] != '}') { ++e; }
						text = std::string(v->substr(0, e));
						while (!text.empty() && text.front() == ' ') { text.erase(0, 1); }
						while (!text.empty() && text.back() == ' ') { text.pop_back(); }
					}
					callArgs.push_back(text);
				}
				out = bridge::CallJson(StringFrom(args, "path"), callArgs);
			}
			else if (op == "show" || op == "hide")
			{
				editor::SetShown(op == "show", "devbench");
				out = State();
			}
			else if (op == "switch")
			{
				editor::OnSwitchKey("devbench");
				out = State();
			}
			else if (op == "reload")
			{
				const bool ok = settings::Reload();
				out = std::format(R"({{"ok":{},"op":"reload","ini":"{}"}})", ok ? "true" : "false", bridge::Escape(settings::GetIniPath()));
			}
			else { out = std::format(R"({{"ok":false,"error":"unknown op '{}'"}})", bridge::Escape(op)); }
			logger::debug("{} {} -> {} bytes", name::kDevBenchTool, op.empty() ? "state" : op, out.size());
			a_write(a_sink, out.c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"{}\" tool", name::kDevBenchTool); }
			else { logger::debug("DevBench not detected yet; will retry at the next message"); }
			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"RaceMenu Mirrorwright (a character editor over RaceMenu) live state and driver. No op / op=state: the editor (shown, why, switches, "
			"frames, rail entry), the bridge (RaceMenu open, probes, the model: races, sliders, changed count, which lists and member names were chosen, the "
			"onItemPress owner, the Done function, panels hidden) and every list found in the movie. op=probe: read RaceMenu again. op=list list=<i> from count: a "
			"list's entries with their strings and numbers. op=plugins: _global.skse.plugins members and the game's own handlers for the menu. op=model from count: "
			"categories, races, sliders (callback, value, range, open value, engine), function members. op=slider key=<slider key from op=model> value=<n>: move a slider as the "
			"editor does. op=choose name=<race or choice> (or list=<i> index=<n>) [route=auto|changerace|<clip path>]: press it (the log names raceID, filteredIndex, itemIndex and the owner used). op=confirm / op=cancel: answer the open question (Done, Reset, Load...). op=category index=<n>: pick a rail entry. op=done: press "
			"Done. op=presets: the preset folder and files. op=members path=<AS path>: every member of it with type and value. op=call path=<owner.function> a0..a3: call an AS function and report the result (probe). op=show / op=hide / op=switch: the editor window. op=reload: re-read the INI.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"list\":{\"type\":\"integer\"},\"index\":{\"type\":\"integer\"},"
			"\"from\":{\"type\":\"integer\"},\"count\":{\"type\":\"integer\"},\"callback\":{\"type\":\"string\"},\"value\":{\"type\":\"number\"}},\"additionalProperties\":true},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool(name::kDevBenchTool, descriptor, &ControlTool, nullptr))
		{
			logger::info("registered \"{}\" with DevBench (build {})", name::kDevBenchTool, devBench->GetBuildNumber());
			registered = true;
		}
		else { logger::warn("DevBench refused \"{}\"", name::kDevBenchTool); }
	}
}
