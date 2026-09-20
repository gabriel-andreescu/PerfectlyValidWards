#include <RE/Skyrim.h> // IWYU pragma: keep

#include "FormCache.h"
#include "GameTasks.h"
#include "Mechanics.h"

#include "Settings.h"
#include <RE/A/ActiveEffect.h>
#include <RE/A/Actor.h>
#include <RE/A/ActorValues.h>
#include <RE/B/BSContainer.h>
#include <RE/B/BSPointerHandle.h>
#include <RE/E/Effect.h>
#include <RE/E/EffectArchetypes.h>
#include <RE/E/EffectSetting.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicSystem.h>
#include <RE/M/MagicTarget.h>
#include <RE/N/NiPoint3.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESObjectREFR.h>
#include <SKSE/SKSE.h>
#include <algorithm>

[[nodiscard]] const char* Mechanics::FeatureName(const Feature a_feature) {
    switch (a_feature) {
        case Feature::kPhysical:   return "Physical";
        case Feature::kShout:      return "Shout";
        case Feature::kDisease:    return "Disease";
        case Feature::kCloak:      return "Cloak";
        case Feature::kReflection: return "Reflection";
    }
    return "Unknown";
}

[[nodiscard]] bool Mechanics::IsFeatureDisabledByExclusion(const RE::Actor* a_wardCaster, const Feature a_feature) {
    const auto* settings = Settings::GetSingleton();
    if (!FormCache::GetSingleton()->IsExcludedItemEquipped(a_wardCaster)) {
        return false;
    }

    bool disabled = false;
    switch (a_feature) {
        case Feature::kPhysical:   disabled = settings->excludedItemsDisablePhysicalBlocking; break;
        case Feature::kShout:      disabled = settings->excludedItemsDisableShoutMechanics; break;
        case Feature::kDisease:    disabled = settings->excludedItemsDisableDiseaseBlocking; break;
        case Feature::kCloak:      disabled = settings->excludedItemsDisableCloakBlocking; break;
        case Feature::kReflection: disabled = settings->excludedItemsDisableReflection; break;
    }

    if (disabled) {
        SKSE::log::debug(
            "Exclusions: skipped {} for {} due to excluded ward item",
            FeatureName(a_feature),
            a_wardCaster->GetName()
        );
    }

    return disabled;
}

bool Mechanics::HasRequiredPerks(const RE::Actor* a_actor, const Feature a_feature) {
    if (a_actor == nullptr) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    if (settings->perkGatesPlayerOnly && !a_actor->IsPlayerRef()) {
        return true;
    }

    const auto* cache = FormCache::GetSingleton();
    switch (a_feature) {
        case Feature::kPhysical:   return cache->HasPhysicalPerks(a_actor);
        case Feature::kShout:      return cache->HasShoutPerks(a_actor);
        case Feature::kDisease:    return cache->HasDiseasePerks(a_actor);
        case Feature::kCloak:      return cache->HasCloakPerks(a_actor);
        case Feature::kReflection: return cache->HasReflectionPerks(a_actor);
    }
    return true;
}

bool Mechanics::ShouldApply(RE::Actor* a_defender, const RE::TESObjectREFR* a_attacker, const Feature a_feature) {
    if ((a_defender == nullptr) || (a_attacker == nullptr)) {
        return false;
    }

    if (GetCurrentWardPower(a_defender) <= 0.F) {
        return false;
    }

    if (IsFeatureDisabledByExclusion(a_defender, a_feature)) {
        return false;
    }

    if (!HasRequiredPerks(a_defender, a_feature)) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    const auto angle = a_defender->GetHeadingAngle(a_attacker->GetPosition(), true);
    return angle <= settings->blockingAngle;
}

[[nodiscard]] float Mechanics::GetCurrentWardPower(RE::Actor* a_actor) {
    if (const auto* actorValues = (a_actor != nullptr) ? a_actor->AsActorValueOwner() : nullptr) {
        return actorValues->GetActorValue(RE::ActorValue::kWardPower);
    }
    return 0.F;
}

void Mechanics::DamageWardPower(RE::Actor* a_actor, float a_damage) {
    if ((a_actor == nullptr) || a_damage <= 0.F) {
        return;
    }

    const auto handle = a_actor->CreateRefHandle();
    GameTasks::Add([handle, a_damage] {
        const auto actorPtr = handle.get();
        auto* actor = actorPtr ? actorPtr->As<RE::Actor>() : nullptr;
        auto* actorValues = actor ? actor->AsActorValueOwner() : nullptr;
        if (!actorValues) {
            return;
        }

        const auto before = actorValues->GetActorValue(RE::ActorValue::kWardPower);
        const auto dmg = std::min(a_damage, before);
        if (dmg <= 0.F) {
            return;
        }

        actorValues->DamageActorValue(RE::ActorValue::kWardPower, dmg);

        SKSE::log::debug(
            "Damaging WardPower | actor={} | raw_damage={:.2f} | before={:.2f} | clamped_damage={:.2f}",
            actor->GetName(),
            a_damage,
            before,
            dmg
        );
    });
}

