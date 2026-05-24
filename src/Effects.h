#pragma once

namespace Effects {
[[nodiscard]] std::optional<bool> OnMagicTargetAdd(
    RE::MagicTarget* a_this,
    const RE::MagicTarget::AddTargetData* a_data
);
}
