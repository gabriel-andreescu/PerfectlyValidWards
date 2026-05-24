#include "Tweaks.h"
#include "FormCache.h"
#include "Mechanics.h"
#include "Settings.h"

#include <filesystem>
#include <fstream>

namespace Tweaks {
namespace {
    // ScrambledBugs compatibility: when accumulatingMagnitude is enabled,
    // magnitude and maximumMagnitude members are swapped
    bool g_scrambledBugsSwapActive = false;

    [[nodiscard]] float GetTrueMaximum(const RE::AccumulatingValueModifierEffect* a_effect) {
        return g_scrambledBugsSwapActive ? a_effect->magnitude : a_effect->maximumMagnitude;
    }

    void SetTrueMaximum(RE::AccumulatingValueModifierEffect* a_effect, const float a_value) {
        if (g_scrambledBugsSwapActive) {
            a_effect->magnitude = a_value;
        } else {
            a_effect->maximumMagnitude = a_value;
        }
    }

    [[nodiscard]] bool ShouldModify(RE::AccumulatingValueModifierEffect* a_effect) {
        if (!a_effect) {
            return false;
        }

        const auto* settings = Settings::GetSingleton();
        if (settings->restrictTweaksToPlayerTeam.load()) {
            const auto* target = a_effect->GetTargetActor();
            if (!target) {
                return false;
            }
            if (!target->IsPlayerRef() && !target->IsPlayerTeammate()) {
                return false;
            }
        }

        const auto* baseEffect = a_effect->GetBaseObject();
        if (!baseEffect) {
            return false;
        }

        return baseEffect->HasKeyword(FormCache::GetSingleton()->GetWardKeyword());
    }

    void DetectScrambledBugs() {
        const auto* moduleHandle = GetModuleHandleA("ScrambledBugs.dll");
        if (!moduleHandle) {
            logger::info("Tweaks: ScrambledBugs not detected");
            return;
        }

        const std::filesystem::path configPath = "Data/SKSE/Plugins/ScrambledBugs.json";
        if (!std::filesystem::exists(configPath)) {
            logger::warn("Tweaks: ScrambledBugs.dll loaded but config not found");
            return;
        }

        std::ifstream file(configPath);
        if (!file.is_open()) {
            logger::warn("Tweaks: Failed to open ScrambledBugs.json");
            return;
        }

        try {
            const std::string content((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

            // Simple JSON parse for patches.accumulatingMagnitude
            // Look for "accumulatingMagnitude" followed by : and true/false
            const auto pos = content.find("\"accumulatingMagnitude\"");
            if (pos != std::string::npos) {
                const auto colonPos = content.find(':', pos);
                if (colonPos != std::string::npos) {
                    const auto valueStart = content.find_first_not_of(" \t\n\r", colonPos + 1);
                    if (valueStart != std::string::npos) {
                        g_scrambledBugsSwapActive = content.substr(valueStart, 4) == "true";
                    }
                }
            }
        } catch (...) {
            logger::warn("Tweaks: Failed to parse ScrambledBugs.json");
            return;
        }

        logger::info("Tweaks: ScrambledBugs detected | accumulatingMagnitude={}", g_scrambledBugsSwapActive);
    }
}

struct AccumEffect_Start {
    static void thunk(RE::AccumulatingValueModifierEffect* a_this) {
        func(a_this);

        if (!ShouldModify(a_this)) {
            return;
        }

        const auto* settings = Settings::GetSingleton();
        const float current = GetTrueMaximum(a_this);
        SetTrueMaximum(a_this, current * settings->wardMagnitudeMultiplier.load());
    }

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx = 0x14;
};

struct AccumEffect_Update {
    static void thunk(RE::AccumulatingValueModifierEffect* a_this, float a_delta) {
        if (!ShouldModify(a_this)) {
            func(a_this, a_delta);
            return;
        }

        const auto* settings = Settings::GetSingleton();
        const float trueMax = GetTrueMaximum(a_this);

        if (settings->instantWardCharge.load()) {
            a_this->accumulatedMagnitude = trueMax;
            func(a_this, a_delta);
            return;
        }

        float delta = a_delta;
        const auto chargeRate = settings->wardChargeRateMultiplier.load();
        if (chargeRate != 1.0f && a_this->holdTimer <= 0.0f) {
            delta *= chargeRate;
        }
        func(a_this, delta);
    }

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx = 0x4;
};

struct SpellItem_AdjustCost {
    static void thunk(const RE::SpellItem* a_this, float& a_cost, RE::Actor* a_actor) {
        func(a_this, a_cost, a_actor);

        if (!Mechanics::IsWardSpell(a_this)) {
            return;
        }

        const auto* settings = Settings::GetSingleton();
        if (settings->restrictTweaksToPlayerTeam.load() && a_actor) {
            if (!a_actor->IsPlayerRef() && !a_actor->IsPlayerTeammate()) {
                return;
            }
        }

        a_cost *= settings->wardCostMultiplier.load();
    }

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx = 0x63;
};

void InstallHooks() {
    const auto* settings = Settings::GetSingleton();

    DetectScrambledBugs();

#ifndef __clang_analyzer__
    stl::write_vfunc<RE::AccumulatingValueModifierEffect, AccumEffect_Start>();
    stl::write_vfunc<RE::AccumulatingValueModifierEffect, AccumEffect_Update>();
    stl::write_vfunc<RE::SpellItem, SpellItem_AdjustCost>();
#endif

    logger::info(
        "Tweaks: hooks installed | magnitude={:.2f}x | instantCharge={} | chargeRate={:.2f}x | cost={:.2f}x | "
        "playerTeamOnly={}",
        settings->wardMagnitudeMultiplier.load(),
        settings->instantWardCharge.load(),
        settings->wardChargeRateMultiplier.load(),
        settings->wardCostMultiplier.load(),
        settings->restrictTweaksToPlayerTeam.load()
    );
}
}
