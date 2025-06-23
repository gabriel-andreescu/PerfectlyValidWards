#include "Hooks.h"

#include "Settings.h"

namespace Hooks {
    void Install() {
        stl::write_thunk_call<VATS_SetMagicTimeSlowdown>(
            REL::Relocation{
                RELOCATION_ID(34175, 34968),
                //REL::Relocate(0x5D, 0x5D)
                0x5D
            }
        );
        stl::write_vfunc<RE::ArrowProjectile, ArrowProjectile_AddImpact>();
        stl::hook_function_prologue<Actor_GetCarryWeight, 0x6>(
            REL::Relocation{ RELOCATION_ID(36456, 37452) }
        );
    }

    void VATS_SetMagicTimeSlowdown::thunk(
        RE::VATS* a_this,
        float a_magicTimeSlowdown,
        float a_playerMagicTimeSlowdown
    ) {
        func(a_this, a_magicTimeSlowdown, a_playerMagicTimeSlowdown);
    }

    void ArrowProjectile_AddImpact::thunk(
        RE::Projectile* a_this,
        RE::TESObjectREFR* a_ref,
        const RE::NiPoint3& a_targetLoc,
        const RE::NiPoint3& a_velocity,
        RE::hkpCollidable* a_collidable,
        std::int32_t a_arg6,
        std::uint32_t a_arg7
    ) {
        if (a_ref) {
            if (auto* actor = skyrim_cast<RE::Actor*>(a_ref);
                actor) {
                logger::debug("{} got hit by an arrow", actor->GetDisplayFullName());
            }
        }

        func(a_this, a_ref, a_targetLoc, a_velocity, a_collidable, a_arg6, a_arg7);
    }

    float Actor_GetCarryWeight::thunk(RE::Actor* a_this) {
        const auto result = func(a_this);
        const auto carryWeight = Settings::GetSingleton()->maxCarryWeight;

        if (carryWeight > 0 && a_this == RE::PlayerCharacter::GetSingleton()) {
            return carryWeight;
        }

        return result;
    }
}
