#pragma once

#include <REX/REX/Singleton.h>

#include <shared_mutex>
#include <unordered_set>

class Settings;

struct HandleHash {
    std::size_t operator()(const RE::ActorHandle& a_handle) const noexcept {
        return a_handle.native_handle();
    }
};

class FormCache : public REX::Singleton<FormCache> {
public:
    void Initialize(RE::TESDataHandler& a_data, const Settings& a_settings);

    [[nodiscard]] bool IsOffensiveShoutSpell(RE::FormID a_id) const;
    [[nodiscard]] bool IsShoutSpell(RE::FormID a_id) const;
    [[nodiscard]] bool IsExcludedShoutSpell(RE::FormID a_id) const;

    [[nodiscard]] bool IsDiseaseSpell(RE::FormID a_id) const;
    [[nodiscard]] bool IsCloakSpell(RE::FormID a_id) const;

    [[nodiscard]] bool IsExcludedItemEquipped(const RE::Actor* a_actor) const;

    [[nodiscard]] RE::BGSKeyword* GetWardKeyword() const {
        return wardKeyword_;
    }

    [[nodiscard]] RE::NiPointer<RE::TESObjectREFR> GetOrCreateSpellCaster(
        RE::Actor* a_defender,
        RE::TESBoundObject* a_activator
    );

    [[nodiscard]] bool IsReflectionExcluded(RE::FormID a_id) const;
    [[nodiscard]] bool HasReflectionPerks(const RE::Actor* a_actor) const;
    [[nodiscard]] bool HasPhysicalPerks(const RE::Actor* a_actor) const;
    [[nodiscard]] bool HasShoutPerks(const RE::Actor* a_actor) const;
    [[nodiscard]] bool HasDiseasePerks(const RE::Actor* a_actor) const;
    [[nodiscard]] bool HasCloakPerks(const RE::Actor* a_actor) const;
    [[nodiscard]] bool IsReflectableSpell(RE::MagicItem* a_spell) const;

private:
    using FormIDSet = std::unordered_set<RE::FormID>;

    FormIDSet offensiveShoutSpells_;
    FormIDSet shoutSpells_;
    FormIDSet excludedShoutSpells_;
    FormIDSet diseaseSpellIDs_;
    FormIDSet cloakSpellIDs_;
    FormIDSet reflectionExcludedSpells_;
    std::vector<RE::TESBoundObject*> excludedItems_;
    std::vector<RE::BGSPerk*> reflectionRequiredPerks_;
    std::vector<RE::BGSPerk*> physicalRequiredPerks_;
    std::vector<RE::BGSPerk*> shoutsRequiredPerks_;
    std::vector<RE::BGSPerk*> diseaseRequiredPerks_;
    std::vector<RE::BGSPerk*> cloakRequiredPerks_;
    RE::BGSKeyword* wardKeyword_ {nullptr};

    std::unordered_map<RE::ActorHandle, RE::NiPointer<RE::TESObjectREFR>, HandleHash> spellCasters_;
    mutable std::shared_mutex mutex_;

    void BuildShoutCaches(RE::TESDataHandler& a_data, const Settings& a_settings);
    void BuildEffectCaches(RE::TESDataHandler& a_data, const Settings& a_settings);
    void BuildExcludedItems(RE::TESDataHandler& a_data, const Settings& a_settings);
    void BuildReflectionCaches(RE::TESDataHandler& a_data, const Settings& a_settings);
    void PruneSpellCasters();
    void PruneSpellCastersLocked();
};
