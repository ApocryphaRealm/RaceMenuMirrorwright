#include "PCH.h"

#include "Presets.h"

#include "Bridge.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <functional>

namespace presets
{
	namespace
	{
		std::filesystem::path FolderPath() { return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "CharGen" / "Presets"; }

		std::wstring Wide(const std::string& a_utf8)
		{
			if (a_utf8.empty()) { return {}; }
			const int len = MultiByteToWideChar(CP_UTF8, 0, a_utf8.c_str(), -1, nullptr, 0);
			std::wstring out(static_cast<std::size_t>(std::max(len - 1, 0)), L'\0');
			if (len > 1) { MultiByteToWideChar(CP_UTF8, 0, a_utf8.c_str(), -1, out.data(), len); }
			return out;
		}

		std::string Utf8(const std::wstring& a_w)
		{
			if (a_w.empty()) { return {}; }
			const int len = WideCharToMultiByte(CP_UTF8, 0, a_w.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string out(static_cast<std::size_t>(std::max(len - 1, 0)), '\0');
			if (len > 1) { WideCharToMultiByte(CP_UTF8, 0, a_w.c_str(), -1, out.data(), len, nullptr, nullptr); }
			return out;
		}

		// The system code page form, or false when a character has no exact form there (WC_NO_BEST_FIT_CHARS + usedDefault).
		bool Ansi(const std::wstring& a_w, std::string& a_out)
		{
			if (a_w.empty()) { return false; }
			BOOL usedDefault = FALSE;
			const int len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, a_w.c_str(), -1, nullptr, 0, nullptr, &usedDefault);
			if (len <= 1 || usedDefault) { return false; }
			a_out.assign(static_cast<std::size_t>(len - 1), '\0');
			WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, a_w.c_str(), -1, a_out.data(), len, nullptr, &usedDefault);
			return !usedDefault;
		}

		bool CallCharGen(const char* a_function, const std::string& a_ansiName)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm)
			{
				logger::error("presets: no Papyrus VM; CharGen.{} not called", a_function);
				return false;
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
			auto* args = RE::MakeFunctionArguments(RE::BSFixedString(a_ansiName.c_str()));
			const bool ok = vm->DispatchStaticCall("CharGen", a_function, args, result);
			logger::info("presets: CharGen.{}(\"{}\") {}", a_function, a_ansiName,
				ok ? "dispatched" : "REFUSED by the VM (are RaceMenu's scripts linked? see Papyrus.0.log)");
			return ok;
		}

		bool OnMain(std::function<void()> a_fn)
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) { return false; }
			tasks->AddTask(std::move(a_fn));
			return true;
		}
	}

	std::string Folder() { return Utf8(FolderPath().wstring()); }

	std::vector<Preset> List()
	{
		std::vector<Preset> out;
		std::error_code ec;
		const auto folder = FolderPath();
		if (!std::filesystem::is_directory(folder, ec)) { return out; }
		for (const auto& file : std::filesystem::directory_iterator(folder, ec))
		{
			if (ec) { break; }
			if (!file.is_regular_file(ec)) { continue; }
			std::wstring ext = file.path().extension().wstring();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
			if (ext != L".jslot") { continue; }
			Preset p;
			const std::wstring stem = file.path().stem().wstring();
			p.name = Utf8(stem);
			p.loadable = Ansi(stem, p.ansi);
			out.push_back(std::move(p));
		}
		std::sort(out.begin(), out.end(), [](const Preset& a, const Preset& b) { return a.name < b.name; });
		return out;
	}

	bool Exists(const std::string& a_utf8)
	{
		std::error_code ec;
		return std::filesystem::exists(FolderPath() / (Wide(a_utf8) + L".jslot"), ec);
	}

	bool Save(const std::string& a_utf8)
	{
		std::string ansi;
		if (!Ansi(Wide(a_utf8), ansi) || ansi.find_first_of("\\/:*?\"<>|") != std::string::npos)
		{
			logger::warn("presets: \"{}\" cannot be a preset name on this Windows (code page or file-name characters)", a_utf8);
			return false;
		}
		std::error_code ec;
		std::filesystem::create_directories(FolderPath(), ec);
		return OnMain([ansi]() { CallCharGen("SavePreset", ansi); });
	}

	bool Load(const Preset& a_preset)
	{
		if (!a_preset.loadable) { return false; }
		const std::string ansi = a_preset.ansi;
		const bool ok = OnMain([ansi]() { CallCharGen("LoadPreset", ansi); });
		// RaceMenu applies the preset in its scripts; read the menu again so the sliders show it. CONFIRM IN THE TEST RUN that
		// RaceMenu's movie refreshes its values after a script-side load.
		bridge::ReprobeSoon(1500);
		return ok;
	}

	bool Remove(const Preset& a_preset)
	{
		std::error_code ec;
		const auto path = FolderPath() / (Wide(a_preset.name) + L".jslot");
		const bool ok = std::filesystem::remove(path, ec);
		logger::info("presets: delete \"{}\" {}", a_preset.name, ok ? std::string("done") : ("failed: " + ec.message()));
		return ok;
	}
}
