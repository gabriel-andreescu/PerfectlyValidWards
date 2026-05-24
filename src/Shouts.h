#pragma once

namespace Shouts {
void ProcessWardHit(RE::Actor* a_defender, RE::Actor* a_attacker, RE::MagicItem* a_spell);
void ApplyPassThrough(RE::Actor* a_defender, RE::Actor* a_attacker, RE::MagicItem* a_spell);
[[nodiscard]] bool IsPassThroughCandidate(RE::MagicItem* a_shoutSpell);
}
