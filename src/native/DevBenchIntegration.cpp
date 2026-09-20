#include <RE/Skyrim.h> // IWYU pragma: keep

#include "DevBenchIntegration.h"
#include "Patches.h"
#include "Settings.h"

#include <BMK/Skyrim/DevBench.h>
#include <RE/A/AIProcess.h>
#include <RE/A/AccumulatingValueModifierEffect.h>
#include <RE/A/ActiveEffect.h>
#include <RE/A/Actor.h>
#include <RE/A/ActorValueOwner.h>
#include <RE/A/ActorValues.h>
#include <RE/B/BGSCollisionLayer.h>
#include <RE/B/BGSImpactData.h>
#include <RE/B/BGSImpactDataSet.h>
#include <RE/B/BGSMaterialType.h>
#include <RE/B/BSContainer.h>
#include <RE/E/EffectArchetypes.h>
#include <RE/E/EffectSetting.h>
#include <RE/M/MagicCaster.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicSystem.h>
#include <RE/M/MagicTarget.h>
#include <RE/M/MiddleHighProcessData.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/T/TESDataHandler.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace {
std::string InspectCaster(RE::Actor& a_actor, RE::MagicSystem::CastingSource a_hand) {
    const auto* caster = a_actor.GetMagicCaster(a_hand);
    if (caster == nullptr) {
        return "null";
    }
    return std::format(
        R"({{"spell":{},"state":{},"castingTimer":{},"chargeTime":{}}})",
        caster->currentSpell != nullptr ? caster->currentSpell->GetFormID() : 0,
        static_cast<std::uint32_t>(caster->state.get()),
        caster->castingTimer,
        caster->currentSpell != nullptr ? caster->currentSpell->GetChargeTime() : 0.0F
    );
}

