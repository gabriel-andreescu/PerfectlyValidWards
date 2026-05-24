#include "Physical.h"
#include "Mechanics.h"
#include "Settings.h"

namespace {
void ApplyMeleeFeedback(RE::Actor* a_attacker, const RE::Actor* a_defender, const bool a_isPowerAttack) {
    const auto* settings = Settings::GetSingleton();

    if (a_attacker && a_isPowerAttack && settings->staggerPowerAttacks) {
        auto attackerHandle = a_attacker->CreateRefHandle();
        stl::add_thread_task(
            [attackerHandle] {
                const auto attackerPtr = attackerHandle.get();
                if (auto* attacker = attackerPtr ? attackerPtr.get() : nullptr) {
                    attacker->NotifyAnimationGraph("recoilLargeStart");
                }
            },
            0ns
        );
    } else if (!a_isPowerAttack && settings->staggerNormalAttacks) {
        if (a_attacker && a_defender) {
            Mechanics::ApplyStagger(a_attacker, a_defender->GetPosition(), settings->staggerMagnitude);
        }
    }
}

void GrantPlayerBlockXP(float a_wardDamage, const float a_xpScale) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    const auto* avOwner = player ? player->AsActorValueOwner() : nullptr;
    if (!avOwner || a_wardDamage <= 0.f) {
        return;
    }

    Mechanics::FlagBlock(player);

    const float skill = avOwner->GetActorValue(RE::ActorValue::kRestoration);
    const float skillScale = std::clamp(1.f - skill / 100.f, 0.1f, 1.f);
    const float xp = a_wardDamage * a_xpScale * skillScale;

    if (xp <= 0.f) {
        return;
    }

    stl::add_thread_task(
        [xp] {
            if (auto* pc = RE::PlayerCharacter::GetSingleton()) {
                pc->AddSkillExperience(RE::ActorValue::kRestoration, xp);
            }
        },
        0ns
    );

    logger::debug(
        "Granted Restoration XP | xp={:.2f} | wardDamage={:.2f} | skillLvl={:.0f} | scale={:.2f}",
        xp,
        a_wardDamage,
        skill,
        skillScale
    );
}

[[nodiscard]] float EstimateWardPowerDamage(
    const RE::Actor* a_defender,
    RE::Actor* a_attacker,
    const RE::TESObjectWEAP* a_weapon,
    const bool a_powerAttack
) {
    if (!a_defender || !a_attacker || !a_weapon) {
        return 0.f;
    }

    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return 0.f;
    }

    const auto* avAttacker = a_attacker->AsActorValueOwner();
    if (!avAttacker) {
        return 0.f;
    }

    const auto minSkillMult = Mechanics::GetGameSettingFloat(
        a_attacker == player ? "fDamagePCSkillMin" : "fDamageSkillMin"
    );
    const auto maxSkillMult = Mechanics::GetGameSettingFloat(
        a_attacker == player ? "fDamagePCSkillMax" : "fDamageSkillMax"
    );

    auto weaponDamage = static_cast<float>(a_weapon->GetAttackDamage());
    auto attackerSkillLevel = 1.f;
    auto avMod = 1.f;
    auto avPowerMod = 1.f;

    auto estimatedWardDamage = 0.f;

    switch (a_weapon->GetWeaponType()) {
        case RE::WeaponTypes::kHandToHandMelee:
            estimatedWardDamage = avAttacker->GetActorValue(RE::ActorValue::kUnarmedDamage)
                                  * Settings::GetSingleton()->powerDamageMultiplier;
            break;
        case RE::WeaponTypes::kBow:
        case RE::WeaponTypes::kCrossbow:
            if (const auto* ammo = a_attacker->GetCurrentAmmo()) {
                weaponDamage += ammo->GetRuntimeData().data.damage;
            }
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kArchery);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kMarksmanModifier) / 100.f;
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kMarksmanPowerModifier) / 100.f;
            break;
        case RE::WeaponTypes::kTwoHandSword:
        case RE::WeaponTypes::kTwoHandAxe:
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kTwoHanded);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kTwoHandedModifier);
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kTwoHandedPowerModifier);
            break;
        default:
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kOneHanded);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kOneHandedModifier);
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kOneHandedPowerModifier);
            break;
    }

    if (a_weapon->GetWeaponType() != RE::WeaponTypes::kHandToHandMelee) {
        const float skillDamageMult = minSkillMult + (maxSkillMult - minSkillMult) * attackerSkillLevel / 100.f;

        estimatedWardDamage = weaponDamage
                              * Settings::GetSingleton()->powerDamageMultiplier
                              * skillDamageMult
                              * avMod
                              * avPowerMod;
    }

    if (a_powerAttack) {
        estimatedWardDamage *= Mechanics::GetGameSettingFloat("fPowerAttackDefaultBonus", 0.f) + 1.f;
    }

    return estimatedWardDamage;
}
}

