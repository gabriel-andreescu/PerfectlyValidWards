#include <RE/Skyrim.h> // IWYU pragma: keep

#include <RE/B/BGSCollisionLayer.h>
#include <RE/B/BGSImpactData.h>
#include <RE/B/BGSImpactDataSet.h>
#include <RE/B/BGSMaterialType.h>
#include <RE/B/BSTArray.h>

#include "Patches.h"
#include "Settings.h"
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESTopicInfo.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <array>

namespace Patches {
using namespace IDs;

namespace {

    struct CollisionPatch {
        RE::BGSCollisionLayer* layer = nullptr;
        RE::BGSCollisionLayer* other = nullptr;
        bool inserted = false;

        void Apply(bool a_enabled) {
            auto& links = layer->collidesWith;
            const auto* const found = std::ranges::find(links, other);
            if (a_enabled && found == links.end()) {
                links.push_back(other);
                std::ranges::sort(links, {}, &RE::BGSCollisionLayer::collisionIdx);
                inserted = true;
            } else if (!a_enabled && inserted) {
                if (found != links.end()) {
                    links.erase(found);
                }
                inserted = false;
            }
        }
    };

    void CopyFirstConditionFrom(const RE::TESTopicInfo* a_sourceInfo, RE::TESTopicInfo* a_targetInfo) {
        if ((a_sourceInfo == nullptr) || (a_targetInfo == nullptr) || (a_sourceInfo->objConditions.head == nullptr)) {
            return;
        }

        auto*& targetHead = a_targetInfo->objConditions.head;
        if (targetHead == nullptr) {
            targetHead = a_sourceInfo->objConditions.head;
            return;
        }

        auto* current = targetHead;
        while (current->next != nullptr) {
            current = current->next;
        }
        current->next = a_sourceInfo->objConditions.head;
    }

}

void PatchCollisionLayers() {
    auto* const data = RE::TESDataHandler::GetSingleton();
    if (data == nullptr) {
        SKSE::log::warn("Patching collision layers: TESDataHandler unavailable");
        return;
    }

    auto* weapon = data->LookupForm<RE::BGSCollisionLayer>(kWeaponCol, Settings::kSkyrimESM);
    auto* projectile = data->LookupForm<RE::BGSCollisionLayer>(kProjectileCol, Settings::kSkyrimESM);
    auto* ward = data->LookupForm<RE::BGSCollisionLayer>(kWardCol, Settings::kSkyrimESM);

    if (weapon == nullptr || projectile == nullptr || ward == nullptr) {
        return;
    }

    // Loaded forms live for the process lifetime. Track only links added by this mod.
    static std::array meleeLinks {
        CollisionPatch {.layer = weapon, .other = ward},
        CollisionPatch {.layer = ward, .other = weapon},
    };
    static std::array arrowLinks {
        CollisionPatch {.layer = projectile, .other = ward},
        CollisionPatch {.layer = ward, .other = projectile},
    };
    const auto* settings = Settings::GetSingleton();
    for (auto& patch : meleeLinks) {
        patch.Apply(settings->blockMelee.load());
    }
    for (auto& patch : arrowLinks) {
        patch.Apply(settings->blockArrows.load());
    }
}

void PatchImpactDataSets() {
    auto* const data = RE::TESDataHandler::GetSingleton();
    if (data == nullptr) {
        SKSE::log::warn("Patching impact data sets: TESDataHandler unavailable");
        return;
    }

    auto* wardMaterial = data->LookupForm<RE::BGSMaterialType>(kWardMaterial, Settings::kSkyrimESM);
    auto* arrowImpactDataSet = data->LookupForm<RE::BGSImpactDataSet>(kArrowImpactSet, Settings::kSkyrimESM);
    auto* arrowVsWardImpact = data->LookupForm<RE::BGSImpactData>(kArrowVsWardImpact, Settings::kPluginName);

    if (wardMaterial == nullptr || arrowImpactDataSet == nullptr || arrowVsWardImpact == nullptr) {
        return;
    }
    static bool inserted = false;
    auto& impacts = arrowImpactDataSet->impactMap;
    if (Settings::GetSingleton()->blockArrows.load()) {
        const auto result = impacts.insert({wardMaterial, arrowVsWardImpact});
        inserted = inserted || result.second;
    } else if (inserted) {
        const auto found = impacts.find(wardMaterial);
        if (found != impacts.end() && found->second == arrowVsWardImpact) {
            impacts.erase(found);
        }
        inserted = false;
    }
}

void PatchHitGrunts() {
    auto* const data = RE::TESDataHandler::GetSingleton();
    if (data == nullptr) {
        SKSE::log::warn("Patching hit grunts: TESDataHandler unavailable");
        return;
    }

    const auto* gruntPlaceholder = data->LookupForm<RE::TESTopicInfo>(kGruntPlaceholder, Settings::kPluginName);
    for (const auto formID : kGrunts) {
        if (auto* grunt = data->LookupForm<RE::TESTopicInfo>(formID, Settings::kSkyrimESM)) {
            CopyFirstConditionFrom(gruntPlaceholder, grunt);
        }
    }
}

}
