#pragma once

namespace Physical {
void ModifyHitData(RE::HitData* a_hitData);
[[nodiscard]] std::optional<float> GetBlockCost(const RE::HitData& a_hitData);
void OnCombatHit(RE::HitData* a_hitData);

struct XPFilterResult {
    bool suppress;
};
[[nodiscard]] XPFilterResult FilterSkillXP(RE::ActorValue a_av);
}
