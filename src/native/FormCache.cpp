#include <RE/Skyrim.h> // IWYU pragma: keep

#include "FormCache.h"
#include "Settings.h"
#include "SkyrimUtil.h"
#include <RE/A/Actor.h>
#include <RE/A/ActorValues.h>
#include <RE/B/BGSKeyword.h>
#include <RE/B/BGSPerk.h>
#include <RE/B/BSCoreTypes.h>
#include <RE/B/BSTArray.h>
#include <RE/E/Effect.h>
#include <RE/E/EffectSetting.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicSystem.h>
#include <RE/N/NiSmartPointer.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESShout.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using namespace std::literals;

using FormIDSet = std::unordered_set<RE::FormID>;

// MSVC's unordered_set move constructor allocates and can throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct OffensiveShoutSpells {
    FormIDSet spellIDs;
    std::vector<std::string> names;
};

template <class Range>
[[nodiscard]] bool HasAnyKeyword(const RE::EffectSetting* a_effect, const Range& a_keywords) {
    if (!a_effect) {
        return false;
    }

    return std::ranges::any_of(a_keywords, [a_effect](const std::string_view a_keyword) {
        return a_effect->HasKeywordString(a_keyword);
    });
}

[[nodiscard]] bool HasTargetedDelivery(const RE::MagicSystem::Delivery a_delivery) {
    switch (a_delivery) {
        case RE::MagicSystem::Delivery::kAimed:
        case RE::MagicSystem::Delivery::kTargetActor:
        case RE::MagicSystem::Delivery::kTargetLocation: return true;
        default:                                         return false;
    }
}

[[nodiscard]] bool TargetsVitalActorValue(const RE::ActorValue a_av) {
    switch (a_av) {
        case RE::ActorValue::kHealth:
        case RE::ActorValue::kMagicka:
        case RE::ActorValue::kStamina: return true;
        default:                       return false;
    }
}

[[nodiscard]] bool IsLikelyOffensiveEffect(const RE::Effect* a_effect) {
    if (a_effect == nullptr) {
        return false;
    }

    const auto* baseEffect = a_effect->baseEffect;
    if (baseEffect == nullptr) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    const bool hasDamageKeyword = HasAnyKeyword(baseEffect, settings->damageKeywords);
    const bool isHostile = baseEffect->IsHostile() || baseEffect->IsDetrimental();
    const bool targetsVitals = TargetsVitalActorValue(baseEffect->data.primaryAV);
    const bool projectileOrExplosion = (baseEffect->data.projectileBase != nullptr)
                                       || (baseEffect->data.explosion != nullptr);
    const bool meaningfulMagnitude = a_effect->GetMagnitude() > 0.F || a_effect->GetDuration() > 0;

    if (!isHostile && !hasDamageKeyword) {
        return false;
    }

    if (!targetsVitals && !hasDamageKeyword && !projectileOrExplosion) {
        return false;
    }

    if (!meaningfulMagnitude && !projectileOrExplosion) {
        return false;
    }

    return true;
}

[[nodiscard]] bool IsLikelyOffensiveShoutSpell(
    const RE::SpellItem* a_spell,
    const FormIDSet& a_shoutSpells,
    const FormIDSet& a_excludedShoutSpells
) {
    if ((a_spell == nullptr) || a_spell->GetSpellType() != RE::MagicSystem::SpellType::kVoicePower) {
        return false;
    }

    const auto spellID = a_spell->GetFormID();

    if (!a_shoutSpells.empty() && !a_shoutSpells.contains(spellID)) {
        return false;
    }

    if (!a_excludedShoutSpells.empty() && a_excludedShoutSpells.contains(spellID)) {
        return false;
    }

    if (!HasTargetedDelivery(a_spell->GetDelivery())) {
        return false;
    }

    return std::ranges::any_of(a_spell->effects, [](const RE::Effect* a_effect) {
        return IsLikelyOffensiveEffect(a_effect);
    });
}

[[nodiscard]] FormIDSet CollectShoutSpells(const RE::BSTArray<RE::TESShout*>& a_shouts) {
    FormIDSet result;
    result.reserve(static_cast<std::size_t>(a_shouts.size()) * 3ULL);

    for (const auto* shout : a_shouts) {
        if (shout == nullptr) {
            continue;
        }

        for (const auto& variation : shout->variations) {
            if (variation.spell != nullptr) {
                result.insert(variation.spell->GetFormID());
            }
        }
    }

    return result;
}

