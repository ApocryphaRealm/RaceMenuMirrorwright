#include "PCH.h"

#include "DevBenchTool.h"

#include "Bridge.h"
#include "DevBench/DevBenchAPI.h"
#include "Editor.h"
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
		// The string value of "op".
		std::string OpFrom(std::string_view a_args)
		{
			const auto at = a_args.find("\"op\"");
			if (at == std::string_view::npos) { return {}; }
			const auto colon = a_args.find(':', at + 4);
			if (colon == std::string_view::npos) { return {}; }
			const auto open = a_args.find('"', colon + 1);
			if (open == std::string_view::npos) { return {}; }
			const auto close = a_args.find('"', open + 1);
			if (close == std::string_view::npos) { return {}; }
			return std::string(a_args.substr(open + 1, close - open - 1));
		}

		// A whole-number field ("list":3 or "list":"3"). Missing: nothing.
		std::optional<long> NumberFrom(std::string_view a_args, const char* a_name)
		{
			const std::string key = std::string("\"") + a_name + "\"";
			const auto at = a_args.find(key);
			if (at == std::string_view::npos) { return std::nullopt; }
			auto p = a_args.find(':', at + key.size());
			if (p == std::string_view::npos) { return std::nullopt; }
			++p;
			while (p < a_args.size() && (a_args[p] == ' ' || a_args[p] == '"')) { ++p; }
			const std::string rest(a_args.substr(p, 16));
			char* end = nullptr;
			const long v = std::strtol(rest.c_str(), &end, 10);
			if (end == rest.c_str()) { return std::nullopt; }
			return v;
		}

		std::string State()
		{
			return std::format(R"({{"ok":true,"op":"state","editor":{},"bridge":{},"strings":{}}})", editor::StateJson(), bridge::StateJson(), strings::StatusJson());
		}

		// Queue a probe and wait (off the game threads - this is DevBench's listener thread) until the snapshot moves.
		std::string ProbeAndWait()
		{
			const auto before = bridge::Copy().serial;
			if (!bridge::RequestProbe()) { return R"({"ok":false,"op":"probe","error":"RaceMenu is not open"})"; }
			for (int i = 0; i < 50; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if (bridge::Copy().serial != before) { return State(); }
			}
			return R"({"ok":false,"op":"probe","error":"the UI thread did not run the probe in 5 seconds"})";
		}

		void ControlTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string args = a_argsJson ? a_argsJson : "";
			const std::string op = OpFrom(args);
			std::string out;
			if (op.empty() || op == "state") { out = State(); }
			else if (op == "probe") { out = ProbeAndWait(); }
			else if (op == "list")
			{
				const auto index = NumberFrom(args, "list").value_or(0);
				const auto from = NumberFrom(args, "from").value_or(0);
				const auto count = NumberFrom(args, "count").value_or(50);
				out = bridge::ListJson(static_cast<std::size_t>(std::max(0L, index)), static_cast<std::size_t>(std::max(0L, from)), static_cast<std::size_t>(std::max(1L, count)));
			}
			else if (op == "plugins") { out = bridge::PluginsJson(); }
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
			logger::debug("likeness.control {} -> {} bytes", op.empty() ? "state" : op, out.size());
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
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"likeness.control\" tool"); }
			else { logger::debug("DevBench not detected yet; will retry at the next message"); }
			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Likeness (a character editor over RaceMenu) live state and driver. No op / op=state: the editor window (available, shown, why, "
			"switches, frames drawn), the bridge (RaceMenu open, probes, every list found in the RaceSex Menu movie with its path, size and first-element "
			"fields) and the language. op=probe: read RaceMenu's movie again and answer when done. op=list list=<index> from=<n> count=<n>: the entries of a "
			"found list (label and number members). op=plugins: every _global.skse.plugins object and its members. op=show / op=hide: the editor window. "
			"op=switch: the switch key's action. op=reload: re-read the INI.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"list\":{\"type\":\"integer\"},\"from\":{\"type\":\"integer\"},"
			"\"count\":{\"type\":\"integer\"}},\"additionalProperties\":true},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool("likeness.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("registered \"likeness.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
		else { logger::warn("DevBench refused \"likeness.control\""); }
	}
}
