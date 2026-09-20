#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Events.h"
#include "FormCache.h"
#include "GameTasks.h"
#include "Mechanics.h"
#include "Reflection.h"
#include "Settings.h"
#include "SkyrimUtil.h"
#include <RE/A/ActiveEffect.h>
#include <RE/A/Actor.h>
#include <RE/B/BSPointerHandle.h>
#include <RE/M/MagicCaster.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicSystem.h>
#include <RE/M/MagicTarget.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiPoint3.h>
#include <RE/R/ReferenceEffect.h>
#include <RE/S/SpellItem.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <initializer_list>

namespace {
[[nodiscard]] bool ShouldReflect(
    RE::Actor* a_defender,
    RE::Actor* a_attacker,
    RE::MagicItem* a_spell,
    const RE::TESMagicWardHitEvent::Status a_status
) {
    if ((a_defender == nullptr) || (a_spell == nullptr)) {
        return false;
    }

    if (a_status == RE::TESMagicWardHitEvent::Status::kFriendlyHit) {
        return false;
    }

    // Only reflect spells that have a concrete hostile attacker. This avoids
    // edge cases like friendly projectiles or trap/environment spells.
    if (a_attacker == nullptr) {
        return false;
    }

    if (!a_defender->IsHostileToActor(a_attacker)) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    if (!settings->enableSpellReflection || (settings->emptyActivator == nullptr)) {
        return false;
    }

    if (a_status == RE::TESMagicWardHitEvent::Status::kWardBroke && !settings->reflectEvenIfWardBroken) {
        return false;
    }

    if (settings->restrictReflectionToPlayerTeam) {
        if (!a_defender->IsPlayerRef() && !a_defender->IsPlayerTeammate()) {
            return false;
        }
    }

    if (Mechanics::IsFeatureDisabledByExclusion(a_defender, Mechanics::Feature::kReflection)) {
        return false;
    }

    if (!Mechanics::HasRequiredPerks(a_defender, Mechanics::Feature::kReflection)) {
        return false;
    }

    return FormCache::GetSingleton()->IsReflectableSpell(a_spell);
}

[[nodiscard]] float ReflectionForwardOffset(RE::Actor* a_defender, const RE::NiPoint3& a_direction) {
    const auto position = a_defender->GetPosition();
    float forward = Settings::GetSingleton()->reflectionForwardOffset;
    const auto includeBound = [&](const RE::NiAVObject* a_node) {
        if (a_node != nullptr && a_node->worldBound.radius > 0.0F) {
            const auto offset = a_node->worldBound.center - position;
            const float edge = offset.Dot(a_direction) + a_node->worldBound.radius;
            forward = std::max(forward, edge);
        }
    };

    // An origin measured only from the actor can lie inside the animated ward.
    for (const auto hand : {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand}) {
        auto* caster = a_defender->GetMagicCaster(hand);
        if (caster == nullptr || caster->state != RE::MagicCaster::State::kCasting) {
            continue;
        }
        const auto* ward = caster->currentSpell != nullptr ? caster->currentSpell->As<RE::SpellItem>() : nullptr;
        if (!Mechanics::IsWardSpell(ward)) {
            continue;
        }
        includeBound(caster->GetMagicNode());
    }

    // Shield wards attach their geometry to the effect instead of a hand caster.
    RE::MagicTarget::EffectVisitor visitor([&](RE::ActiveEffect* a_effect) {
        const auto* base = a_effect != nullptr ? a_effect->GetBaseObject() : nullptr;
        const bool isWard = base != nullptr && base->data.primaryAV == RE::ActorValue::kWardPower;
        if (!isWard || a_effect->hitEffects == nullptr) {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        for (const auto* hitEffect : *a_effect->hitEffects) {
            if (hitEffect != nullptr) {
                includeBound(hitEffect->Get3D());
            }
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    a_defender->AsMagicTarget()->VisitEffects(visitor);

    return forward;
}

void ApplyReflection(RE::Actor* a_defender, RE::Actor* a_attacker, RE::MagicItem* a_spell) {
    const auto* settings = Settings::GetSingleton();
    if (settings->emptyActivator == nullptr) {
        return;
    }

    const auto casterPtr = FormCache::GetSingleton()->GetOrCreateSpellCaster(a_defender, settings->emptyActivator);
    auto* casterRef = casterPtr.get();
    if (casterRef == nullptr) {
        SKSE::log::debug("Reflection: failed, no caster");
        return;
    }

    const auto heading = a_defender->GetAngle().z;
    const RE::NiPoint3 direction {std::sin(heading), std::cos(heading), 0.0F};
    const float forward = ReflectionForwardOffset(a_defender, direction);

    casterRef->MoveTo(a_defender);
    auto casterPos = a_defender->GetPosition() + direction * forward;
    casterPos.z += a_defender->GetHeight() * 0.75F;
    casterRef->SetPosition(casterPos);
    casterRef->Update3DPosition(true);

    RE::TESObjectREFR* targetRef = nullptr;
    RE::NiPointer<RE::TESObjectREFR> placedTarget;

    if (settings->autoAimReflection && (a_attacker != nullptr)) {
        targetRef = a_attacker->AsReference();
    } else {
        placedTarget = a_defender->PlaceObjectAtMe(settings->emptyActivator, false);
        if (auto* target = placedTarget.get()) {
            target->SetTemporary();
            target->MoveTo(a_defender);
            const auto targetPos = casterPos + direction * forward;
            target->SetPosition(targetPos);
            target->Update3DPosition(true);
            targetRef = target;
        }
    }

    if (targetRef == nullptr) {
        SKSE::log::debug("Reflection: failed, no target");
        return;
    }

    auto* magicCaster = casterRef->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
    if (magicCaster == nullptr) {
        SKSE::log::debug("Reflection: failed, activator has no magic caster");
        return;
    }

    RE::Actor* blameActor = settings->reflectionBlameAttacker && (a_attacker != nullptr) ? a_attacker : a_defender;

    magicCaster->CastSpellImmediate(a_spell, false, targetRef, 1.0F, false, 0.0F, blameActor);

    SKSE::log::debug(
        "Reflected spell | defender={} | attacker={} | spell={} <{:08X}>",
        a_defender->GetName(),
        (a_attacker != nullptr) ? a_attacker->GetName() : "none",
        a_spell->GetName(),
        a_spell->GetFormID()
    );

    stl::PlaySound(a_defender, "MAGWardTestDeflect");
}
}

void Reflection::ProcessWardHit(
    RE::Actor* a_defender,
    RE::Actor* a_attacker,
    RE::MagicItem* a_spell,
    const RE::TESMagicWardHitEvent::Status a_status
) {
    if (!ShouldReflect(a_defender, a_attacker, a_spell, a_status)) {
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

            ApplyReflection(defender, attacker, a_spell);
        },
        60ms
    );
}
