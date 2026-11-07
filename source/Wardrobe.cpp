#include "PCH.h"

#include "Wardrobe.h"

#include "Bridge.h"
#include "utils/Logger.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <format>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

namespace wardrobe
{
	namespace
	{
		struct Worn
		{
			RE::FormID form = 0;              // kept by ID; the object is looked up again on dressing
			RE::ExtraDataList* extra = nullptr;
			std::string name;
		};

		std::mutex g_lock;
		std::vector<Worn> g_removed;          // guarded by g_lock
		std::atomic<bool> g_undressed{ false };

		void OnMain(std::function<void()> a_fn)
		{
			if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(std::move(a_fn)); }
			else { logger::error("wardrobe: no task interface"); }
		}

		// TNG is never touched (the owner's rule; run 18 took off "TNG GenitalCover"). A file from The New Gentleman or its
		// patches: its name holds "TheNewGentleman" or starts with "TNG".
		bool IsTngFile(const RE::TESFile* a_file)
		{
			if (!a_file) { return false; }
			std::string name(a_file->GetFilename());
			for (char& c : name) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return name.find("thenewgentleman") != std::string::npos || name.starts_with("tng");
		}

		// Why an armour must stay on, or nullptr when it may come off.
		// Only body clothing / armour on slots 30-61, never slot 52 (TNG's genital slot by default), never anything of TNG's
		// (its own plugin).
		const char* KeepReason(const RE::TESObjectARMO* a_armor)
		{
			if (!a_armor) { return "not an armour"; }
			if (IsTngFile(a_armor->GetFile(0))) { return "from The New Gentleman's plugins"; }
			// No keyword rule (run 19: TNG adds its keywords to ordinary clothing, so it kept nearly every body armour on).
			const auto mask = static_cast<std::uint32_t>(a_armor->bipedModelData.bipedObjectSlots.underlying());   // both CommonLib lines
			if (mask == 0) { return "no body slot"; }
			if (mask & (1u << (52 - 30))) { return "uses slot 52 (TNG's genital slot)"; }
			return nullptr;   // slots 30-61 are the 32 bits of the mask, so any other slot is a body slot
		}

		// Whether a_list still belongs to the inventory entry of a_obj (it is passed back to the game only then).
		bool StillThere(RE::PlayerCharacter* a_player, RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
		{
			if (!a_list) { return false; }
			auto inv = a_player->GetInventory([a_obj](RE::TESBoundObject& a_o) { return &a_o == a_obj; });
			for (auto& [obj, data] : inv)
			{
				const auto& entry = data.second;
				if (!entry || !entry->extraLists) { continue; }
				for (auto* xl : *entry->extraLists)
				{
					if (xl == a_list) { return true; }
				}
			}
			return false;
		}
	}

	void Undress()
	{
		OnMain([]() {
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* equip = RE::ActorEquipManager::GetSingleton();
			if (!player || !equip)
			{
				logger::warn("undress: no player or equip manager");
				return;
			}
			if (g_undressed)
			{
				logger::debug("undress: already undressed");
				return;
			}
			std::vector<Worn> removed;
			auto inv = player->GetInventory([](RE::TESBoundObject& a_o) { return a_o.IsArmor(); });
			for (auto& [obj, data] : inv)
			{
				const auto& entry = data.second;
				if (!obj || !entry || !entry->IsWorn() || !entry->extraLists) { continue; }
				for (auto* xl : *entry->extraLists)
				{
					if (!xl || !(xl->HasType(RE::ExtraDataType::kWorn) || xl->HasType(RE::ExtraDataType::kWornLeft))) { continue; }
					if (const char* why = KeepReason(obj->As<RE::TESObjectARMO>()))
					{
						logger::info("undress: kept {} ({:08X}): {}", obj->GetName() ? obj->GetName() : "", obj->GetFormID(), why);
						continue;
					}
					removed.push_back({ obj->GetFormID(), xl, obj->GetName() ? obj->GetName() : "" });
				}
			}
			for (const auto& w : removed)
			{
				auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(w.form);
				if (!obj) { continue; }
				equip->UnequipObject(player, obj, w.extra, 1, nullptr, false, false, false, true);
				logger::info("undress: took off {} ({:08X})", w.name, w.form);
			}
			{
				std::scoped_lock guard(g_lock);
				g_removed = std::move(removed);
				g_undressed = true;
				logger::info("undress: {} item(s) off; they go back on with Dress, Done, closing the menu or a load", g_removed.size());
			}
			// Run 18: the RaceSex Menu does not rebuild the player's model after a quiet unequip - ask for it.
			player->Update3DModel();
			logger::debug("undress: 3D model update requested");
		});
	}

	void Dress(const char* a_why)
	{
		const std::string why = a_why ? a_why : "?";
		OnMain([why]() {
			std::vector<Worn> list;
			{
				std::scoped_lock guard(g_lock);
				if (!g_undressed) { return; }
				list.swap(g_removed);
				g_undressed = false;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* equip = RE::ActorEquipManager::GetSingleton();
			if (!player || !equip)
			{
				logger::warn("dress ({}): no player or equip manager; {} item(s) not put back", why, list.size());
				return;
			}
			for (const auto& w : list)
			{
				auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(w.form);
				if (!obj)
				{
					logger::warn("dress: {:08X} is gone", w.form);
					continue;
				}
				RE::ExtraDataList* xl = StillThere(player, obj, w.extra) ? w.extra : nullptr;
				equip->EquipObject(player, obj, xl, 1, nullptr, false, false, false, true);
				logger::info("dress ({}): put on {} ({:08X}){}", why, w.name, w.form, xl ? "" : " - its own instance was gone, another copy used");
			}
			player->Update3DModel();
		});
	}

	bool IsUndressed() { return g_undressed; }

	std::string StateJson()
	{
		std::scoped_lock guard(g_lock);
		std::string items;
		for (const auto& w : g_removed) { items += std::format("{}\"{}\"", items.empty() ? "" : ",", bridge::Escape(w.name)); }
		return std::format(R"({{"undressed":{},"removed":[{}]}})", g_undressed.load(), items);
	}

	std::string PoseCheckJson(const std::string& a_event)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return R"({"ok":false,"error":"no task interface"})"; }
		auto promise = std::make_shared<std::promise<std::string>>();
		auto future = promise->get_future();
		tasks->AddTask([promise, a_event]() {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player)
			{
				promise->set_value(R"({"ok":false,"error":"no player"})");
				return;
			}
			bool inGraph = false;
			player->GetGraphVariableBool("bAnimationDriven", inGraph);
			const bool accepted = player->NotifyAnimationGraph(RE::BSFixedString(a_event.c_str()));
			logger::info("pose check: NotifyAnimationGraph(\"{}\") -> {} (3D loaded {})", a_event, accepted, player->Is3DLoaded());
			promise->set_value(std::format(R"({{"ok":true,"event":"{}","accepted":{},"loaded3D":{}}})", bridge::Escape(a_event), accepted, player->Is3DLoaded()));
		});
		if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) { return R"({"ok":false,"error":"the main thread did not answer in 5 seconds"})"; }
		return future.get();
	}
}