[[nodiscard]] FormIDSet CollectExcludedShoutSpells(RE::TESDataHandler& a_data, const Settings& a_settings) {
    FormIDSet excluded;
    excluded.reserve(a_settings.shoutExclusions.size());

    for (const auto& [plugin, formID] : a_settings.shoutExclusions) {
        const auto* shout = a_data.LookupForm<RE::TESShout>(formID, plugin);
        if (shout == nullptr) {
            SKSE::log::warn("Shout Filter: failed to resolve excluded shout 0x{:06X}~{}", formID, plugin);
            continue;
        }

        SKSE::log::info(
            "Shout Filter: excluding shout 0x{:06X}~{} -> {} <{:08X}>",
            formID,
            plugin,
            shout->GetName(),
            shout->GetFormID()
        );

        std::uint32_t wordIndex = 1;
        for (const auto& variation : shout->variations) {
            if (variation.spell != nullptr) {
                excluded.insert(variation.spell->GetFormID());
                SKSE::log::debug(
                    "  - word {} spell: {} <{:08X}>",
                    wordIndex,
                    variation.spell->GetName(),
                    variation.spell->GetFormID()
                );
            }
            ++wordIndex;
        }
    }

    return excluded;
}

[[nodiscard]] OffensiveShoutSpells CollectOffensiveShoutSpells(
    const RE::BSTArray<RE::SpellItem*>& a_spells,
    const FormIDSet& a_shoutSpells,
    const FormIDSet& a_excludedShoutSpells
) {
    OffensiveShoutSpells cache;
    cache.spellIDs.reserve(a_spells.size());
    cache.names.reserve(a_spells.size());

    const auto count = a_spells.size();
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* spell = a_spells[i];
        if (!IsLikelyOffensiveShoutSpell(spell, a_shoutSpells, a_excludedShoutSpells)) {
            continue;
        }

        cache.spellIDs.insert(spell->GetFormID());
        cache.names.emplace_back(std::format("{} <{:08X}>", spell->GetName(), spell->GetFormID()));
    }

    return cache;
}

std::string JoinSpellNames(const std::vector<std::string>& a_names) {
    if (a_names.empty()) {
        return {};
    }

    std::string joined;
    joined.reserve(a_names.size() * 32);

    for (std::size_t i = 0; i < a_names.size(); ++i) {
        if (i > 0) {
            joined.append(", ");
        }
        joined.append(a_names[i]);
    }

    return joined;
}

[[nodiscard]] FormIDSet ResolveSpellList(
    RE::TESDataHandler& a_data,
    const std::vector<std::pair<std::string, std::uint32_t>>& a_entries,
    std::string_view a_filterName
) {
    FormIDSet result;
    result.reserve(a_entries.size());

    for (auto&& [plugin, formID] : a_entries) {
        if (const auto* spell = a_data.LookupForm<RE::MagicItem>(formID, plugin)) {
            result.insert(spell->GetFormID());
            SKSE::log::info(
                "{}: resolved 0x{:06X}~{} -> {} <{:08X}>",
                a_filterName,
                formID,
                plugin,
                spell->GetName(),
                spell->GetFormID()
            );
        } else {
            SKSE::log::warn("{}: failed to resolve 0x{:06X}~{}", a_filterName, formID, plugin);
        }
    }

    return result;
}

[[nodiscard]] std::vector<RE::BGSPerk*> ResolvePerkList(
    RE::TESDataHandler& a_data,
    const std::vector<std::pair<std::string, std::uint32_t>>& a_entries,
    std::string_view a_filterName
) {
    std::vector<RE::BGSPerk*> result;
    result.reserve(a_entries.size());

    for (auto&& [plugin, formID] : a_entries) {
        if (auto* perk = a_data.LookupForm<RE::BGSPerk>(formID, plugin)) {
            result.push_back(perk);
            SKSE::log::info(
                "{}: resolved 0x{:06X}~{} -> {} <{:08X}>",
                a_filterName,
                formID,
                plugin,
                perk->GetName(),
                perk->GetFormID()
            );
        } else {
            SKSE::log::warn("{}: failed to resolve 0x{:06X}~{}", a_filterName, formID, plugin);
        }
    }

    return result;
}
}

void FormCache::Initialize(RE::TESDataHandler& a_data, const Settings& a_settings) {
    BuildShoutCaches(a_data, a_settings);
    BuildEffectCaches(a_data, a_settings);
    BuildExcludedItems(a_data, a_settings);
    BuildReflectionCaches(a_data, a_settings);

    wardKeyword_ = a_data.LookupForm<RE::BGSKeyword>(0x1EA69, "Skyrim.esm");
    if (wardKeyword_ == nullptr) {
        SKSE::log::warn("FormCache: failed to resolve MagicWard keyword");
    }
}

