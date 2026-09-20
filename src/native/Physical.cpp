#include <RE/Skyrim.h> // IWYU pragma: keep

#include "GameTasks.h"
#include "Mechanics.h"
#include "Physical.h"
#include "Settings.h"
#include "SkyrimUtil.h"
#include <RE/A/Actor.h>
#include <RE/A/ActorValues.h>
#include <RE/H/HitData.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESObjectWEAP.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <atomic>

#include <optional>

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) File-local state shared by the hit and XP hooks.
std::atomic_bool recentWardBlock {false};

[[nodiscard]] bool ShouldBlock(const RE::HitData& a_hitData) {
    const auto* settings = Settings::GetSingleton();
    const auto* weapon = a_hitData.weapon;
    const auto weaponType = weapon != nullptr ? weapon->GetWeaponType() : RE::WeaponTypes::kHandToHandMelee;
    const bool isBow = weaponType == RE::WeaponTypes::kBow || weaponType == RE::WeaponTypes::kCrossbow;
    const bool isBash = a_hitData.flags.any(RE::HitData::Flag::kBash, RE::HitData::Flag::kTimedBash);
    const bool isProjectile = isBow && !isBash;
    if (!(isProjectile ? settings->blockArrows.load() : settings->blockMelee.load())) {
        return false;
    }
    return Mechanics::ShouldApply(
        a_hitData.target.get().get(),
        a_hitData.aggressor.get().get(),
        Mechanics::Feature::kPhysical
    );
}

void ApplyMeleeFeedback(RE::Actor* a_attacker, const RE::Actor* a_defender, const bool a_isPowerAttack) {
    const auto* settings = Settings::GetSingleton();

    if ((a_attacker != nullptr) && a_isPowerAttack && settings->staggerPowerAttacks) {
        const auto attackerHandle = a_attacker->CreateRefHandle();
        GameTasks::Add([attackerHandle] {
            const auto attackerPtr = attackerHandle.get();
            if (auto* attacker = attackerPtr ? attackerPtr.get() : nullptr) {
                attacker->NotifyAnimationGraph("recoilLargeStart");
            }
        });
    } else if (!a_isPowerAttack && settings->staggerNormalAttacks) {
        if ((a_attacker != nullptr) && (a_defender != nullptr)) {
            Mechanics::ApplyStagger(a_attacker, a_defender->GetPosition(), settings->staggerMagnitude);
        }
    }
}

void GrantPlayerBlockXP(float a_wardDamage, const float a_xpScale) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    const auto* avOwner = (player != nullptr) ? player->AsActorValueOwner() : nullptr;
    if ((avOwner == nullptr) || a_wardDamage <= 0.F) {
        return;
    }

    recentWardBlock = true;

    const float skill = avOwner->GetActorValue(RE::ActorValue::kRestoration);
    const float skillScale = std::clamp(1.F - (skill / 100.F), 0.1F, 1.F);
    const float experience = a_wardDamage * a_xpScale * skillScale;

    if (experience <= 0.F) {
        return;
    }

    GameTasks::Add([experience] {
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            player->AddSkillExperience(RE::ActorValue::kRestoration, experience);
        }
    });

    SKSE::log::debug(
        "Granted Restoration XP | xp={:.2f} | wardDamage={:.2f} | skillLvl={:.0f} | scale={:.2f}",
        experience,
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
    if ((a_defender == nullptr) || (a_attacker == nullptr) || (a_weapon == nullptr)) {
        return 0.F;
    }

    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return 0.F;
    }

    const auto* avAttacker = a_attacker->AsActorValueOwner();
    if (avAttacker == nullptr) {
        return 0.F;
    }

    const auto minSkillMult = Mechanics::GetGameSettingFloat(
        a_attacker == player ? "fDamagePCSkillMin" : "fDamageSkillMin"
    );
    const auto maxSkillMult = Mechanics::GetGameSettingFloat(
        a_attacker == player ? "fDamagePCSkillMax" : "fDamageSkillMax"
    );

    auto weaponDamage = static_cast<float>(a_weapon->GetAttackDamage());
    auto attackerSkillLevel = 1.F;
    auto avMod = 1.F;
    auto avPowerMod = 1.F;

    auto estimatedWardDamage = 0.F;

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
            avMod += avAttacker->GetActorValue(RE::ActorValue::kMarksmanModifier) / 100.F;
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kMarksmanPowerModifier) / 100.F;
            break;
        case RE::WeaponTypes::kTwoHandSword:
        case RE::WeaponTypes::kTwoHandAxe:
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kTwoHanded);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kTwoHandedModifier) / 100.F;
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kTwoHandedPowerModifier) / 100.F;
            break;
        default:
            attackerSkillLevel = avAttacker->GetActorValue(RE::ActorValue::kOneHanded);
            avMod += avAttacker->GetActorValue(RE::ActorValue::kOneHandedModifier) / 100.F;
            avPowerMod += avAttacker->GetActorValue(RE::ActorValue::kOneHandedPowerModifier) / 100.F;
            break;
    }

    if (a_weapon->GetWeaponType() != RE::WeaponTypes::kHandToHandMelee) {
        const float skillDamageMult = minSkillMult + ((maxSkillMult - minSkillMult) * attackerSkillLevel / 100.F);

        estimatedWardDamage = weaponDamage
                              * Settings::GetSingleton()->powerDamageMultiplier
                              * skillDamageMult
                              * avMod
                              * avPowerMod;
    }

    if (a_powerAttack) {
        estimatedWardDamage *= Mechanics::GetGameSettingFloat("fPowerAttackDefaultBonus", 0.F) + 1.F;
    }

    return estimatedWardDamage;
}
}

