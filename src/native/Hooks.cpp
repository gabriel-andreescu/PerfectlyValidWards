#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Effects.h"
#include "Hooks.h"
#include "Physical.h"
#include "SkyrimUtil.h"
#include <RE/A/Actor.h>
#include <RE/H/HitData.h>
#include <RE/M/MagicTarget.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

namespace Hooks {
void Install() {
    stl::WriteThunkCall<ActorCombatHit>(REL::Relocation {RELOCATION_ID(37673, 38627), REL::Relocate(0x3c0, 0x4A8)});

    // SSE: Up      p   HitData__Populate_140742850+37C                     call    HitData__Resolve_140743510
    // AE: Up       p   HitData__Populate_1407DAFF0+358                     call    HitData__Resolve_1407DBD20
    stl::WriteThunkCall<HitDataResolve>(
        REL::Relocation {RELOCATION_ID(42832, 44001), REL::Relocate(0x37C, 0x358, 0x3CF)}
    );

    stl::WriteThunkCall<ActorGetBlockCost>(REL::Relocation {RELOCATION_ID(37633, 38586), REL::Relocate(0x8D4, 0xB39)});

    // SSE: MagicTarget::AddTarget caller +0x1E8
    // AE:  MagicTarget::AddTarget caller +0x20B
    stl::WriteThunkCall<MagicTargetAddTarget>(
        REL::Relocation {RELOCATION_ID(33742, 34526), REL::Relocate(0x1E8, 0x20B)}
    );

    SKSE::log::info("Hooks: installed");
}

bool HitDataResolve::thunk(RE::HitData* a_hitData, bool a_ignoreBlocking) {
    Physical::ModifyHitData(a_hitData);
    return func(a_hitData, a_ignoreBlocking);
}

float ActorGetBlockCost::thunk(RE::HitData& a_hitData) {
    if (const auto cost = Physical::GetBlockCost(a_hitData)) {
        return *cost;
    }
    return func(a_hitData);
}

float ActorCombatHit::thunk(RE::Actor* a_this, RE::HitData* a_hitData) {
    Physical::OnCombatHit(a_hitData);
    return func(a_this, a_hitData);
}

bool MagicTargetAddTarget::thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData* a_data) {
    if (const auto result = Effects::OnMagicTargetAdd(a_this, a_data)) {
        return *result;
    }
    return func(a_this, a_data);
}
}
