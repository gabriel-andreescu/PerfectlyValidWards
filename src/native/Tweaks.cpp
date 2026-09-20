#include <RE/Skyrim.h> // IWYU pragma: keep

#include "FormCache.h"
#include "Mechanics.h"
#include "Settings.h"
#include "SkyrimUtil.h"
#include "Tweaks.h"
#include <RE/A/AccumulatingValueModifierEffect.h>
#include <RE/A/Actor.h>
#include <RE/A/ActorMagicCaster.h>
#include <RE/A/ActorValues.h>
#include <RE/B/BGSEntryPoint.h>
#include <RE/M/MagicCaster.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicTarget.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectREFR.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <libloaderapi.h>
#include <string>
#include <string_view>

namespace Tweaks {
namespace {
    [[nodiscard]] bool ScrambledBugsSwapsMagnitude();

    [[nodiscard]] bool ShouldCastInstantly(const RE::ActorMagicCaster& a_caster) {
        const auto* settings = Settings::GetSingleton();
        if (!settings->instantWardCast) {
            return false;
        }
        const auto* spell = a_caster.currentSpell;
        if (spell == nullptr || !Mechanics::IsWardSpell(spell->As<RE::SpellItem>())) {
            return false;
        }
        const auto* actor = a_caster.actor;
        return !settings->restrictTweaksToPlayerTeam
               || (actor != nullptr && (actor->IsPlayerRef() || actor->IsPlayerTeammate()));
    }

    [[nodiscard]] float GetTrueMaximum(const RE::AccumulatingValueModifierEffect* a_effect) {
        return ScrambledBugsSwapsMagnitude() ? a_effect->magnitude : a_effect->maximumMagnitude;
    }

    void SetTrueMaximum(RE::AccumulatingValueModifierEffect* a_effect, const float a_value) {
        if (ScrambledBugsSwapsMagnitude()) {
            a_effect->magnitude = a_value;
        } else {
            a_effect->maximumMagnitude = a_value;
        }
    }

    void ChargeWardInstantly(RE::AccumulatingValueModifierEffect* a_effect) {
        constexpr float kInstantChargeDelta = 1.0e6F;

        if ((a_effect == nullptr) || a_effect->actorValue != RE::ActorValue::kWardPower || a_effect->holdTimer > 0.0F) {
            return;
        }

        if (a_effect->ShouldModifyOnUpdate()) {
            a_effect->ModifyOnUpdate(kInstantChargeDelta);
        }
    }

    [[nodiscard]] bool ShouldModify(RE::AccumulatingValueModifierEffect* a_effect) {
        if (a_effect == nullptr) {
            return false;
        }

        const auto* settings = Settings::GetSingleton();
        if (settings->restrictTweaksToPlayerTeam.load()) {
            // MagicTarget is a secondary Actor base, so an unadjusted cast is invalid.
            const auto* reference = a_effect->target != nullptr ? a_effect->target->GetTargetStatsObject() : nullptr;
            const auto* target = reference != nullptr ? reference->As<RE::Actor>() : nullptr;
            if (target == nullptr) {
                return false;
            }
            if (!target->IsPlayerRef() && !target->IsPlayerTeammate()) {
                return false;
            }
        }

        const auto* baseEffect = a_effect->GetBaseObject();
        if (baseEffect == nullptr) {
            return false;
        }

        return baseEffect->HasKeyword(FormCache::GetSingleton()->GetWardKeyword());
    }

    [[nodiscard]] bool ReadAccumulatingMagnitude(const std::string_view a_content) {
        const auto key = a_content.find("\"accumulatingMagnitude\"");
        if (key == std::string_view::npos) {
            return false;
        }
        const auto colon = a_content.find(':', key);
        if (colon == std::string_view::npos) {
            return false;
        }
        const auto value = a_content.find_first_not_of(" \t\n\r", colon + 1);
        return value != std::string_view::npos && a_content.substr(value, 4) == "true";
    }

