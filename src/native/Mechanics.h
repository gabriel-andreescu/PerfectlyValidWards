#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

namespace Mechanics {

enum class Feature : std::uint8_t {
    kPhysical,
    kShout,
    kDisease,
    kCloak,
    kReflection,
};

[[nodiscard]] bool ShouldApply(RE::Actor* a_defender, const RE::TESObjectREFR* a_attacker, Feature a_feature);

[[nodiscard]] bool IsFeatureDisabledByExclusion(const RE::Actor* a_wardCaster, Feature a_feature);

[[nodiscard]] bool HasRequiredPerks(const RE::Actor* a_actor, Feature a_feature);

[[nodiscard]] const char* FeatureName(Feature a_feature);

[[nodiscard]] float GetCurrentWardPower(RE::Actor* a_actor);

void DamageWardPower(RE::Actor* a_actor, float a_damage);

void ApplyStagger(RE::Actor* a_actor, const RE::NiPoint3& a_sourcePosition, float a_magnitude);

[[nodiscard]] bool IsDiseaseSpell(RE::MagicItem* a_spell);

[[nodiscard]] bool IsCloakSpell(const RE::MagicItem* a_spell, const RE::EffectSetting* a_effect);

[[nodiscard]] bool IsCloakDamageSpell(const RE::MagicItem* a_spell, RE::Actor* a_caster);

[[nodiscard]] bool IsWardSpell(const RE::SpellItem* a_spell);

[[nodiscard]] inline float GetGameSettingFloat(const char* a_name, const float a_fallback = 1.F) {
    if (auto* settings = RE::GameSettingCollection::GetSingleton()) {
        if (const auto* set = settings->GetSetting(a_name); set) {
            return set->GetFloat();
        }
    }
    return a_fallback;
}

}
