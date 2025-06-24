#include "WardManager.h"
#include "Settings.h"

using namespace IDs;

namespace {
    void CopyFirstConditionFrom(RE::TESTopicInfo* sourceInfo, RE::TESTopicInfo* targetInfo) {
        if (!sourceInfo || !targetInfo || !sourceInfo->objConditions.head) {
            return;
        }

        auto*& targetHead = targetInfo->objConditions.head;
        if (!targetHead) {
            targetHead = sourceInfo->objConditions.head;
            return;
        }

        auto* current = targetHead;
        while (current->next) {
            current = current->next;
        }
        current->next = sourceInfo->objConditions.head;
    }
}

[[nodiscard]] float WardManager::GetCurrentWardPower(RE::Actor* a_actor) {
    if (auto* av = a_actor ? a_actor->AsActorValueOwner() : nullptr) {
        return av->GetActorValue(RE::ActorValue::kWardPower);
    }
    return 0.f;
}

[[nodiscard]] float WardManager::EstimateWardPowerDamage(
    RE::Actor* a_defender,
    RE::Actor* a_attacker,
    RE::TESObjectWEAP* a_weapon,
    bool a_powerAttack
) {
    if (!a_defender || !a_attacker || !a_weapon) {
        return 0.f;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return 0.f;
    }

    auto* avAttacker = a_attacker->AsActorValueOwner();
    if (!avAttacker) {
        return 0.f;
    }

    const auto minSkillMult = get_float_gs(a_attacker == player ? "fDamagePCSkillMin" : "fDamageSkillMin");
    const auto maxSkillMult = get_float_gs(a_attacker == player ? "fDamagePCSkillMax" : "fDamageSkillMax");

    auto weaponDamage = static_cast<float>(a_weapon->GetAttackDamage());
    auto attackerSkillLevel = 1.f;
    auto avMod = 1.f;
    auto avPowerMod = 1.f;

    auto estimatedWardDamage = 0.f;

    switch (a_weapon->GetWeaponType()) {
        case RE::WeaponTypes::kHandToHandMelee: { // unarmed
            estimatedWardDamage = avAttacker->GetActorValue(RE::ActorValue::kUnarmedDamage)
                                  * Settings::GetSingleton()->wardPowerDamageMultiplier;
        }
        break;

        case RE::WeaponTypes::kBow:
        case RE::WeaponTypes::kCrossbow: { // ranged
            if (const auto* ammo = a_attacker->GetCurrentAmmo()) {
                weaponDamage += ammo->GetRuntimeData().data.damage;
            }
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kArchery);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kMarksmanModifier) / 100.f;
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kMarksmanPowerModifier) / 100.f;
        }
        break;

        case RE::WeaponTypes::kTwoHandSword:
        case RE::WeaponTypes::kTwoHandAxe: { // two‑hand
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kTwoHanded);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kTwoHandedModifier);
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kTwoHandedPowerModifier);
        }
        break;

        default: { // one‑hand
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kOneHanded);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kOneHandedModifier);
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kOneHandedPowerModifier);
        }
        break;
    }

    if (a_weapon->GetWeaponType() != RE::WeaponTypes::kHandToHandMelee) {
        const float skillDamageMult =
                minSkillMult + (maxSkillMult - minSkillMult) * attackerSkillLevel / 100.f;

        estimatedWardDamage =
                weaponDamage *
                Settings::GetSingleton()->wardPowerDamageMultiplier *
                skillDamageMult *
                avMod *
                avPowerMod;
    }

    if (a_powerAttack) {
        estimatedWardDamage *= get_float_gs("fPowerAttackDefaultBonus", 0.f) + 1.f;
    }

    return estimatedWardDamage;
}

void WardManager::DamageWardPower(RE::Actor* a_actor, float a_damage) {
    if (auto* av = a_actor ? a_actor->AsActorValueOwner() : nullptr) {
        const auto before = av->GetActorValue(RE::ActorValue::kWardPower);
        auto dmg = std::min(a_damage, before);

        av->RestoreActorValue(
            RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kWardPower,
            -dmg
        );

        logger::debug(
            "Damaging Ward Power: {:.2f} → {:.2f}",
            before,
            av->GetActorValue(RE::ActorValue::kWardPower)
        );
    }
}

void WardManager::PatchCollisionLayers() {
    const auto data = RE::TESDataHandler::GetSingleton();
    auto* weapon = data->LookupForm<RE::BGSCollisionLayer>(WeaponCol, Settings::skyrimESM);
    auto* projectile = data->LookupForm<RE::BGSCollisionLayer>(ProjectileCol, Settings::skyrimESM);
    auto* ward = data->LookupForm<RE::BGSCollisionLayer>(WardCol, Settings::skyrimESM);

    if (!(weapon && projectile && ward)) {
        return;
    }

    reserve_extra(weapon->collidesWith, 1);     // +ward
    reserve_extra(projectile->collidesWith, 1); // +ward
    reserve_extra(ward->collidesWith, 2);       // +weapon +projectile

    insert_unique(weapon->collidesWith, ward);
    insert_unique(projectile->collidesWith, ward);
    insert_unique(ward->collidesWith, weapon);
    insert_unique(ward->collidesWith, projectile);

    sort_by_collision_idx(weapon->collidesWith);
    sort_by_collision_idx(projectile->collidesWith);
    sort_by_collision_idx(ward->collidesWith);
}

void WardManager::PatchImpactDataSets() {
    const auto data = RE::TESDataHandler::GetSingleton();
    auto* wardMaterial = data->LookupForm<RE::BGSMaterialType>(WardMaterial, Settings::skyrimESM);
    auto* arrowImpactDataSet = data->LookupForm<RE::BGSImpactDataSet>(ArrowImpactSet, Settings::skyrimESM);
    auto* arrowVsWardImpact = data->LookupForm<RE::BGSImpactData>(ArrowVsWardImpact, Settings::pluginName);

    if (wardMaterial && arrowImpactDataSet && arrowVsWardImpact) {
        arrowImpactDataSet->impactMap.insert({ wardMaterial, arrowVsWardImpact });
    }
}

void WardManager::PatchHitGrunts() {
    const auto data = RE::TESDataHandler::GetSingleton();
    auto* gruntPlaceholder = data->LookupForm<RE::TESTopicInfo>(GruntPlaceholder, Settings::pluginName);
    for (auto id: Grunts) {
        if (auto* grunt = data->LookupForm<RE::TESTopicInfo>(id, Settings::skyrimESM)) {
            CopyFirstConditionFrom(gruntPlaceholder, grunt);
        }
    }
}

void WardManager::FlagWardBlock(RE::Actor* a_actor) {
    g_wardBlocks.flag(a_actor);
}

[[nodiscard]] bool WardManager::ConsumeWardBlock(RE::Actor* a_actor) {
    return g_wardBlocks.consume(a_actor);
}


