#pragma once

namespace RE {
    // credit: DavidJCobb (https://github.com/DavidJCobb/skyrim-classic-re)
    struct TESMagicWardHitEvent {
        enum class Status : std::int32_t {
            kFriendlyHit = 0, // per CK wiki; can wards detect/block friendly healing, or is this for friendly fire?
            kAbsorbed    = 1,
            kWardBroke   = 2
            //
            // I've seen Drain Life pass 0 when it was cast on the player by a hostile actor. Maybe
            // 0 just means that the spell passed through the ward, but Bethesda had friendly heals
            // in mind when they wrote the docs?
            //
        };

        TESObjectREFR* defender; // 00 // Actor casting the ward
        TESObjectREFR* attacker; // 08 // Actor casting the spell that hit the ward
        FormID spell;            // 10
        Status status;           // 14
    };
    static_assert(sizeof(TESMagicWardHitEvent) == 0x18);
}
