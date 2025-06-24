#pragma once
#include <shared_mutex>

namespace IDs {
    inline constexpr uint32_t WeaponCol = 0x88766;
    inline constexpr uint32_t ProjectileCol = 0x88767;
    inline constexpr uint32_t WardCol = 0x8877E;
    inline constexpr uint32_t ArrowImpactSet = 0x193B9;
    inline constexpr uint32_t WardMaterial = 0x1EFF7;
    inline constexpr uint32_t ArrowVsWardImpact = 0x800;

    inline constexpr uint32_t GruntPlaceholder = 0xD65;
    inline constexpr uint32_t Grunts[] = { 0x3E249, 0x3E24A, 0x3E24B, 0x3E9A1, 0x3E9A2 };
}

struct HandleHash {
    std::size_t operator()(const RE::ActorHandle& h) const noexcept { return h.native_handle(); }
};

class RecentWardBlocks
{
public:
    void flag(RE::Actor* a)
    {
        if (!a) return;
        std::unique_lock lk(mx_);
        set_.insert(a->GetHandle());
    }

    bool consume(RE::Actor* a)
    {
        if (!a) return false;
        std::unique_lock lk(mx_);
        return set_.erase(a->GetHandle()) > 0;
    }

    bool contains(RE::Actor* a) const
    {
        if (!a) return false;
        std::shared_lock lk(mx_);
        return set_.contains(a->GetHandle());
    }

private:
    std::unordered_set<RE::ActorHandle, HandleHash> set_;
    mutable std::shared_mutex mx_;
} inline g_wardBlocks;

namespace WardManager {
    [[nodiscard]] float GetCurrentWardPower(RE::Actor* a_actor);

    [[nodiscard]] float EstimateWardPowerDamage(
        RE::Actor* a_defender,
        RE::Actor* a_attacker,
        RE::TESObjectWEAP* a_weapon,
        bool a_powerAttack
    );

    void DamageWardPower(RE::Actor* a_actor, float a_damage);

    void PatchCollisionLayers();

    void PatchImpactDataSets();

    void PatchHitGrunts();

    void FlagWardBlock(RE::Actor* a_actor);

    [[nodiscard]] bool ConsumeWardBlock(RE::Actor* a_actor);

    template<class T, class Alloc>
    void sort_by_collision_idx(RE::BSTArray<T*, Alloc>& arr) {
        std::ranges::sort(arr, [](auto* a, auto* b) { return a->collisionIdx < b->collisionIdx; });
    }

    [[nodiscard]] inline float get_float_gs(const char* name, float fallback = 1.f) {
        if (auto* s = RE::GameSettingCollection::GetSingleton()) {
            if (auto* set = s->GetSetting(name); set) { return set->GetFloat(); }
        }
        return fallback;
    }

    template <class Arr>
    void insert_unique(Arr& arr, RE::BGSCollisionLayer* layer)
    {
        if (!std::ranges::contains(arr, layer)) {
            arr.push_back(layer);
        }
    }

    template <class Arr>
    void reserve_extra(Arr& arr, std::size_t extra)
    {
        using size_type = typename Arr::size_type;  // BSTArray::size_type == std::uint32_t
        const std::size_t desired = arr.size() + extra;

        static_assert(sizeof(size_type) == 4, "BSTArray::size_type changed?");
        assert(desired <= std::numeric_limits<size_type>::max());

        arr.reserve(static_cast<size_type>(desired));
    }
}