// needed for melee attacks to not use metal impact sound
void Physical::ModifyHitData(RE::HitData* a_hitData) {
    const auto* attacker = a_hitData->aggressor.get().get();

    if (auto* defender = a_hitData->target.get().get();
        Mechanics::ShouldApply(defender, attacker, Mechanics::Feature::kPhysical)) {
        a_hitData->totalDamage = 0.f;
        a_hitData->percentBlocked = 1.f;
        a_hitData->flags.set(RE::HitData::Flag::kBlocked);
        a_hitData->flags.set(RE::HitData::Flag::kBlockWithWeapon);
    }
}

[[nodiscard]] std::optional<float> Physical::GetBlockCost(const RE::HitData& a_hitData) {
    const auto* attacker = a_hitData.aggressor.get().get();

    if (auto* defender = a_hitData.target.get().get();
        Mechanics::ShouldApply(defender, attacker, Mechanics::Feature::kPhysical)) {
        return 0.f;
    }
    return std::nullopt;
}

void Physical::OnCombatHit(RE::HitData* a_hitData) {
    auto* attacker = a_hitData->aggressor.get().get();
    auto* defender = a_hitData->target.get().get();

    if (!Mechanics::ShouldApply(defender, attacker, Mechanics::Feature::kPhysical)) {
        return;
    }

    const auto* settings = Settings::GetSingleton();
    const auto* const weapon = a_hitData->weapon;
    const bool isMelee = weapon ? weapon->IsMelee() : false;
    const bool isPowerAtk = a_hitData->flags.any(RE::HitData::Flag::kPowerAttack);

    if (isMelee) {
        ApplyMeleeFeedback(attacker, defender, isPowerAtk);
    }

    a_hitData->totalDamage = 0.f;
    a_hitData->percentBlocked = 1.f;
    a_hitData->flags.set(RE::HitData::Flag::kBlocked);
    a_hitData->flags.set(RE::HitData::Flag::kBlockWithWeapon);

    const float wardDamage = EstimateWardPowerDamage(defender, attacker, weapon, isPowerAtk);

    logger::debug(
        "Blocked attack | defender={} | attacker={} | weapon={} | damage={:.1f} | power={}",
        defender->GetName(),
        attacker ? attacker->GetName() : "none",
        weapon ? weapon->GetName() : "unarmed",
        wardDamage,
        isPowerAtk
    );

    Mechanics::DamageWardPower(defender, wardDamage);

    if (defender->IsPlayer() || defender->IsPlayerRef()) {
        GrantPlayerBlockXP(wardDamage, settings->blockXPScale);
    }

    auto defenderHandle = defender->CreateRefHandle();
    stl::add_thread_task(
        [defenderHandle] {
            const auto defenderPtr = defenderHandle.get();
            if (const auto* def = defenderPtr ? defenderPtr.get() : nullptr) {
                stl::play_sound(def, "MAGWardTestDeflect");
            }
        },
        0ns
    );
}

[[nodiscard]] Physical::XPFilterResult Physical::FilterSkillXP(const RE::ActorValue a_av) {
    if ((a_av == RE::ActorValue::kBlock || a_av == RE::ActorValue::kLightArmor || a_av == RE::ActorValue::kHeavyArmor)
        && Mechanics::ConsumeBlock(RE::PlayerCharacter::GetSingleton())) {
        return {.suppress = true};
    }
    return {.suppress = false};
}