// needed for melee attacks to not use metal impact sound
void Physical::ModifyHitData(RE::HitData* a_hitData) {
    if (ShouldBlock(*a_hitData)) {
        a_hitData->totalDamage = 0.F;
        a_hitData->percentBlocked = 1.F;
        a_hitData->flags.set(RE::HitData::Flag::kBlocked);
        a_hitData->flags.set(RE::HitData::Flag::kBlockWithWeapon);
    }
}

[[nodiscard]] std::optional<float> Physical::GetBlockCost(const RE::HitData& a_hitData) {
    if (ShouldBlock(a_hitData)) {
        return 0.F;
    }
    return std::nullopt;
}

void Physical::OnCombatHit(RE::HitData* a_hitData) {
    auto* attacker = a_hitData->aggressor.get().get();
    auto* defender = a_hitData->target.get().get();

    if (!ShouldBlock(*a_hitData)) {
        return;
    }

    const auto* settings = Settings::GetSingleton();
    const auto* const weapon = a_hitData->weapon;
    const bool isMelee = (weapon != nullptr) ? weapon->IsMelee() : false;
    const bool isPowerAtk = a_hitData->flags.any(RE::HitData::Flag::kPowerAttack);

    if (isMelee) {
        ApplyMeleeFeedback(attacker, defender, isPowerAtk);
    }

    a_hitData->totalDamage = 0.F;
    a_hitData->percentBlocked = 1.F;
    a_hitData->flags.set(RE::HitData::Flag::kBlocked);
    a_hitData->flags.set(RE::HitData::Flag::kBlockWithWeapon);

    const float wardDamage = EstimateWardPowerDamage(defender, attacker, weapon, isPowerAtk);

    SKSE::log::debug(
        "Blocked attack | defender={} | attacker={} | weapon={} | damage={:.1f} | power={}",
        defender->GetName(),
        (attacker != nullptr) ? attacker->GetName() : "none",
        (weapon != nullptr) ? weapon->GetName() : "unarmed",
        wardDamage,
        isPowerAtk
    );

    Mechanics::DamageWardPower(defender, wardDamage);

    if (defender->IsPlayer() || defender->IsPlayerRef()) {
        GrantPlayerBlockXP(wardDamage, settings->blockXPScale);
    }

    const auto defenderHandle = defender->CreateRefHandle();
    GameTasks::Add([defenderHandle] {
        const auto defenderPtr = defenderHandle.get();
        if (const auto* def = defenderPtr ? defenderPtr.get() : nullptr) {
            stl::PlaySound(def, "MAGWardTestDeflect");
        }
    });
}

[[nodiscard]] bool Physical::ShouldSuppressSkillXP(const RE::ActorValue a_av) {
    switch (a_av) {
        case RE::ActorValue::kBlock:
        case RE::ActorValue::kLightArmor:
        case RE::ActorValue::kHeavyArmor: return recentWardBlock.exchange(false);
        default:                          return false;
    }
}