void FormCache::BuildShoutCaches(RE::TESDataHandler& a_data, const Settings& a_settings) {
    const auto& shoutArray = a_data.GetFormArray<RE::TESShout>();
    shoutSpells_ = CollectShoutSpells(shoutArray);

    SKSE::log::info("Shout Filter: indexed {} spell(s) from {} shout(s)", shoutSpells_.size(), shoutArray.size());

    if (!a_settings.shoutExclusions.empty()) {
        excludedShoutSpells_ = CollectExcludedShoutSpells(a_data, a_settings);
        SKSE::log::info(
            "Shout Filter: excluded {} spell(s) from {} shout(s)",
            excludedShoutSpells_.size(),
            a_settings.shoutExclusions.size()
        );
    }

    auto [spellIDs, names] = CollectOffensiveShoutSpells(
        a_data.GetFormArray<RE::SpellItem>(),
        shoutSpells_,
        excludedShoutSpells_
    );
    offensiveShoutSpells_ = std::move(spellIDs);

    if (names.empty()) {
        SKSE::log::warn("Shout Filter: no offensive voice spells detected. Falling back to all voice spells.");
        return;
    }

    SKSE::log::info("Shout Filter: {} offensive voice spell(s) enabled: {}", names.size(), JoinSpellNames(names));
}

void FormCache::BuildEffectCaches(RE::TESDataHandler& a_data, const Settings& a_settings) {
    diseaseSpellIDs_ = ResolveSpellList(a_data, a_settings.diseaseSpells, "Disease Filter");
    cloakSpellIDs_ = ResolveSpellList(a_data, a_settings.cloakSpells, "Cloak Filter");

    SKSE::log::info(
        "Effect Filters: {} disease spell(s), {} cloak spell(s) in whitelist",
        diseaseSpellIDs_.size(),
        cloakSpellIDs_.size()
    );
}

void FormCache::BuildExcludedItems(RE::TESDataHandler& a_data, const Settings& a_settings) {
    excludedItems_.clear();
    excludedItems_.reserve(a_settings.excludedItems.size());

    for (auto&& [plugin, formID] : a_settings.excludedItems) {
        auto* form = a_data.LookupForm(formID, plugin);
        if (form == nullptr) {
            SKSE::log::warn("Exclusions: failed to resolve 0x{:06X}~{}", formID, plugin);
            continue;
        }

        auto* obj = form->As<RE::TESBoundObject>();
        if (obj == nullptr) {
            SKSE::log::warn(
                "Exclusions: 0x{:06X}~{} is not a TESBoundObject (type={})",
                formID,
                plugin,
                static_cast<std::uint32_t>(form->GetFormType())
            );
            continue;
        }

        excludedItems_.push_back(obj);
        SKSE::log::info(
            "Exclusions: resolved 0x{:06X}~{} -> {} <{:08X}>",
            formID,
            plugin,
            obj->GetName(),
            obj->GetFormID()
        );
    }
}

bool FormCache::IsOffensiveShoutSpell(const RE::FormID a_id) const {
    if (offensiveShoutSpells_.empty()) {
        return true;
    }
    return offensiveShoutSpells_.contains(a_id);
}

bool FormCache::IsShoutSpell(const RE::FormID a_id) const {
    return shoutSpells_.contains(a_id);
}

bool FormCache::IsExcludedShoutSpell(const RE::FormID a_id) const {
    return excludedShoutSpells_.contains(a_id);
}

bool FormCache::IsDiseaseSpell(const RE::FormID a_id) const {
    return diseaseSpellIDs_.contains(a_id);
}

bool FormCache::IsCloakSpell(const RE::FormID a_id) const {
    return cloakSpellIDs_.contains(a_id);
}

bool FormCache::IsExcludedItemEquipped(const RE::Actor* a_actor) const {
    if (a_actor == nullptr) {
        return false;
    }

    if (excludedItems_.empty()) {
        return false;
    }

    auto* left = a_actor->GetEquippedObject(true);
    auto* right = a_actor->GetEquippedObject(false);

    return std::ranges::contains(excludedItems_, left) || std::ranges::contains(excludedItems_, right);
}

