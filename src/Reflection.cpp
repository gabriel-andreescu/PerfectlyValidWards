#include "Reflection.h"
#include "FormCache.h"
#include "Mechanics.h"
#include "Settings.h"

namespace {
[[nodiscard]] bool ShouldReflect(
    RE::Actor* a_defender,
    RE::Actor* a_attacker,
    RE::MagicItem* a_spell,
    const RE::TESMagicWardHitEvent::Status a_status
) {
    if (!a_defender || !a_spell) {
        return false;
    }

    if (a_status == RE::TESMagicWardHitEvent::Status::kFriendlyHit) {
        return false;
    }

    // Only reflect spells that have a concrete hostile attacker. This avoids
    // edge cases like friendly projectiles or trap/environment spells.
    if (!a_attacker) {
        return false;
    }

    if (!a_defender->IsHostileToActor(a_attacker)) {
        return false;
    }

    const auto* a_settings = Settings::GetSingleton();
    if (!a_settings || !a_settings->enableSpellReflection || !a_settings->emptyActivator) {
        return false;
    }

    if (a_status == RE::TESMagicWardHitEvent::Status::kWardBroke && !a_settings->reflectEvenIfWardBroken) {
        return false;
    }

    if (a_settings->restrictReflectionToPlayerTeam) {
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

void ApplyReflection(RE::Actor* a_defender, RE::Actor* a_attacker, RE::MagicItem* a_spell) {
    const auto* a_settings = Settings::GetSingleton();
    if (!a_settings || !a_settings->emptyActivator) {
        return;
    }

    const auto a_casterPtr = FormCache::GetSingleton()->GetOrCreateSpellCaster(a_defender, a_settings->emptyActivator);
    auto* a_casterRef = a_casterPtr.get();
    if (!a_casterRef) {
        logger::debug("Reflection: failed, no caster");
        return;
    }

    const auto a_heading = a_defender->GetAngle().z;
    const float a_forward = a_settings->reflectionForwardOffset;

    a_casterRef->MoveTo(a_defender);
    auto a_casterPos = a_defender->GetPosition();
    a_casterPos.x += std::sin(a_heading) * a_forward;
    a_casterPos.y += std::cos(a_heading) * a_forward;
    a_casterPos.z += a_defender->GetHeight() * 0.75f;
    a_casterRef->SetPosition(a_casterPos);
    a_casterRef->Update3DPosition(true);

    RE::TESObjectREFR* a_targetRef = nullptr;

    if (a_settings->autoAimReflection && a_attacker) {
        a_targetRef = a_attacker->AsReference();
    } else {
        const auto a_placedTarget = a_defender->PlaceObjectAtMe(a_settings->emptyActivator, false);
        if (auto* a_target = a_placedTarget.get()) {
            a_target->SetTemporary();
            a_target->MoveTo(a_defender);
            auto a_targetPos = a_defender->GetPosition();
            a_targetPos.x += std::sin(a_heading) * a_forward * 2.0f;
            a_targetPos.y += std::cos(a_heading) * a_forward * 2.0f;
            a_targetPos.z += a_defender->GetHeight() * 0.75f;
            a_target->SetPosition(a_targetPos);
            a_target->Update3DPosition(true);
            a_targetRef = a_target;
        }
    }

    if (!a_targetRef) {
        logger::debug("Reflection: failed, no target");
        return;
    }

    auto* a_magicCaster = a_casterRef->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
    if (!a_magicCaster) {
        logger::debug("Reflection: failed, activator has no magic caster");
        return;
    }

    RE::Actor* a_blameActor = a_settings->reflectionBlameAttacker && a_attacker ? a_attacker : a_defender;

    a_magicCaster->CastSpellImmediate(a_spell, false, a_targetRef, 1.0f, false, 0.0f, a_blameActor);

    logger::debug(
        "Reflected spell | defender={} | attacker={} | spell={} <{:08X}>",
        a_defender->GetName(),
        a_attacker ? a_attacker->GetName() : "none",
        a_spell->GetName(),
        a_spell->GetFormID()
    );

    stl::play_sound(a_defender, "MAGWardTestDeflect");
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

    auto a_defenderHandle = a_defender->CreateRefHandle();
    RE::ActorHandle a_attackerHandle;
    if (a_attacker) {
        a_attackerHandle = a_attacker->CreateRefHandle();
    }

    stl::add_thread_task(
        [a_defenderHandle, a_attackerHandle, a_spell] {
            const auto defenderPtr = a_defenderHandle.get();
            auto* defender = defenderPtr ? defenderPtr->As<RE::Actor>() : nullptr;
            if (!defender) {
                return;
            }

            const auto attackerPtr = a_attackerHandle.get();
            auto* attacker = attackerPtr ? attackerPtr->As<RE::Actor>() : nullptr;

            ApplyReflection(defender, attacker, a_spell);
        },
        60ms
    );
}
