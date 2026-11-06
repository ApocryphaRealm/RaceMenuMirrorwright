// Likeness - own code, GPL-3.0-or-later. A character editor drawn over RaceMenu through the Apocrypha Menu Framework;
// RaceMenu stays underneath and does the real work. See README.md and 4. plans\racemenu-rebuild\PLAN.md.
#include "PCH.h"

#include "Bridge.h"
#include "DevBenchTool.h"
#include "Editor.h"
#include "Settings.h"

#include "utils/AddressLibraryGuard.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) { return; }
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			strings::Configure("Likeness");
			editor::Register();
			bridge::Install();
			DevBenchTool::Init(true);
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::log::init("Likeness");
	// Address Library pre-check, before SKSE::Init opens it: a missing file gets a message naming it and the plugin loads inert.
	if (!AddressLibraryGuard::Guard("Likeness")) { return true; }
	SKSE::Init(a_skse);

	settings::Init("Likeness.ini");
	settings::ApplyLogLevel();
	SKSE::log::describe_level("Likeness.ini");
	logger::info("Likeness {} loading", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));
	logger::info("Build line: {}", RUNTIME_LINE == 17 ? "Skyrim 1.7.x" : "SE 1.5.97 / AE 1.6.x");

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler))
	{
		logger::error("could not register the SKSE message listener; Likeness stands down");
		return true;
	}
	return true;
}