RE::NiPointer<RE::TESObjectREFR> FormCache::GetOrCreateSpellCaster(
    RE::Actor* a_defender,
    RE::TESBoundObject* a_activator
) {
    if ((a_defender == nullptr) || (a_activator == nullptr)) {
        return {};
    }

    const auto defenderHandle = a_defender->GetHandle();

    std::unique_lock lock(spellCastersMutex_);

    PruneSpellCastersLocked();

    if (const auto found = spellCasters_.find(defenderHandle); found != spellCasters_.end()) {
        if (found->second) {
            return found->second;
        }
    }

    lock.unlock();

    auto placedCaster = a_defender->PlaceObjectAtMe(a_activator, false);
    auto* casterRef = placedCaster.get();
    if (casterRef == nullptr) {
        return {};
    }

    casterRef->SetTemporary();

    lock.lock();
    auto& entry = spellCasters_[defenderHandle];
    if (!entry) {
        entry = placedCaster;
    } else {
        placedCaster = entry;
    }

    return placedCaster;
}

void FormCache::PruneSpellCastersLocked() {
    // Active spells can still own these temporary casters. Release our references
    // and let the engine destroy them when the remaining owners finish.
    for (auto it = spellCasters_.begin(); it != spellCasters_.end();) {
        if (!it->first || !it->second) {
            it = spellCasters_.erase(it);
            continue;
        }

        if (const auto actorPtr = it->first.get(); !actorPtr) {
            it = spellCasters_.erase(it);
            continue;
        }

        ++it;
    }
}

void FormCache::ClearSpellCasters() {
    std::unique_lock const lock(spellCastersMutex_);
    spellCasters_.clear();
}

void FormCache::BuildReflectionCaches(RE::TESDataHandler& a_data, const Settings& a_settings) {
    for (auto&& [plugin, formID] : a_settings.reflectionExclusions) {
        if (const auto* spell = a_data.LookupForm<RE::MagicItem>(formID, plugin)) {
            reflectionExcludedSpells_.insert(spell->GetFormID());
            SKSE::log::info(
                "Reflection Exclusions: resolved 0x{:06X}~{} -> {} <{:08X}>",
                formID,
                plugin,
                spell->GetName(),
                spell->GetFormID()
            );
        } else {
            SKSE::log::warn("Reflection Exclusions: failed to resolve 0x{:06X}~{}", formID, plugin);
        }
    }

    reflectionRequiredPerks_ = ResolvePerkList(a_data, a_settings.reflectionRequiredPerks, "Reflection Perks");
    physicalRequiredPerks_ = ResolvePerkList(a_data, a_settings.physicalRequiredPerks, "Physical Perks");
    shoutsRequiredPerks_ = ResolvePerkList(a_data, a_settings.shoutsRequiredPerks, "Shout Perks");
    diseaseRequiredPerks_ = ResolvePerkList(a_data, a_settings.diseaseRequiredPerks, "Disease Perks");
    cloakRequiredPerks_ = ResolvePerkList(a_data, a_settings.cloakRequiredPerks, "Cloak Perks");
}

bool FormCache::IsReflectionExcluded(const RE::FormID a_id) const {
    return reflectionExcludedSpells_.contains(a_id);
}

bool FormCache::HasReflectionPerks(const RE::Actor* a_actor) const {
    return stl::HasAllRequiredPerks(a_actor, reflectionRequiredPerks_);
}

bool FormCache::HasPhysicalPerks(const RE::Actor* a_actor) const {
    return stl::HasAllRequiredPerks(a_actor, physicalRequiredPerks_);
}

bool FormCache::HasShoutPerks(const RE::Actor* a_actor) const {
    return stl::HasAllRequiredPerks(a_actor, shoutsRequiredPerks_);
}

bool FormCache::HasDiseasePerks(const RE::Actor* a_actor) const {
    return stl::HasAllRequiredPerks(a_actor, diseaseRequiredPerks_);
}

bool FormCache::HasCloakPerks(const RE::Actor* a_actor) const {
    return stl::HasAllRequiredPerks(a_actor, cloakRequiredPerks_);
}

bool FormCache::IsReflectableSpell(RE::MagicItem* a_spell) const {
    if (a_spell == nullptr) {
        return false;
    }

    if (a_spell->GetSpellType() == RE::MagicSystem::SpellType::kVoicePower) {
        return false;
    }

    if (a_spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
        return false;
    }

    if (a_spell->GetNoAbsorb()) {
        return false;
    }

    if (IsReflectionExcluded(a_spell->GetFormID())) {
        return false;
    }

    return std::ranges::any_of(a_spell->effects, [](const RE::Effect* a_effect) {
        const auto* base = a_effect ? a_effect->baseEffect : nullptr;
        if (!base) {
            return false;
        }

        if (base->HasKeywordString("MagicRune")) {
            return false;
        }

        const auto* projectile = base->data.projectileBase;
        return projectile && !projectile->IsFlamethrower() && !projectile->IsBeam();
    });
}
