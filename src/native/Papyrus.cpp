#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Papyrus.h"
#include "Patches.h"
#include <RE/T/TESQuest.h>
#include <SKSE/SKSE.h>

#include "Settings.h"

namespace Papyrus {
namespace {
    // Papyrus passes the script owner as a mutable game object pointer.
    // NOLINTNEXTLINE(misc-const-correctness)
    void OnConfigClose([[maybe_unused]] RE::TESQuest* a_quest) {
        const auto reload = Settings::GetSingleton()->Reload();
        if (reload.changed) {
            SKSE::GetTaskInterface()->AddTask([] {
                Patches::PatchCollisionLayers();
                Patches::PatchImpactDataSets();
            });
            SKSE::log::info("Papyrus: MCM settings changed | action=liveReload");
        }
    }

    bool RegisterMCM(RE::BSScript::IVirtualMachine* a_vm) {
        a_vm->RegisterFunction("OnConfigClose", "PerfectlyValidWards_MCM", OnConfigClose);
        SKSE::log::info("Papyrus: MCM reload callback registered");
        return true;
    }
}

void Register() {
    const auto* papyrus = SKSE::GetPapyrusInterface();
    if (papyrus == nullptr) {
        SKSE::log::critical("Papyrus: register skipped | reason=noInterface");
        return;
    }

    papyrus->Register(RegisterMCM);
}
}
