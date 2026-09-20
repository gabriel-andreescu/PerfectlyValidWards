#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Effects.h"
#include "Mechanics.h"
#include "Settings.h"
#include <RE/A/Actor.h>
#include <RE/E/Effect.h>
#include <RE/E/EffectSetting.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicTarget.h>
#include <RE/T/TESObjectREFR.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <optional>
#include <string>

namespace {
enum class EffectBlockType {
    kNone,
    kDisease,
    kCloak,
};

[[nodiscard]] RE::Actor* ResolveActor(RE::TESObjectREFR* a_ref) {
    return (a_ref != nullptr) ? a_ref->As<RE::Actor>() : nullptr;
}

[[nodiscard]] EffectBlockType ClassifyBlockableEffect(
    const Settings* a_settings,
    RE::MagicItem* a_spell,
    const RE::EffectSetting* a_effectSetting,
    RE::TESObjectREFR* a_caster
) {
    if (a_settings->blockDiseases && Mechanics::IsDiseaseSpell(a_spell)) {
        return EffectBlockType::kDisease;
    }

    if (a_settings->blockCloaks) {
        if (Mechanics::IsCloakSpell(a_spell, a_effectSetting)) {
            return EffectBlockType::kCloak;
        }
        if (auto* casterActor = ResolveActor(a_caster);
            (casterActor != nullptr) && Mechanics::IsCloakDamageSpell(a_spell, casterActor)) {
            return EffectBlockType::kCloak;
        }
    }

    return EffectBlockType::kNone;
}

[[nodiscard]] bool IsDamagingEffect(const RE::EffectSetting* a_effectSetting) {
    if ((a_effectSetting == nullptr) || !a_effectSetting->IsHostile()) {
        return false;
    }

    const Settings* settings = Settings::GetSingleton();
    if (settings == nullptr) {
        return false;
    }

    return std::ranges::any_of(settings->damageKeywords, [a_effectSetting](const std::string& a_keyword) {
        return a_effectSetting->HasKeywordString(a_keyword);
    });
}

[[nodiscard]] bool AreEffectConditionsMet(const RE::Effect* a_effect, RE::Actor* a_subject, RE::Actor* a_target) {
    if ((a_effect == nullptr) || (a_effect->conditions.head == nullptr)) {
        return true;
    }
    if (a_subject == nullptr) {
        return false;
    }
    return a_effect->conditions.IsTrue(a_subject, a_target);
}
}

[[nodiscard]] std::optional<bool> Effects::OnMagicTargetAdd(
    RE::MagicTarget* a_this,
    const RE::MagicTarget::AddTargetData* a_data
) {
    if ((a_this == nullptr) || (a_data == nullptr)) {
        return std::nullopt;
    }

    auto* spell = a_data->magicItem;
    const auto* effect = a_data->effect;
    if ((spell == nullptr) || (effect == nullptr) || (effect->baseEffect == nullptr)) {
        return std::nullopt;
    }

    const auto* settings = Settings::GetSingleton();
    if ((settings == nullptr) || (!settings->blockDiseases && !settings->blockCloaks)) {
        return std::nullopt;
    }

    auto* target = a_this->GetTargetStatsObject();
    auto* targetActor = (target != nullptr) ? target->As<RE::Actor>() : nullptr;
    if (targetActor == nullptr) {
        return std::nullopt;
    }

    auto* casterRef = a_data->caster;
    const auto blockType = ClassifyBlockableEffect(settings, spell, effect->baseEffect, casterRef);
    if (blockType == EffectBlockType::kNone) {
        return std::nullopt;
    }

    const Mechanics::Feature feature = blockType == EffectBlockType::kDisease ? Mechanics::Feature::kDisease
                                                                              : Mechanics::Feature::kCloak;

    if (!Mechanics::ShouldApply(targetActor, casterRef, feature)) {
        return std::nullopt;
    }

    auto* casterActor = ResolveActor(casterRef);

    if (blockType == EffectBlockType::kCloak
        && IsDamagingEffect(effect->baseEffect)
        && AreEffectConditionsMet( // NOLINT(readability-suspicious-call-argument) targetActor=Subject, caster=Target in condition terms
            effect,
            targetActor,
            casterActor
        )) {
        const float wardDamage = effect->GetMagnitude() * settings->cloakDamageMultiplier;
        Mechanics::DamageWardPower(targetActor, wardDamage);

        SKSE::log::debug(
            "Blocked cloak | target={} | caster={} | spell={} <{:08X}> | wardDamage={:.1f}",
            targetActor->GetName(),
            (casterActor != nullptr) ? casterActor->GetName() : "none",
            spell->GetName(),
            spell->GetFormID(),
            wardDamage
        );
    } else if (blockType == EffectBlockType::kDisease) {
        SKSE::log::debug(
            "Blocked disease | target={} | caster={} | spell={} <{:08X}>",
            targetActor->GetName(),
            (casterActor != nullptr) ? casterActor->GetName() : "none",
            spell->GetName(),
            spell->GetFormID()
        );
    }

    return false;
}
