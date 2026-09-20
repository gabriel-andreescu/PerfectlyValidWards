#pragma once

#include <array>
#include <cstdint>

namespace Patches {

namespace IDs {
    inline constexpr std::uint32_t kWeaponCol = 0x88766;
    inline constexpr std::uint32_t kProjectileCol = 0x88767;
    inline constexpr std::uint32_t kWardCol = 0x8877E;
    inline constexpr std::uint32_t kArrowImpactSet = 0x193B9;
    inline constexpr std::uint32_t kWardMaterial = 0x1EFF7;
    inline constexpr std::uint32_t kArrowVsWardImpact = 0x800;
    inline constexpr std::uint32_t kGruntPlaceholder = 0xD65;
    inline constexpr auto kGrunts = std::to_array<std::uint32_t>({
        0x3E249,
        0x3E24A,
        0x3E24B,
        0x3E9A1,
        0x3E9A2,
    });
}

void PatchCollisionLayers();
void PatchImpactDataSets();
void PatchHitGrunts();

}
