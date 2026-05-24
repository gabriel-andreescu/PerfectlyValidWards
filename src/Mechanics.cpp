#include "Mechanics.h"
#include "FormCache.h"
#include "Settings.h"

namespace {
class RecentBlocks {
public:
    void flag(RE::Actor* a_actor) {
        if (!a_actor) {
            return;
        }
        std::unique_lock lk(mx_);
        set_.insert(a_actor->GetHandle());
    }

    bool consume(RE::Actor* a_actor) {
        if (!a_actor) {
            return false;
        }
        std::unique_lock lk(mx_);
        return set_.erase(a_actor->GetHandle()) > 0;
    }

private:
    std::unordered_set<RE::ActorHandle, HandleHash> set_;
    mutable std::shared_mutex mx_;
};

[[nodiscard]] RecentBlocks& GetRecentBlocks() {
    static auto* blocks = new RecentBlocks();
    return *blocks;
}
}

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
    if (!settings || !FormCache::GetSingleton()->IsExcludedItemEquipped(a_wardCaster)) {
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
        logger::debug(
            "Exclusions: skipped {} for {} due to excluded ward item",
            FeatureName(a_feature),
            a_wardCaster->GetName()
        );
    }

    return disabled;
}

bool Mechanics::HasRequiredPerks(const RE::Actor* a_actor, const Feature a_feature) {
    if (!a_actor) {
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
    if (!a_defender || !a_attacker) {
        return false;
    }

    if (GetCurrentWardPower(a_defender) <= 0.f) {
        return false;
    }

    if (IsFeatureDisabledByExclusion(a_defender, a_feature)) {
        return false;
    }

    if (!HasRequiredPerks(a_defender, a_feature)) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    if (!settings) {
        return false;
    }

    const auto angle = a_defender->GetHeadingAngle(a_attacker->GetPosition(), true);
    return angle <= settings->blockingAngle;
}

[[nodiscard]] float Mechanics::GetCurrentWardPower(RE::Actor* a_actor) {
    if (const auto* av = a_actor ? a_actor->AsActorValueOwner() : nullptr) {
        return av->GetActorValue(RE::ActorValue::kWardPower);
    }
    return 0.f;
}

void Mechanics::DamageWardPower(RE::Actor* a_actor, float a_damage) {
    if (!a_actor || a_damage <= 0.f) {
        return;
    }

    auto handle = a_actor->CreateRefHandle();
    stl::add_thread_task(
        [handle, a_damage] {
            const auto actorPtr = handle.get();
            auto* actor = actorPtr ? actorPtr->As<RE::Actor>() : nullptr;
            auto* av = actor ? actor->AsActorValueOwner() : nullptr;
            if (!av) {
                return;
            }

            const auto before = av->GetActorValue(RE::ActorValue::kWardPower);
            const auto dmg = std::min(a_damage, before);
            if (dmg <= 0.f) {
                return;
            }

            av->DamageActorValue(RE::ActorValue::kWardPower, dmg);

            logger::debug(
                "Damaging WardPower | actor={} | raw_damage={:.2f} | before={:.2f} | clamped_damage={:.2f}",
                actor->GetName(),
                a_damage,
                before,
                dmg
            );

            auto followupHandle = actor->CreateRefHandle();
            stl::add_thread_task(
                [followupHandle, before, dmg] {
                    const auto followupPtr = followupHandle.get();
                    auto* followupActor = followupPtr ? followupPtr->As<RE::Actor>() : nullptr;
                    const auto* followupAV = followupActor ? followupActor->AsActorValueOwner() : nullptr;
                    if (!followupAV) {
                        return;
                    }

                    const auto settled = followupAV->GetActorValue(RE::ActorValue::kWardPower);

                    logger::debug(
                        "WardPower settled | actor={} | base_before={:.2f} | applied={:.2f} | settled={:.2f}",
                        followupActor->GetName(),
                        before,
                        dmg,
                        settled
                    );
                },
                50ms
            );
        },
        0ns
    );
}

void Mechanics::ApplyStagger(RE::Actor* a_actor, const RE::NiPoint3& a_sourcePosition, float a_magnitude) {
    if (!a_actor) {
        return;
    }

    auto handle = a_actor->CreateRefHandle();
    stl::add_thread_task(
        [handle, a_sourcePosition, a_magnitude] {
            const auto actorPtr = handle.get();
            auto* actor = actorPtr ? actorPtr->As<RE::Actor>() : nullptr;
            if (!actor) {
                return;
            }

            const auto heading = actor->GetHeadingAngle(a_sourcePosition, false);
            const auto dir = heading >= 0.f ? heading / 360.f : (360.f + heading) / 360.f;

            logger::debug(
                "Applying stagger | actor={} | magnitude={:.2f} | heading={:.2f} | dir={:.3f}",
                actor->GetName(),
                a_magnitude,
                heading,
                dir
            );

            actor->SetGraphVariableFloat("staggerDirection", dir);
            actor->SetGraphVariableFloat("StaggerMagnitude", a_magnitude);
            actor->NotifyAnimationGraph("staggerStart");
        },
        0ns
    );
}

void Mechanics::FlagBlock(RE::Actor* a_actor) {
    GetRecentBlocks().flag(a_actor);
}

[[nodiscard]] bool Mechanics::ConsumeBlock(RE::Actor* a_actor) {
    return GetRecentBlocks().consume(a_actor);
}

[[nodiscard]] bool Mechanics::IsDiseaseSpell(RE::MagicItem* a_spell) {
    if (!a_spell) {
        return false;
    }

    if (const auto* spellItem = a_spell->As<RE::SpellItem>()) {
        if (spellItem->GetSpellType() == RE::MagicSystem::SpellType::kDisease) {
            return true;
        }
    }

    const auto formID = a_spell->GetFormID();
    return formID && FormCache::GetSingleton()->IsDiseaseSpell(formID);
}

[[nodiscard]] bool Mechanics::IsCloakSpell(const RE::MagicItem* a_spell, const RE::EffectSetting* a_effect) {
    if (!a_spell || !a_effect) {
        return false;
    }

    const bool isHostile = a_effect->IsHostile();
    const bool isCloakArchetype = a_effect->data.archetype == RE::EffectArchetypes::ArchetypeID::kCloak;
    const bool hasMagicCloakKeyword = a_effect->HasKeywordString("MagicCloak");

    if (isHostile && (isCloakArchetype || hasMagicCloakKeyword)) {
        return true;
    }

    const auto formID = a_spell->GetFormID();
    return formID && FormCache::GetSingleton()->IsCloakSpell(formID);
}

[[nodiscard]] bool Mechanics::IsCloakDamageSpell(const RE::MagicItem* a_spell, RE::Actor* a_caster) {
    if (!a_spell || !a_caster) {
        return false;
    }

    const auto spellFormID = a_spell->GetFormID();
    if (!spellFormID) {
        return false;
    }

    auto* magicTarget = a_caster->AsMagicTarget();
    if (!magicTarget) {
        return false;
    }

    if (REL::Module::IsVR()) {
        bool found = false;
        magicTarget->VisitActiveEffects([spellFormID, &found](RE::ActiveEffect* a_activeEffect) {
            if (!a_activeEffect) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto* baseEffect = a_activeEffect->GetBaseObject();
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

        return found;
    }

    auto* effectList = magicTarget->GetActiveEffectList();
    if (!effectList) {
        return false;
    }

    return std::ranges::any_of(*effectList, [spellFormID](const RE::ActiveEffect* a_activeEffect) {
        if (!a_activeEffect) {
            return false;
        }

        const RE::EffectSetting* baseEffect = a_activeEffect->GetBaseObject();
        if (!baseEffect) {
            return false;
        }

        if (baseEffect->data.archetype != RE::EffectArchetypes::ArchetypeID::kCloak) {
            return false;
        }

        const RE::TESForm* associatedForm = baseEffect->data.associatedForm;
        return associatedForm && associatedForm->GetFormID() == spellFormID;
    });
}

[[nodiscard]] bool Mechanics::IsWardSpell(const RE::SpellItem* a_spell) {
    if (!a_spell) {
        return false;
    }
    auto* keyword = FormCache::GetSingleton()->GetWardKeyword();
    if (!keyword) {
        return false;
    }
    return std::ranges::any_of(a_spell->effects, [keyword](const RE::Effect* a_effect) {
        return a_effect && a_effect->baseEffect && a_effect->baseEffect->HasKeyword(keyword);
    });
}
