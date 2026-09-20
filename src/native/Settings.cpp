#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Settings.h"
#include "SettingsData.h"
#include "SettingsFile.h"
#include <BMK/Settings.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESObjectACTI.h>
#include <SKSE/SKSE.h>
#include <utility>

namespace {
void ApplyShoutMode(SettingsData& a_data) {
    switch (a_data.shoutMode) {
        case ShoutMode::kVanilla:
            a_data.shoutPassThrough = false;
            a_data.staggerDefenderOnBreak = false;
            a_data.shoutInstantBreak = false;
            a_data.shoutDamage = 0.0F;
            break;

        case ShoutMode::kBreakOnly:
            a_data.shoutPassThrough = false;
            a_data.staggerDefenderOnBreak = false;
            break;

        case ShoutMode::kBreakWithStagger:
            a_data.shoutPassThrough = false;
            a_data.staggerDefenderOnBreak = true;
            break;

        case ShoutMode::kBreakWithPassThrough:
            a_data.shoutPassThrough = true;
            a_data.staggerDefenderOnBreak = false;
            break;
    }
}

[[nodiscard]] bool LiveSettingsEqual(const Settings& a_settings, const SettingsData& a_data) {
    return a_settings.showWardMeter.load()
           == a_data.showWardMeter
           && a_settings.perkGatesPlayerOnly.load()
           == a_data.perkGatesPlayerOnly
           && a_settings.debugLogging.load()
           == a_data.debugLogging
           && a_settings.instantWardCharge.load()
           == a_data.instantWardCharge
           && a_settings.instantWardCast.load()
           == a_data.instantWardCast
           && a_settings.wardChargeRateMultiplier.load()
           == a_data.wardChargeRateMultiplier
           && a_settings.wardMagnitudeMultiplier.load()
           == a_data.wardMagnitudeMultiplier
           && a_settings.wardCostMultiplier.load()
           == a_data.wardCostMultiplier
           && a_settings.restrictTweaksToPlayerTeam.load()
           == a_data.restrictTweaksToPlayerTeam
           && a_settings.blockMelee.load()
           == a_data.blockMelee
           && a_settings.blockArrows.load()
           == a_data.blockArrows
           && a_settings.staggerNormalAttacks.load()
           == a_data.staggerNormalAttacks
           && a_settings.staggerMagnitude.load()
           == a_data.staggerMagnitude
           && a_settings.staggerPowerAttacks.load()
           == a_data.staggerPowerAttacks
           && a_settings.blockingAngle.load()
           == a_data.blockingAngle
           && a_settings.powerDamageMultiplier.load()
           == a_data.powerDamageMultiplier
           && a_settings.blockXPScale.load()
           == a_data.blockXPScale
           && a_settings.shoutMode.load()
           == a_data.shoutMode
           && a_settings.staggerDefenderOnBreak.load()
           == a_data.staggerDefenderOnBreak
           && a_settings.staggerMagnitudeTowardDefender.load()
           == a_data.staggerMagnitudeTowardDefender
           && a_settings.shoutPassThrough.load()
           == a_data.shoutPassThrough
           && a_settings.playerImmuneToShoutMechanics.load()
           == a_data.playerImmuneToShoutMechanics
           && a_settings.shoutDamage.load()
           == a_data.shoutDamage
           && a_settings.shoutInstantBreak.load()
           == a_data.shoutInstantBreak
           && a_settings.blockDiseases.load()
           == a_data.blockDiseases
           && a_settings.blockCloaks.load()
           == a_data.blockCloaks
           && a_settings.cloakDamageMultiplier.load()
           == a_data.cloakDamageMultiplier
           && a_settings.enableSpellReflection.load()
           == a_data.enableSpellReflection
           && a_settings.autoAimReflection.load()
           == a_data.autoAimReflection
           && a_settings.reflectionBlameAttacker.load()
           == a_data.reflectionBlameAttacker
           && a_settings.reflectionForwardOffset.load()
           == a_data.reflectionForwardOffset
           && a_settings.reflectEvenIfWardBroken.load()
           == a_data.reflectEvenIfWardBroken
           && a_settings.restrictReflectionToPlayerTeam.load()
           == a_data.restrictReflectionToPlayerTeam
           && a_settings.excludedItemsDisablePhysicalBlocking.load()
           == a_data.excludedItemsDisablePhysicalBlocking
           && a_settings.excludedItemsDisableShoutMechanics.load()
           == a_data.excludedItemsDisableShoutMechanics
           && a_settings.excludedItemsDisableDiseaseBlocking.load()
           == a_data.excludedItemsDisableDiseaseBlocking
           && a_settings.excludedItemsDisableCloakBlocking.load()
           == a_data.excludedItemsDisableCloakBlocking
           && a_settings.excludedItemsDisableReflection.load()
           == a_data.excludedItemsDisableReflection;
}

void ApplyLiveSettings(Settings& a_settings, const SettingsData& a_data) {
    a_settings.showWardMeter.store(a_data.showWardMeter);
    a_settings.perkGatesPlayerOnly.store(a_data.perkGatesPlayerOnly);
    a_settings.debugLogging.store(a_data.debugLogging);

    a_settings.instantWardCharge.store(a_data.instantWardCharge);
    a_settings.instantWardCast.store(a_data.instantWardCast);
    a_settings.wardChargeRateMultiplier.store(a_data.wardChargeRateMultiplier);
    a_settings.wardMagnitudeMultiplier.store(a_data.wardMagnitudeMultiplier);
    a_settings.wardCostMultiplier.store(a_data.wardCostMultiplier);
    a_settings.restrictTweaksToPlayerTeam.store(a_data.restrictTweaksToPlayerTeam);

    a_settings.blockMelee.store(a_data.blockMelee);
    a_settings.blockArrows.store(a_data.blockArrows);
    a_settings.staggerNormalAttacks.store(a_data.staggerNormalAttacks);
    a_settings.staggerMagnitude.store(a_data.staggerMagnitude);
    a_settings.staggerPowerAttacks.store(a_data.staggerPowerAttacks);
    a_settings.blockingAngle.store(a_data.blockingAngle);
    a_settings.powerDamageMultiplier.store(a_data.powerDamageMultiplier);
    a_settings.blockXPScale.store(a_data.blockXPScale);

    a_settings.shoutMode.store(a_data.shoutMode);
    a_settings.staggerDefenderOnBreak.store(a_data.staggerDefenderOnBreak);
    a_settings.staggerMagnitudeTowardDefender.store(a_data.staggerMagnitudeTowardDefender);
    a_settings.shoutPassThrough.store(a_data.shoutPassThrough);
    a_settings.playerImmuneToShoutMechanics.store(a_data.playerImmuneToShoutMechanics);
    a_settings.shoutDamage.store(a_data.shoutDamage);
    a_settings.shoutInstantBreak.store(a_data.shoutInstantBreak);

    a_settings.blockDiseases.store(a_data.blockDiseases);
    a_settings.blockCloaks.store(a_data.blockCloaks);
    a_settings.cloakDamageMultiplier.store(a_data.cloakDamageMultiplier);

    a_settings.enableSpellReflection.store(a_data.enableSpellReflection);
    a_settings.autoAimReflection.store(a_data.autoAimReflection);
    a_settings.reflectionBlameAttacker.store(a_data.reflectionBlameAttacker);
    a_settings.reflectionForwardOffset.store(a_data.reflectionForwardOffset);
    a_settings.reflectEvenIfWardBroken.store(a_data.reflectEvenIfWardBroken);
    a_settings.restrictReflectionToPlayerTeam.store(a_data.restrictReflectionToPlayerTeam);

    a_settings.excludedItemsDisablePhysicalBlocking.store(a_data.excludedItemsDisablePhysicalBlocking);
    a_settings.excludedItemsDisableShoutMechanics.store(a_data.excludedItemsDisableShoutMechanics);
    a_settings.excludedItemsDisableDiseaseBlocking.store(a_data.excludedItemsDisableDiseaseBlocking);
    a_settings.excludedItemsDisableCloakBlocking.store(a_data.excludedItemsDisableCloakBlocking);
    a_settings.excludedItemsDisableReflection.store(a_data.excludedItemsDisableReflection);
}

void ApplyStaticSettings(Settings& a_settings, const SettingsData& a_data) {
    a_settings.physicalRequiredPerks = a_data.physicalRequiredPerks;
    a_settings.damageKeywords = a_data.damageKeywords;
    a_settings.shoutsRequiredPerks = a_data.shoutsRequiredPerks;
    a_settings.diseaseSpells = a_data.diseaseSpells;
    a_settings.cloakSpells = a_data.cloakSpells;
    a_settings.diseaseRequiredPerks = a_data.diseaseRequiredPerks;
    a_settings.cloakRequiredPerks = a_data.cloakRequiredPerks;
    a_settings.reflectionRequiredPerks = a_data.reflectionRequiredPerks;
    a_settings.excludedItems = a_data.excludedItems;
    a_settings.shoutExclusions = a_data.shoutExclusions;
    a_settings.reflectionExclusions = a_data.reflectionExclusions;
}
}

