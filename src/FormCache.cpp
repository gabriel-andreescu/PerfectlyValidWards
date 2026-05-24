#include "FormCache.h"
#include "Settings.h"

namespace {
using namespace std::literals;

using FormIDSet = std::unordered_set<RE::FormID>;

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
    if (!a_effect) {
        return false;
    }

    const auto* baseEffect = a_effect->baseEffect;
    if (!baseEffect) {
        return false;
    }

    const auto* settings = Settings::GetSingleton();
    const bool hasDamageKeyword = settings ? HasAnyKeyword(baseEffect, settings->damageKeywords) : false;
    const bool isHostile = baseEffect->IsHostile() || baseEffect->IsDetrimental();
    const bool targetsVitals = TargetsVitalActorValue(baseEffect->data.primaryAV);
    const bool projectileOrExplosion = baseEffect->data.projectileBase || baseEffect->data.explosion;
    const bool meaningfulMagnitude = a_effect->GetMagnitude() > 0.f || a_effect->GetDuration() > 0;

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
    if (!a_spell || a_spell->GetSpellType() != RE::MagicSystem::SpellType::kVoicePower) {
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
    result.reserve(static_cast<std::size_t>(a_shouts.size()) * 3ull);

    for (const auto* shout : a_shouts) {
        if (!shout) {
            continue;
        }

        for (const auto& variation : shout->variations) {
            if (variation.spell) {
                result.insert(variation.spell->GetFormID());
            }
        }
    }

    return result;
}

[[nodiscard]] FormIDSet CollectExcludedShoutSpells(RE::TESDataHandler& a_data, const Settings& a_settings) {
    FormIDSet excluded;
    excluded.reserve(a_settings.shoutExclusions.size());

    for (const auto& [plugin, id] : a_settings.shoutExclusions) {
        auto* shout = a_data.LookupForm<RE::TESShout>(id, plugin);
        if (!shout) {
            logger::warn("Shout Filter: failed to resolve excluded shout {}|0x{:06X}", plugin, id);
            continue;
        }

        logger::info(
            "Shout Filter: excluding shout {}|0x{:06X} -> {} <{:08X}>",
            plugin,
            id,
            shout->GetName(),
            shout->GetFormID()
        );

        std::uint32_t wordIndex = 1;
        for (const auto& variation : shout->variations) {
            if (variation.spell) {
                excluded.insert(variation.spell->GetFormID());
                logger::debug(
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

    for (auto&& [plugin, id] : a_entries) {
        if (const auto* spell = a_data.LookupForm<RE::MagicItem>(id, plugin)) {
            result.insert(spell->GetFormID());
            logger::info(
                "{}: resolved {}|0x{:06X} -> {} <{:08X}>",
                a_filterName,
                plugin,
                id,
                spell->GetName(),
                spell->GetFormID()
            );
        } else {
            logger::warn("{}: failed to resolve {}|0x{:06X}", a_filterName, plugin, id);
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

    for (auto&& [plugin, id] : a_entries) {
        if (auto* perk = a_data.LookupForm<RE::BGSPerk>(id, plugin)) {
            result.push_back(perk);
            logger::info(
                "{}: resolved {}|0x{:06X} -> {} <{:08X}>",
                a_filterName,
                plugin,
                id,
                perk->GetName(),
                perk->GetFormID()
            );
        } else {
            logger::warn("{}: failed to resolve {}|0x{:06X}", a_filterName, plugin, id);
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
    if (!wardKeyword_) {
        logger::warn("FormCache: failed to resolve MagicWard keyword");
    }
}

void FormCache::BuildShoutCaches(RE::TESDataHandler& a_data, const Settings& a_settings) {
    const auto& shoutArray = a_data.GetFormArray<RE::TESShout>();
    shoutSpells_ = CollectShoutSpells(shoutArray);

    logger::info("Shout Filter: indexed {} spell(s) from {} shout(s)", shoutSpells_.size(), shoutArray.size());

    if (!a_settings.shoutExclusions.empty()) {
        excludedShoutSpells_ = CollectExcludedShoutSpells(a_data, a_settings);
        logger::info(
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
        logger::warn("Shout Filter: no offensive voice spells detected; shout-specific ward mechanics disabled");
        return;
    }

    logger::info("Shout Filter: {} offensive voice spell(s) enabled: {}", names.size(), JoinSpellNames(names));
}

void FormCache::BuildEffectCaches(RE::TESDataHandler& a_data, const Settings& a_settings) {
    diseaseSpellIDs_ = ResolveSpellList(a_data, a_settings.diseaseSpells, "Disease Filter");
    cloakSpellIDs_ = ResolveSpellList(a_data, a_settings.cloakSpells, "Cloak Filter");

    logger::info(
        "Effect Filters: {} disease spell(s), {} cloak spell(s) in whitelist",
        diseaseSpellIDs_.size(),
        cloakSpellIDs_.size()
    );
}

void FormCache::BuildExcludedItems(RE::TESDataHandler& a_data, const Settings& a_settings) {
    excludedItems_.clear();
    excludedItems_.reserve(a_settings.excludedItems.size());

    for (auto&& [plugin, id] : a_settings.excludedItems) {
        auto* form = a_data.LookupForm(id, plugin);
        if (!form) {
            logger::warn("Exclusions: failed to resolve {}|0x{:06X}", plugin, id);
            continue;
        }

        auto* obj = form->As<RE::TESBoundObject>();
        if (!obj) {
            logger::warn(
                "Exclusions: {}|0x{:06X} is not a TESBoundObject (type={})",
                plugin,
                id,
                static_cast<std::uint32_t>(form->GetFormType())
            );
            continue;
        }

        excludedItems_.push_back(obj);
        logger::info("Exclusions: resolved {}|0x{:06X} -> {} <{:08X}>", plugin, id, obj->GetName(), obj->GetFormID());
    }
}

bool FormCache::IsOffensiveShoutSpell(const RE::FormID a_id) const {
    std::shared_lock lock(mutex_);
    if (offensiveShoutSpells_.empty()) {
        return true;
    }
    return offensiveShoutSpells_.contains(a_id);
}

bool FormCache::IsShoutSpell(const RE::FormID a_id) const {
    std::shared_lock lock(mutex_);
    return shoutSpells_.contains(a_id);
}

bool FormCache::IsExcludedShoutSpell(const RE::FormID a_id) const {
    std::shared_lock lock(mutex_);
    return excludedShoutSpells_.contains(a_id);
}

bool FormCache::IsDiseaseSpell(const RE::FormID a_id) const {
    std::shared_lock lock(mutex_);
    return diseaseSpellIDs_.contains(a_id);
}

bool FormCache::IsCloakSpell(const RE::FormID a_id) const {
    std::shared_lock lock(mutex_);
    return cloakSpellIDs_.contains(a_id);
}

bool FormCache::IsExcludedItemEquipped(const RE::Actor* a_actor) const {
    if (!a_actor) {
        return false;
    }

    std::shared_lock lock(mutex_);
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
    if (!a_defender || !a_activator) {
        return {};
    }

    const auto defenderHandle = a_defender->GetHandle();

    std::unique_lock lock(mutex_);

    PruneSpellCastersLocked();

    if (const auto it = spellCasters_.find(defenderHandle); it != spellCasters_.end()) {
        if (it->second) {
            return it->second;
        }
    }

    lock.unlock();

    auto placedCaster = a_defender->PlaceObjectAtMe(a_activator, false);
    auto* casterRef = placedCaster.get();
    if (!casterRef) {
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

void FormCache::PruneSpellCasters() {
    std::unique_lock lock(mutex_);
    PruneSpellCastersLocked();
}

void FormCache::PruneSpellCastersLocked() {
    // TODO: Consider explicit Disable()/DeleteThis() on orphaned refs.
    // Currently relying on SetTemporary() for engine cleanup.
    // Explicit deletion caused crashes when shouts still referenced their caster.
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

void FormCache::BuildReflectionCaches(RE::TESDataHandler& a_data, const Settings& a_settings) {
    for (auto&& [a_plugin, a_id] : a_settings.reflectionExclusions) {
        if (const auto* a_spell = a_data.LookupForm<RE::MagicItem>(a_id, a_plugin)) {
            reflectionExcludedSpells_.insert(a_spell->GetFormID());
            logger::info(
                "Reflection Exclusions: resolved {}|0x{:06X} -> {} <{:08X}>",
                a_plugin,
                a_id,
                a_spell->GetName(),
                a_spell->GetFormID()
            );
        } else {
            logger::warn("Reflection Exclusions: failed to resolve {}|0x{:06X}", a_plugin, a_id);
        }
    }

    reflectionRequiredPerks_ = ResolvePerkList(a_data, a_settings.reflectionRequiredPerks, "Reflection Perks");
    physicalRequiredPerks_ = ResolvePerkList(a_data, a_settings.physicalRequiredPerks, "Physical Perks");
    shoutsRequiredPerks_ = ResolvePerkList(a_data, a_settings.shoutsRequiredPerks, "Shout Perks");
    diseaseRequiredPerks_ = ResolvePerkList(a_data, a_settings.diseaseRequiredPerks, "Disease Perks");
    cloakRequiredPerks_ = ResolvePerkList(a_data, a_settings.cloakRequiredPerks, "Cloak Perks");
}

bool FormCache::IsReflectionExcluded(const RE::FormID a_id) const {
    std::shared_lock a_lock(mutex_);
    return reflectionExcludedSpells_.contains(a_id);
}

bool FormCache::HasReflectionPerks(const RE::Actor* a_actor) const {
    std::shared_lock a_lock(mutex_);
    return stl::has_all_required_perks(a_actor, reflectionRequiredPerks_);
}

bool FormCache::HasPhysicalPerks(const RE::Actor* a_actor) const {
    std::shared_lock a_lock(mutex_);
    return stl::has_all_required_perks(a_actor, physicalRequiredPerks_);
}

bool FormCache::HasShoutPerks(const RE::Actor* a_actor) const {
    std::shared_lock a_lock(mutex_);
    return stl::has_all_required_perks(a_actor, shoutsRequiredPerks_);
}

bool FormCache::HasDiseasePerks(const RE::Actor* a_actor) const {
    std::shared_lock a_lock(mutex_);
    return stl::has_all_required_perks(a_actor, diseaseRequiredPerks_);
}

bool FormCache::HasCloakPerks(const RE::Actor* a_actor) const {
    std::shared_lock a_lock(mutex_);
    return stl::has_all_required_perks(a_actor, cloakRequiredPerks_);
}

bool FormCache::IsReflectableSpell(RE::MagicItem* a_spell) const {
    if (!a_spell) {
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
        const auto* a_base = a_effect ? a_effect->baseEffect : nullptr;
        if (!a_base) {
            return false;
        }

        if (a_base->HasKeywordString("MagicRune")) {
            return false;
        }

        const auto* a_proj = a_base->data.projectileBase;
        return a_proj && !a_proj->IsFlamethrower() && !a_proj->IsBeam();
    });
}