std::string InspectWardEffects(RE::Actor& a_actor) {
    std::string effects = "[";
    RE::MagicTarget::EffectVisitor visitor([&effects](RE::ActiveEffect* a_effect) {
        const auto* base = a_effect != nullptr ? a_effect->GetBaseObject() : nullptr;
        if (base == nullptr || base->data.primaryAV != RE::ActorValue::kWardPower) {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        std::string accumulation = "null";
        if (base->data.archetype == RE::EffectArchetypes::ArchetypeID::kAccumulateMagnitude) {
            const auto* ward = static_cast<const RE::AccumulatingValueModifierEffect*>(a_effect);
            accumulation = std::format(
                R"({{"accumulatedMagnitude":{},"maximumMagnitude":{},"holdTimer":{}}})",
                ward->accumulatedMagnitude,
                ward->maximumMagnitude,
                ward->holdTimer
            );
        }
        if (effects.size() > 1) {
            effects += ',';
        }
        effects += std::format(
            R"({{"spell":{},"effect":{},"inactive":{},"dispelled":{},"magnitude":{},)"
            R"("elapsed":{},"accumulation":{}}})",
            a_effect->spell != nullptr ? a_effect->spell->GetFormID() : 0,
            base->GetFormID(),
            a_effect->flags.any(RE::ActiveEffect::Flag::kInactive),
            a_effect->flags.any(RE::ActiveEffect::Flag::kDispelled),
            a_effect->magnitude,
            a_effect->elapsedSeconds,
            accumulation
        );
        return RE::BSContainer::ForEachResult::kContinue;
    });
    a_actor.AsMagicTarget()->VisitEffects(visitor);
    return effects + ']';
}

std::string InspectActor(RE::Actor& a_actor) {
    const auto* process = a_actor.GetActorRuntimeData().currentProcess;
    const auto* middleHigh = process != nullptr ? process->middleHigh : nullptr;
    return std::format(
        R"({{"formId":{},"blocking":{},"wardPower":{},"maximumWardPower":{},"right":{},"left":{},"wardEffects":{}}})",
        a_actor.GetFormID(),
        a_actor.IsBlocking(),
        a_actor.AsActorValueOwner()->GetActorValue(RE::ActorValue::kWardPower),
        middleHigh != nullptr ? middleHigh->maximumWardPower : 0.0F,
        InspectCaster(a_actor, RE::MagicSystem::CastingSource::kRightHand),
        InspectCaster(a_actor, RE::MagicSystem::CastingSource::kLeftHand),
        InspectWardEffects(a_actor)
    );
}

std::string InspectNpcs() {
    std::string actors = "[";
    RE::ProcessLists::GetSingleton()->ForEachHighActor([&actors](RE::Actor* a_actor) {
        if (!a_actor->IsPlayerRef()) {
            if (actors.size() > 1) {
                actors += ',';
            }
            actors += InspectActor(*a_actor);
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return actors + ']';
}

std::string InspectSettings() {
    const auto& settings = *Settings::GetSingleton();
    return std::format(
        R"({{"showWardMeter":{},"debugLogging":{},"instantWardCharge":{},"instantWardCast":{},"chargeRate":{},"magnitude":{},"cost":{},)"
        R"("restrictTweaksToPlayerTeam":{},"physicalDamage":{},"blockXPScale":{},"blockingAngle":{},)"
        R"("blockMelee":{},"blockArrows":{},)"
        R"("shoutMode":{},"shoutDamage":{},"shoutInstantBreak":{},"shoutPassThrough":{},)"
        R"("playerImmuneToShouts":{},"blockDiseases":{},"blockCloaks":{},"cloakDamage":{},)"
        R"("reflection":{},"autoAimReflection":{},"reflectEvenIfWardBroken":{}}})",
        settings.showWardMeter.load(),
        settings.debugLogging.load(),
        settings.instantWardCharge.load(),
        settings.instantWardCast.load(),
        settings.wardChargeRateMultiplier.load(),
        settings.wardMagnitudeMultiplier.load(),
        settings.wardCostMultiplier.load(),
        settings.restrictTweaksToPlayerTeam.load(),
        settings.powerDamageMultiplier.load(),
        settings.blockXPScale.load(),
        settings.blockingAngle.load(),
        settings.blockMelee.load(),
        settings.blockArrows.load(),
        std::to_underlying(settings.shoutMode.load()),
        settings.shoutDamage.load(),
        settings.shoutInstantBreak.load(),
        settings.shoutPassThrough.load(),
        settings.playerImmuneToShoutMechanics.load(),
        settings.blockDiseases.load(),
        settings.blockCloaks.load(),
        settings.cloakDamageMultiplier.load(),
        settings.enableSpellReflection.load(),
        settings.autoAimReflection.load(),
        settings.reflectEvenIfWardBroken.load()
    );
}

std::string InspectPhysicalPatches() {
    auto* data = RE::TESDataHandler::GetSingleton();
    const auto* weapon = data->LookupForm<RE::BGSCollisionLayer>(Patches::IDs::kWeaponCol, Settings::kSkyrimESM);
    const auto* projectile = data->LookupForm<RE::BGSCollisionLayer>(
        Patches::IDs::kProjectileCol,
        Settings::kSkyrimESM
    );
    const auto* ward = data->LookupForm<RE::BGSCollisionLayer>(Patches::IDs::kWardCol, Settings::kSkyrimESM);
    const auto* material = data->LookupForm<RE::BGSMaterialType>(Patches::IDs::kWardMaterial, Settings::kSkyrimESM);
    const auto* impacts = data->LookupForm<RE::BGSImpactDataSet>(Patches::IDs::kArrowImpactSet, Settings::kSkyrimESM);
    if (weapon == nullptr || projectile == nullptr || ward == nullptr || material == nullptr || impacts == nullptr) {
        return "null";
    }
    const auto impact = impacts->impactMap.find(material);
    return std::format(
        R"({{"weaponToWard":{},"wardToWeapon":{},"projectileToWard":{},"wardToProjectile":{},"arrowWardImpact":{}}})",
        std::ranges::contains(weapon->collidesWith, ward),
        std::ranges::contains(ward->collidesWith, weapon),
        std::ranges::contains(projectile->collidesWith, ward),
        std::ranges::contains(ward->collidesWith, projectile),
        impact != impacts->impactMap.end() && impact->second != nullptr ? impact->second->GetFormID() : 0
    );
}

std::string Inspect() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return R"({"ok":false,"error":"The player is not loaded"})";
    }
    auto result = InspectActor(*player);
    result.pop_back();
    return std::format(
        R"({},"ok":true,"npcs":{},"settings":{},"physicalPatches":{}}})",
        result,
        InspectNpcs(),
        InspectSettings(),
        InspectPhysicalPatches()
    );
}

constexpr BMK::Skyrim::DevBench::Inspection kInspection {
    .name = "perfectlyvalidwards",
    .descriptor
    = R"({"description":"PerfectlyValidWards player and nearby NPC ward power, casting states, effects, physical patches and active settings.","readOnly":true})",
    .snapshot = Inspect,
    .timeoutResponse = R"({"ok":false,"error":"PerfectlyValidWards inspection timed out waiting for the game thread"})",
    .failureResponse = R"({"ok":false,"error":"PerfectlyValidWards inspection failed. See the plugin log."})",
};
}

void DevBenchIntegration::Register() {
    BMK::Skyrim::DevBench::RegisterInspection<kInspection>();
}