    [[nodiscard]] bool DetectScrambledBugs() {
        const auto* moduleHandle = GetModuleHandleA("ScrambledBugs.dll");
        if (moduleHandle == nullptr) {
            SKSE::log::info("Tweaks: ScrambledBugs not detected");
            return false;
        }

        // Nexus v21 (March 2023) has no GetSettings export. The Version22 interface
        // was added to GitHub in September 2024, after that release.
        // Reading the configuration preserves compatibility with v21.
        const std::filesystem::path configPath = "Data/SKSE/Plugins/ScrambledBugs.json";
        if (!std::filesystem::exists(configPath)) {
            SKSE::log::warn("Tweaks: ScrambledBugs.dll loaded but config not found");
            return false;
        }

        std::ifstream file(configPath);
        if (!file.is_open()) {
            SKSE::log::warn("Tweaks: Failed to open ScrambledBugs.json");
            return false;
        }

        try {
            const std::string content((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());

            const auto enabled = ReadAccumulatingMagnitude(content);
            SKSE::log::info("Tweaks: ScrambledBugs detected | accumulatingMagnitude={}", enabled);
            return enabled;
        } catch (...) {
            SKSE::log::warn("Tweaks: Failed to parse ScrambledBugs.json");
            return false;
        }
    }
    [[nodiscard]] bool ScrambledBugsSwapsMagnitude() {
        // The accumulating-magnitude patch swaps magnitude and maximumMagnitude.
        static const bool enabled = DetectScrambledBugs();
        return enabled;
    }

    struct AccumEffectStart {
        static void thunk(RE::AccumulatingValueModifierEffect* a_this) {
            if (ShouldModify(a_this)) {
                // Start caches the actor's maximum ward power from its active effects.
                const auto* settings = Settings::GetSingleton();
                const float current = GetTrueMaximum(a_this);
                SetTrueMaximum(a_this, current * settings->wardMagnitudeMultiplier.load());
            }
            func(a_this);
        }

        static inline REL::Relocation<decltype(thunk)> func;
        static constexpr std::size_t kIdx = 0x14;
    };

    struct AccumEffectUpdate {
        static void thunk(RE::AccumulatingValueModifierEffect* a_this, float a_delta) {
            if (!ShouldModify(a_this)) {
                func(a_this, a_delta);
                return;
            }

            const auto* settings = Settings::GetSingleton();

            if (settings->instantWardCharge.load()) {
                ChargeWardInstantly(a_this);
                func(a_this, a_delta);
                return;
            }

            float delta = a_delta;
            const auto chargeRate = settings->wardChargeRateMultiplier.load();
            if (chargeRate != 1.0F && a_this->holdTimer <= 0.0F) {
                delta *= chargeRate;
            }
            func(a_this, delta);
        }

        static inline REL::Relocation<decltype(thunk)> func;
        static constexpr std::size_t kIdx = 0x4;
    };

    struct MagicItemModifyCost {
        static void thunk(
            RE::BGSEntryPoint::ENTRY_POINT a_entry,
            RE::Actor* a_actor,
            RE::MagicItem* a_item,
            float* a_cost
        ) {
            func(a_entry, a_actor, a_item, a_cost);

            if (!Mechanics::IsWardSpell(a_item->As<RE::SpellItem>())) {
                return;
            }

            const auto* settings = Settings::GetSingleton();
            if (settings->restrictTweaksToPlayerTeam.load()
                && (a_actor != nullptr)
                && !a_actor->IsPlayerRef()
                && !a_actor->IsPlayerTeammate()) {
                return;
            }

            *a_cost *= settings->wardCostMultiplier.load();
        }

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct ActorMagicCasterSetCastingTimerForCharge {
        static void thunk(RE::ActorMagicCaster* a_caster) {
            func(a_caster);
            if (ShouldCastInstantly(*a_caster)) {
                a_caster->castingTimer = 0.0F;
            }
        }

        static inline REL::Relocation<decltype(thunk)> func;
        static constexpr std::size_t kIdx = 0x14;
    };

    struct ActorMagicCasterStartCastImpl {
        static void thunk(RE::ActorMagicCaster* a_caster) {
            func(a_caster);
            if (a_caster->state != RE::MagicCaster::State::kUnk04 || !ShouldCastInstantly(*a_caster)) {
                return;
            }

            // Use the spell-fire handler's release path before the animation reaches its release event.
            using ReleaseSpell = bool(RE::MagicCaster*, RE::TESBoundObject*, bool);
            static const REL::Relocation<ReleaseSpell> releaseSpell {REL::VariantID(33629, 34407, 0x550B20)};
            releaseSpell(a_caster, nullptr, true);
        }

        static inline REL::Relocation<decltype(thunk)> func;
        static constexpr std::size_t kIdx = 0x6;
    };
}

void InstallHooks() {
    const auto* settings = Settings::GetSingleton();

    static_cast<void>(ScrambledBugsSwapsMagnitude());

    // These vtable addresses are resolved from the running Skyrim executable.
    // NOLINTBEGIN(clang-analyzer-core.FixedAddressDereference)
    stl::WriteVFunc<RE::AccumulatingValueModifierEffect, AccumEffectStart>();
    stl::WriteVFunc<RE::AccumulatingValueModifierEffect, AccumEffectUpdate>();
    stl::WriteVFunc<RE::ActorMagicCaster, ActorMagicCasterSetCastingTimerForCharge>();
    stl::WriteVFunc<RE::ActorMagicCaster, ActorMagicCasterStartCastImpl>();
    // NOLINTEND(clang-analyzer-core.FixedAddressDereference)

    // Both calculated and overridden costs pass through this perk adjustment.
    stl::WriteThunkCall<MagicItemModifyCost>(REL::Relocation {RELOCATION_ID(11213, 11321), REL::Relocate(0xDB, 0xD8)});

    SKSE::log::info(
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
