#include "Hooks.h"
#include "Effects.h"
#include "Physical.h"

namespace Hooks {
void Install() {
    stl::write_thunk_call<Actor_CombatHit>(REL::Relocation {RELOCATION_ID(37673, 38627), REL::Relocate(0x3c0, 0x4A8)});

    // SSE: Up      p   HitData__Populate_140742850+37C                     call    HitData__Resolve_140743510
    // AE: Up       p   HitData__Populate_1407DAFF0+358                     call    HitData__Resolve_1407DBD20
    stl::write_thunk_call<HitData_Resolve>(
        REL::Relocation {RELOCATION_ID(42832, 44001), REL::Relocate(0x37C, 0x358, 0x3CF)}
    );

    stl::write_thunk_call<Actor_GetBlockCost>(
        REL::Relocation {RELOCATION_ID(37633, 38586), REL::Relocate(0x8D4, 0xB39)}
    );

    // SSE: Up      p   PlayerCharacter__AdvanceSkill_1406A2540+25          call    sub_1406E61D0
    // AE:  Up      p   PlayerCharacter__AddSkillExperience_140736E20+25    call    sub_7FF7F480AE60
    stl::write_thunk_call<AddSkillExperience>(REL::Relocation {RELOCATION_ID(39413, 40488), 0x25});

    // SSE: MagicTarget::AddTarget caller +0x1E8
    // AE:  MagicTarget::AddTarget caller +0x20B
    stl::write_thunk_call<MagicTargetAddTarget>(
        REL::Relocation {RELOCATION_ID(33742, 34526), REL::Relocate(0x1E8, 0x20B)}
    );

    logger::info("Hooks: installed");
}

bool HitData_Resolve::thunk(RE::HitData* a_hitData, bool a_ignoreBlocking) {
    Physical::ModifyHitData(a_hitData);
    return func(a_hitData, a_ignoreBlocking);
}

float Actor_GetBlockCost::thunk(RE::HitData& a_hitData) {
    if (const auto cost = Physical::GetBlockCost(a_hitData)) {
        return *cost;
    }
    return func(a_hitData);
}

float Actor_CombatHit::thunk(RE::Actor* a_this, RE::HitData* a_hitData) {
    Physical::OnCombatHit(a_hitData);
    return func(a_this, a_hitData);
}

void AddSkillExperience::thunk(
    float** a_skills,
    RE::ActorValue a_av,
    float a_xp,
    std::uint64_t a_unk1,
    std::uint32_t a_unk2,
    bool a_applyMult,
    bool a_silent
) {
    auto [suppress] = Physical::FilterSkillXP(a_av);
    if (suppress) {
        logger::debug("Ignored XP from blocking | skill={} | xp={:.2f}", RE::ActorValueToString(a_av), a_xp);
        return;
    }
    func(a_skills, a_av, a_xp, a_unk1, a_unk2, a_applyMult, a_silent);
}

bool MagicTargetAddTarget::thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData* a_data) {
    if (const auto result = Effects::OnMagicTargetAdd(a_this, a_data)) {
        return *result;
    }
    return func(a_this, a_data);
}
}
