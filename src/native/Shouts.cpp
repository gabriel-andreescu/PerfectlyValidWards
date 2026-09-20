#include <RE/Skyrim.h> // IWYU pragma: keep

#include "FormCache.h"
#include "GameTasks.h"
#include "Mechanics.h"

#include "Settings.h"
#include "Shouts.h"
#include <RE/A/Actor.h>
#include <RE/B/BSPointerHandle.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicSystem.h>
#include <RE/N/NiPoint3.h>
#include <RE/S/SpellItem.h>
#include <SKSE/SKSE.h>
#include <chrono>
#include <cstdint>
#include <unordered_map>

using namespace std::chrono_literals;

namespace {
using Clock = std::chrono::steady_clock;
using RecentShoutHits = std::unordered_map<std::uint64_t, Clock::time_point>;

[[nodiscard]] RecentShoutHits& GetRecentShoutHits() {
    static auto* hits = new RecentShoutHits();
    return *hits;
}

[[nodiscard]] bool ShouldProcessShout(RE::Actor* a_defender, const RE::Actor* a_attacker, RE::MagicItem* a_spell) {
    if (!Mechanics::ShouldApply(a_defender, a_attacker, Mechanics::Feature::kShout)) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    if (settings->playerImmuneToShoutMechanics && a_defender->IsPlayerRef()) {
        return false;
    }

    if (a_spell->GetSpellType() != RE::MagicSystem::SpellType::kVoicePower) {
        return false;
    }

    if (!Shouts::IsPassThroughCandidate(a_spell)) {
        SKSE::log::debug(
            "Shout skipped | spell={} <{:08X}> | reason=not offensive",
            a_spell->GetName(),
            a_spell->GetFormID()
        );
        return false;
    }

    return true;
}

[[nodiscard]] bool CanPassThrough(RE::MagicItem* a_spell) {
    if ((a_spell == nullptr) || !Shouts::IsPassThroughCandidate(a_spell)) {
        return false;
    }

    if (a_spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
        SKSE::log::debug(
            "Shout pass-through skipped | spell={} <{:08X}> | reason=concentration",
            a_spell->GetName(),
            a_spell->GetFormID()
        );
        return false;
    }

    return true;
}

[[nodiscard]] bool CalculateAndApplyWardDamage(RE::Actor* a_defender, const float a_currentWardPower) {
    const auto* settings = Settings::GetSingleton();

    bool wardBroken = false;
    if (settings->shoutInstantBreak) {
        wardBroken = a_currentWardPower > 0.F;
        Mechanics::DamageWardPower(a_defender, a_currentWardPower);
    } else {
        if (a_currentWardPower - settings->shoutDamage <= 0.F && a_currentWardPower > 0.F) {
            wardBroken = true;
        }
        Mechanics::DamageWardPower(a_defender, settings->shoutDamage);
    }

    return wardBroken;
}

void ApplyWardBreakStagger(RE::Actor* a_defender, const RE::Actor* a_attacker) {
    const auto* settings = Settings::GetSingleton();
    if (!settings->staggerDefenderOnBreak) {
        return;
    }

    const RE::NiPoint3 attackerPos = (a_attacker != nullptr) ? a_attacker->GetPosition() : a_defender->GetPosition();
    Mechanics::ApplyStagger(a_defender, attackerPos, settings->staggerMagnitudeTowardDefender);
}
}

[[nodiscard]] bool Shouts::IsPassThroughCandidate(RE::MagicItem* a_shoutSpell) {
    const auto* spell = (a_shoutSpell != nullptr) ? a_shoutSpell->As<RE::SpellItem>() : nullptr;
    if (spell == nullptr) {
        return false;
    }
    return FormCache::GetSingleton()->IsOffensiveShoutSpell(spell->GetFormID());
}

