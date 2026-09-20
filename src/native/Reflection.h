#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Events.h"

namespace Reflection {
void ProcessWardHit(
    RE::Actor* a_defender,
    RE::Actor* a_attacker,
    RE::MagicItem* a_spell,
    RE::TESMagicWardHitEvent::Status a_status
);
}