void Mechanics::ApplyStagger(RE::Actor* a_actor, const RE::NiPoint3& a_sourcePosition, float a_magnitude) {
    if (a_actor == nullptr) {
        return;
    }

    const auto handle = a_actor->CreateRefHandle();
    GameTasks::Add([handle, a_sourcePosition, a_magnitude] {
        const auto actorPtr = handle.get();
        auto* actor = actorPtr ? actorPtr->As<RE::Actor>() : nullptr;
        if (!actor) {
            return;
        }

        const auto heading = actor->GetHeadingAngle(a_sourcePosition, false);
        const auto dir = heading >= 0.F ? heading / 360.F : (360.F + heading) / 360.F;

        SKSE::log::debug(
            "Applying stagger | actor={} | magnitude={:.2f} | heading={:.2f} | dir={:.3f}",
            actor->GetName(),
            a_magnitude,
            heading,
            dir
        );

        actor->SetGraphVariableFloat("staggerDirection", dir);
        actor->SetGraphVariableFloat("StaggerMagnitude", a_magnitude);
        actor->NotifyAnimationGraph("staggerStart");
    });
}

[[nodiscard]] bool Mechanics::IsDiseaseSpell(RE::MagicItem* a_spell) {
    if (a_spell == nullptr) {
        return false;
    }

    if (const auto* spellItem = a_spell->As<RE::SpellItem>();
        spellItem != nullptr && spellItem->GetSpellType() == RE::MagicSystem::SpellType::kDisease) {
        return true;
    }

    const auto formID = a_spell->GetFormID();
    return (formID != 0U) && FormCache::GetSingleton()->IsDiseaseSpell(formID);
}

[[nodiscard]] bool Mechanics::IsCloakSpell(const RE::MagicItem* a_spell, const RE::EffectSetting* a_effect) {
    if ((a_spell == nullptr) || (a_effect == nullptr)) {
        return false;
    }

    const bool isHostile = a_effect->IsHostile();
    const bool isCloakArchetype = a_effect->data.archetype == RE::EffectArchetypes::ArchetypeID::kCloak;
    const bool hasMagicCloakKeyword = a_effect->HasKeywordString("MagicCloak");

    if (isHostile && (isCloakArchetype || hasMagicCloakKeyword)) {
        return true;
    }

    const auto formID = a_spell->GetFormID();
    return (formID != 0U) && FormCache::GetSingleton()->IsCloakSpell(formID);
}

[[nodiscard]] bool Mechanics::IsCloakDamageSpell(const RE::MagicItem* a_spell, RE::Actor* a_caster) {
    if ((a_spell == nullptr) || (a_caster == nullptr)) {
        return false;
    }

    const auto spellFormID = a_spell->GetFormID();
    if (spellFormID == 0U) {
        return false;
    }

    auto* magicTarget = a_caster->AsMagicTarget();
    if (magicTarget == nullptr) {
        return false;
    }

    bool found = false;
    RE::MagicTarget::EffectVisitor visitor([spellFormID, &found](RE::ActiveEffect* a_activeEffect) {
        const auto* baseEffect = a_activeEffect ? a_activeEffect->GetBaseObject() : nullptr;
        if (!baseEffect || baseEffect->data.archetype != RE::EffectArchetypes::ArchetypeID::kCloak) {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        const auto* associatedForm = baseEffect->data.associatedForm;
        if (associatedForm && associatedForm->GetFormID() == spellFormID) {
            found = true;
            return RE::BSContainer::ForEachResult::kStop;
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    magicTarget->VisitEffects(visitor);
    return found;
}

[[nodiscard]] bool Mechanics::IsWardSpell(const RE::SpellItem* a_spell) {
    if (a_spell == nullptr) {
        return false;
    }
    const auto* keyword = FormCache::GetSingleton()->GetWardKeyword();
    if (keyword == nullptr) {
        return false;
    }
    return std::ranges::any_of(a_spell->effects, [keyword](const RE::Effect* a_effect) {
        return a_effect && a_effect->baseEffect && a_effect->baseEffect->HasKeyword(keyword);
    });
}