void Shouts::ApplyPassThrough(RE::Actor* a_defender, RE::Actor* a_attacker, RE::MagicItem* a_spell) {
    if ((a_defender == nullptr) || (a_spell == nullptr)) {
        return;
    }

    if (Settings::GetSingleton()->emptyActivator == nullptr) {
        SKSE::log::warn("Shouts: pass-through skipped, missing emptyActivator");
        return;
    }

    const auto defenderHandle = a_defender->CreateRefHandle();
    RE::ActorHandle attackerHandle;
    if (a_attacker != nullptr) {
        attackerHandle = a_attacker->CreateRefHandle();
    }

    GameTasks::Add(
        [defenderHandle, attackerHandle, a_spell] {
            const auto defenderPtr = defenderHandle.get();
            auto* defender = defenderPtr ? defenderPtr->As<RE::Actor>() : nullptr;
            if (!defender) {
                return;
            }

            const auto attackerPtr = attackerHandle.get();
            auto* attacker = attackerPtr ? attackerPtr->As<RE::Actor>() : nullptr;

            const auto* settings = Settings::GetSingleton();
            auto* activator = settings->emptyActivator;
            if (!activator) {
                SKSE::log::warn("Shouts: pass-through skipped, missing emptyActivator");
                return;
            }

            const auto casterPtr = FormCache::GetSingleton()->GetOrCreateSpellCaster(defender, activator);
            auto* casterRef = casterPtr.get();
            if (!casterRef) {
                SKSE::log::debug("Shouts: pass-through failed, no caster");
                return;
            }

            casterRef->MoveTo(defender);

            auto defenderPos = defender->GetPosition();
            defenderPos.z += defender->GetHeight();
            casterRef->SetPosition(defenderPos);
            casterRef->Update3DPosition(true);

            if (auto* magicCaster = casterRef->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
                SKSE::log::debug(
                    "Shout pass-through | defender={} | attacker={} | spell={} <{:08X}>",
                    defender->GetName(),
                    attacker ? attacker->GetName() : "none",
                    a_spell->GetName(),
                    a_spell->GetFormID()
                );

                magicCaster->CastSpellImmediate(
                    a_spell,
                    true,
                    defender->AsReference(),
                    1.0F,
                    false,
                    0.0F,
                    attacker ? attacker : defender
                );
            } else {
                SKSE::log::debug("Shouts: pass-through failed, activator has no magic caster");
            }
        },
        60ms
    );
}

void Shouts::ProcessWardHit(RE::Actor* a_defender, RE::Actor* a_attacker, RE::MagicItem* a_spell) {
    if (!ShouldProcessShout(a_defender, a_attacker, a_spell)) {
        return;
    }

    const auto now = Clock::now();
    const auto key = (static_cast<std::uint64_t>(a_defender->GetFormID()) << 32U)
                     | static_cast<std::uint64_t>(a_spell->GetFormID());
    auto& recentShoutHits = GetRecentShoutHits();
    if (const auto found = recentShoutHits.find(key); found != recentShoutHits.end() && (now - found->second) < 150ms) {
        SKSE::log::debug(
            "Shout hit deduped | defender={} | attacker={} | spell={} <{:08X}>",
            a_defender->GetName(),
            (a_attacker != nullptr) ? a_attacker->GetName() : "none",
            a_spell->GetName(),
            a_spell->GetFormID()
        );
        return;
    }

    if (recentShoutHits.size() >= 256) {
        recentShoutHits.clear();
    }
    recentShoutHits.insert_or_assign(key, now);

    float currentWardPower = Mechanics::GetCurrentWardPower(a_defender);
    const auto* settings = Settings::GetSingleton();
    const auto shoutInstantBreak = settings->shoutInstantBreak.load();
    const auto damageToApply = shoutInstantBreak ? currentWardPower : settings->shoutDamage.load();

    SKSE::log::debug(
        "Shout hit | defender={} | attacker={} | spell={} <{:08X}> | ward={:.2f} | damage={:.2f} | instantBreak={}",
        a_defender->GetName(),
        (a_attacker != nullptr) ? a_attacker->GetName() : "none",
        a_spell->GetName(),
        a_spell->GetFormID(),
        currentWardPower,
        damageToApply,
        shoutInstantBreak
    );

    const bool wardBroken = CalculateAndApplyWardDamage(a_defender, currentWardPower);

    const bool shouldPassThrough = wardBroken && settings->shoutPassThrough.load() && CanPassThrough(a_spell);
    if (shouldPassThrough) {
        ApplyPassThrough(a_defender, a_attacker, a_spell);
    } else if (wardBroken) {
        ApplyWardBreakStagger(a_defender, a_attacker);
    }
}
