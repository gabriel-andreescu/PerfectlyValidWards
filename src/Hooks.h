#pragma once

namespace Hooks {
    void Install();

    struct VATS_SetMagicTimeSlowdown {
        static void thunk(RE::VATS* a_this, float a_magicTimeSlowdown, float a_playerMagicTimeSlowdown);

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct ArrowProjectile_AddImpact {
        static void thunk(
            RE::Projectile* a_this,
            RE::TESObjectREFR* a_ref,
            const RE::NiPoint3& a_targetLoc,
            const RE::NiPoint3& a_velocity,
            RE::hkpCollidable* a_collidable,
            std::int32_t a_arg6,
            std::uint32_t a_arg7
        );

        static inline REL::Relocation<decltype(thunk)> func;
        static constexpr std::size_t idx = 0xBD; // AddImpact
    };

    struct Actor_GetCarryWeight {
        static float thunk(RE::Actor* a_this);

        static inline REL::Relocation<decltype(thunk)> func;
    };
}