void Settings::Load() {
    auto data = ReadSettings("Data", SettingsReadMode::kAll);
    if (!data) {
        return;
    }
    ApplyShoutMode(*data);
    ApplyLiveSettings(*this, *data);
    ApplyStaticSettings(*this, *data);

    BMK::Settings::ApplyLogLevel(debugLogging.load(), SKSE::InitInfo {}.logLevel);

    SKSE::log::info(
        "Settings: loaded | path={} | shoutMode={} | physicalDamageMultiplier={} | reflectionEnabled={}",
        "Data/MCM/Settings/PerfectlyValidWards.ini",
        std::to_underlying(shoutMode.load()),
        powerDamageMultiplier.load(),
        enableSpellReflection.load()
    );
}

Settings::ReloadResult Settings::Reload() {
    auto data = ReadSettings("Data", SettingsReadMode::kLiveOnly);
    if (!data) {
        return {};
    }
    ApplyShoutMode(*data);
    const auto changed = !LiveSettingsEqual(*this, *data);
    ApplyLiveSettings(*this, *data);

    BMK::Settings::ApplyLogLevel(debugLogging.load(), SKSE::InitInfo {}.logLevel);

    SKSE::log::info(
        "Settings: reloaded | changed={} | path={} | physicalDamageMultiplier={}",
        changed,
        "Data/MCM/Settings/PerfectlyValidWards.ini",
        powerDamageMultiplier.load()
    );
    return {.changed = changed};
}

void Settings::ResolveRuntimeData() {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (data == nullptr) {
        return;
    }

    emptyActivator = data->LookupForm<RE::TESObjectACTI>(kFXEmptyActivator, kSkyrimESM);
    if (emptyActivator == nullptr) {
        SKSE::log::warn("Settings: failed to resolve FXEmptyActivator");
    } else {
        SKSE::log::info("Settings: resolved FXEmptyActivator <{:08X}>", emptyActivator->GetFormID());
    }
}
