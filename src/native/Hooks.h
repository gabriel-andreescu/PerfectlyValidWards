#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

namespace Hooks {
void Install();

struct ActorCombatHit {
    static float thunk(RE::Actor* a_this, RE::HitData* a_hitData);

    static inline REL::Relocation<decltype(thunk)> func;
};

struct HitDataResolve {
    static bool thunk(RE::HitData* a_hitData, bool a_ignoreBlocking);

    static inline REL::Relocation<decltype(thunk)> func;
};

struct ActorGetBlockCost {
    static float thunk(RE::HitData& a_hitData);

    static inline REL::Relocation<decltype(thunk)> func;
};

struct MagicTargetAddTarget {
    static bool thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData* a_data);

    static inline REL::Relocation<decltype(thunk)> func;
};
}
