#pragma once

namespace Hooks {
    void Install();

    struct Actor_CombatHit {
        static float thunk(RE::Actor* a_this, RE::HitData* a_hitData);

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct HitData_Resolve {
        static bool thunk(RE::HitData* a_hitData, bool a_ignoreBlocking);

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct Actor_GetBlockCost {
        static float thunk(RE::HitData& a_hitData);

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct PlayerCharacter_AddSkillExperience {
        static void thunk(RE::PlayerCharacter* a_this, RE::ActorValue a_skill, float a_experience);

        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct AddSkillExperience {
        static void thunk(
            float** a_skills,
            RE::ActorValue a_av,
            float a_xp,
            std::uint64_t a_unk1,
            std::uint32_t a_unk2,
            bool a_applyMult,
            bool a_silent
        );

        static inline REL::Relocation<decltype(thunk)> func;
    };
}
