#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

namespace Effects {
[[nodiscard]] std::optional<bool> OnMagicTargetAdd(
    RE::MagicTarget* a_this,
    const RE::MagicTarget::AddTargetData* a_data
);
}
