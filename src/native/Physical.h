#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

namespace Physical {
void ModifyHitData(RE::HitData* a_hitData);
[[nodiscard]] std::optional<float> GetBlockCost(const RE::HitData& a_hitData);
void OnCombatHit(RE::HitData* a_hitData);

[[nodiscard]] bool ShouldSuppressSkillXP(RE::ActorValue a_av);
}
