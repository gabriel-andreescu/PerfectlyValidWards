#pragma once

namespace Patches {

namespace IDs {
    inline constexpr std::uint32_t WeaponCol = 0x88766;
    inline constexpr std::uint32_t ProjectileCol = 0x88767;
    inline constexpr std::uint32_t WardCol = 0x8877E;
    inline constexpr std::uint32_t ArrowImpactSet = 0x193B9;
    inline constexpr std::uint32_t WardMaterial = 0x1EFF7;
    inline constexpr std::uint32_t ArrowVsWardImpact = 0x800;
    inline constexpr std::uint32_t GruntPlaceholder = 0xD65;
    inline constexpr auto Grunts = std::to_array<std::uint32_t>({
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
