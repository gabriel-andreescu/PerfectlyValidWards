#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

#include <REX/REX/Singleton.h>
#include <memory>

#include <mutex>
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
    void ClearSpellCasters();

    [[nodiscard]] bool IsOffensiveShoutSpell(RE::FormID a_id) const;
    [[nodiscard]] bool IsShoutSpell(RE::FormID a_id) const;
    [[nodiscard]] bool IsExcludedShoutSpell(RE::FormID a_id) const;

    [[nodiscard]] bool IsDiseaseSpell(RE::FormID a_id) const;
    [[nodiscard]] bool IsCloakSpell(RE::FormID a_id) const;

    [[nodiscard]] bool IsExcludedItemEquipped(const RE::Actor* a_actor) const;

    [[nodiscard]] RE::BGSKeyword* GetWardKeyword() const {
        return _wardKeyword;
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

    // Built before hook installation and read-only afterward.
    FormIDSet _offensiveShoutSpells;
    FormIDSet _shoutSpells;
    FormIDSet _excludedShoutSpells;
    FormIDSet _diseaseSpellIDs;
    FormIDSet _cloakSpellIDs;
    FormIDSet _reflectionExcludedSpells;
    std::vector<RE::TESBoundObject*> _excludedItems;
    std::vector<RE::BGSPerk*> _reflectionRequiredPerks;
    std::vector<RE::BGSPerk*> _physicalRequiredPerks;
    std::vector<RE::BGSPerk*> _shoutsRequiredPerks;
    std::vector<RE::BGSPerk*> _diseaseRequiredPerks;
    std::vector<RE::BGSPerk*> _cloakRequiredPerks;
    RE::BGSKeyword* _wardKeyword {nullptr};

    std::unordered_map<RE::ActorHandle, RE::NiPointer<RE::TESObjectREFR>, HandleHash> _spellCasters;
    std::mutex _spellCastersMutex;

    void BuildShoutCaches(RE::TESDataHandler& a_data, const Settings& a_settings);
    void BuildEffectCaches(RE::TESDataHandler& a_data, const Settings& a_settings);
    void BuildExcludedItems(RE::TESDataHandler& a_data, const Settings& a_settings);
    void BuildReflectionCaches(RE::TESDataHandler& a_data, const Settings& a_settings);
    void PruneSpellCastersLocked();
};
