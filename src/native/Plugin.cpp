#include <RE/Skyrim.h> // IWYU pragma: keep

#include "DevBenchIntegration.h"
#include "EventListener.h"
#include "FormCache.h"
#include "GameTasks.h"
#include "Hooks.h"
#include "Papyrus.h"
#include "Patches.h"
#include "Settings.h"
#include "Tweaks.h"
#include "WardMeter.h"
#include <RE/T/TESDataHandler.h>
#include <SKSE/SKSE.h>

namespace {
constexpr auto kTrampolineSize = 128;

void MessageHandler(SKSE::MessagingInterface::Message* a_message) { // NOLINT(misc-const-correctness)
    if (a_message->type
        == SKSE::MessagingInterface::kPreLoadGame
        || a_message->type
        == SKSE::MessagingInterface::kNewGame) {
        GameTasks::CancelPending();
        FormCache::GetSingleton()->ClearSpellCasters();
    }
    if (a_message->type == SKSE::MessagingInterface::kPostPostLoad) {
        Settings::GetSingleton()->Load();
        DevBenchIntegration::Register();
    }
    if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
        Settings::GetSingleton()->ResolveRuntimeData();
        auto* data = RE::TESDataHandler::GetSingleton();
        if (data == nullptr) {
            SKSE::log::critical("TESDataHandler unavailable at kDataLoaded");
            return;
        }
        FormCache::GetSingleton()->Initialize(*data, *Settings::GetSingleton());
        Patches::PatchCollisionLayers();
        Patches::PatchImpactDataSets();
        Patches::PatchHitGrunts();
        Hooks::Install();
        EventListener::Register();
        WardMeter::InstallHook();
        Tweaks::InstallHooks();
    }
}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_extender) {
    SKSE::Init(
        a_extender,
        {
            .logPattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v",
            .trampoline = true,
            .trampolineSize = kTrampolineSize,
        }
    );
    Papyrus::Register();

    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
    return true;
}
