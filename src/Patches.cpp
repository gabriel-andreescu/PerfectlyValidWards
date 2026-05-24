#include "Patches.h"
#include "Settings.h"

namespace Patches {
using namespace IDs;

namespace {

    template <class T, class Alloc>
    void SortByCollisionIdx(RE::BSTArray<T*, Alloc>& a_arr) {
        std::ranges::sort(a_arr, [](T* a_lhs, T* a_rhs) {
            return a_lhs->collisionIdx < a_rhs->collisionIdx;
        });
    }

    template <class Arr>
    void InsertUnique(Arr& a_arr, RE::BGSCollisionLayer* a_layer) {
        if (!std::ranges::contains(a_arr, a_layer)) {
            a_arr.push_back(a_layer);
        }
    }

    template <class Arr>
    void ReserveExtra(Arr& a_arr, std::size_t a_extra) {
        using size_type = Arr::size_type;
        const std::size_t desired = a_arr.size() + a_extra;

        static_assert(sizeof(size_type) == 4, "BSTArray::size_type changed?");
        assert(desired <= std::numeric_limits<size_type>::max());

        a_arr.reserve(static_cast<size_type>(desired));
    }

    void CopyFirstConditionFrom(const RE::TESTopicInfo* a_sourceInfo, RE::TESTopicInfo* a_targetInfo) {
        if (!a_sourceInfo || !a_targetInfo || !a_sourceInfo->objConditions.head) {
            return;
        }

        auto*& targetHead = a_targetInfo->objConditions.head;
        if (!targetHead) {
            targetHead = a_sourceInfo->objConditions.head;
            return;
        }

        auto* current = targetHead;
        while (current->next) {
            current = current->next;
        }
        current->next = a_sourceInfo->objConditions.head;
    }

}

void PatchCollisionLayers() {
    auto* const data = RE::TESDataHandler::GetSingleton();
    if (!data) {
        logger::warn("Patching collision layers: TESDataHandler unavailable");
        return;
    }

    auto* weapon = data->LookupForm<RE::BGSCollisionLayer>(WeaponCol, Settings::skyrimESM);
    auto* projectile = data->LookupForm<RE::BGSCollisionLayer>(ProjectileCol, Settings::skyrimESM);
    auto* ward = data->LookupForm<RE::BGSCollisionLayer>(WardCol, Settings::skyrimESM);

    if (!(weapon && projectile && ward)) {
        return;
    }

    ReserveExtra(weapon->collidesWith, 1);
    ReserveExtra(projectile->collidesWith, 1);
    ReserveExtra(ward->collidesWith, 2);

    InsertUnique(weapon->collidesWith, ward);
    InsertUnique(projectile->collidesWith, ward);
    InsertUnique(ward->collidesWith, weapon);
    InsertUnique(ward->collidesWith, projectile);

    SortByCollisionIdx(weapon->collidesWith);
    SortByCollisionIdx(projectile->collidesWith);
    SortByCollisionIdx(ward->collidesWith);
}

void PatchImpactDataSets() {
    auto* const data = RE::TESDataHandler::GetSingleton();
    if (!data) {
        logger::warn("Patching impact data sets: TESDataHandler unavailable");
        return;
    }

    auto* wardMaterial = data->LookupForm<RE::BGSMaterialType>(WardMaterial, Settings::skyrimESM);
    auto* arrowImpactDataSet = data->LookupForm<RE::BGSImpactDataSet>(ArrowImpactSet, Settings::skyrimESM);
    auto* arrowVsWardImpact = data->LookupForm<RE::BGSImpactData>(ArrowVsWardImpact, Settings::pluginName);

    if (wardMaterial && arrowImpactDataSet && arrowVsWardImpact) {
        arrowImpactDataSet->impactMap.insert({wardMaterial, arrowVsWardImpact});
    }
}

void PatchHitGrunts() {
    auto* const data = RE::TESDataHandler::GetSingleton();
    if (!data) {
        logger::warn("Patching hit grunts: TESDataHandler unavailable");
        return;
    }

    const auto* gruntPlaceholder = data->LookupForm<RE::TESTopicInfo>(GruntPlaceholder, Settings::pluginName);
    for (const auto id : Grunts) {
        if (auto* grunt = data->LookupForm<RE::TESTopicInfo>(id, Settings::skyrimESM)) {
            CopyFirstConditionFrom(gruntPlaceholder, grunt);
        }
    }
}

}
