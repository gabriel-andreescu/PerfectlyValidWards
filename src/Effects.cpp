#include "Effects.h"
#include "Mechanics.h"
#include "Settings.h"

namespace {
enum class EffectBlockType {
    kNone,
    kDisease,
    kCloak
};

[[nodiscard]] RE::Actor* ResolveActor(RE::TESObjectREFR* a_ref) {
    return a_ref ? a_ref->As<RE::Actor>() : nullptr;
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
            casterActor && Mechanics::IsCloakDamageSpell(a_spell, casterActor)) {
            return EffectBlockType::kCloak;
        }
    }

    return EffectBlockType::kNone;
}

[[nodiscard]] bool IsDamagingEffect(const RE::EffectSetting* a_effectSetting) {
    if (!a_effectSetting || !a_effectSetting->IsHostile()) {
        return false;
    }

    const Settings* settings = Settings::GetSingleton();
    if (!settings) {
        return false;
    }

    return std::ranges::any_of(settings->damageKeywords, [a_effectSetting](const std::string& a_keyword) {
        return a_effectSetting->HasKeywordString(a_keyword);
    });
}

[[nodiscard]] bool AreEffectConditionsMet(const RE::Effect* a_effect, RE::Actor* a_subject, RE::Actor* a_target) {
    if (!a_effect || !a_effect->conditions.head) {
        return true;
    }
    if (!a_subject) {
        return false;
    }
    return a_effect->conditions.IsTrue(a_subject, a_target);
}
}

[[nodiscard]] std::optional<bool> Effects::OnMagicTargetAdd(
    RE::MagicTarget* a_this,
    const RE::MagicTarget::AddTargetData* a_data
) {
    if (!a_this || !a_data) {
        return std::nullopt;
    }

    auto* spell = a_data->magicItem;
    const auto* effect = a_data->effect;
    if (!spell || !effect || !effect->baseEffect) {
        return std::nullopt;
    }

    const auto* settings = Settings::GetSingleton();
    if (!settings || (!settings->blockDiseases && !settings->blockCloaks)) {
        return std::nullopt;
    }

    auto* target = a_this->GetTargetStatsObject();
    auto* targetActor = target ? target->As<RE::Actor>() : nullptr;
    if (!targetActor) {
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

        logger::debug(
            "Blocked cloak | target={} | caster={} | spell={} <{:08X}> | wardDamage={:.1f}",
            targetActor->GetName(),
            casterActor ? casterActor->GetName() : "none",
            spell->GetName(),
            spell->GetFormID(),
            wardDamage
        );
    } else if (blockType == EffectBlockType::kDisease) {
        logger::debug(
            "Blocked disease | target={} | caster={} | spell={} <{:08X}>",
            targetActor->GetName(),
            casterActor ? casterActor->GetName() : "none",
            spell->GetName(),
            spell->GetFormID()
        );
    }

    return false;
}
