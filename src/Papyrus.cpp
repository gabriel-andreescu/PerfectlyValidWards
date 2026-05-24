#include "Papyrus.h"

#include "Settings.h"

namespace Papyrus {
namespace {
    void OnConfigClose([[maybe_unused]] RE::TESQuest* a_quest) {
        const auto reload = Settings::GetSingleton()->Reload();
        if (reload.changed) {
            logger::info("Papyrus: MCM settings changed | action=liveReload");
        }
    }

    bool RegisterMCM(RE::BSScript::IVirtualMachine* a_vm) {
        a_vm->RegisterFunction("OnConfigClose", "PerfectlyValidWards_MCM", OnConfigClose);
        logger::info("Papyrus: MCM reload callback registered");
        return true;
    }
}

void Register() {
    const auto* papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus) {
        logger::critical("Papyrus: register skipped | reason=noInterface");
        return;
    }

    papyrus->Register(RegisterMCM);
}
}
