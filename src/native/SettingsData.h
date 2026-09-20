#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class ShoutMode : std::uint8_t {
    kVanilla = 0,
    kBreakOnly = 1,
    kBreakWithStagger = 2,
    kBreakWithPassThrough = 3,
};

// The field order mirrors the settings file domains for auditable reads and writes.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct SettingsData {
    bool showWardMeter {true};
    bool perkGatesPlayerOnly {false};
    bool debugLogging {false};

    bool instantWardCharge {false};
    bool instantWardCast {false};
    float wardChargeRateMultiplier {1.0F};
    float wardMagnitudeMultiplier {1.0F};
    float wardCostMultiplier {1.0F};
    bool restrictTweaksToPlayerTeam {false};

    bool blockMelee {true};
    bool blockArrows {true};
    bool staggerNormalAttacks {false};
    float staggerMagnitude {0.3F};
    bool staggerPowerAttacks {true};
    float blockingAngle {90.0F};
    float powerDamageMultiplier {1.0F};
    float blockXPScale {0.25F};
    std::vector<std::pair<std::string, std::uint32_t>> physicalRequiredPerks;

    ShoutMode shoutMode {ShoutMode::kBreakWithPassThrough};
    bool staggerDefenderOnBreak {false};
    float staggerMagnitudeTowardDefender {0.3F};
    bool shoutPassThrough {false};
    bool playerImmuneToShoutMechanics {false};
    float shoutDamage {40.0F};
    bool shoutInstantBreak {false};
    std::vector<std::string> damageKeywords {
        "MagicDamageFire",
        "MagicDamageFrost",
        "MagicDamageShock",
        "MagicDamageStamina",
        "MagicDamageMagicka",
        "MagicDamageHealth",
        "MagicDamageDrain",
        "MagicDamagePoison",
    };
    std::vector<std::pair<std::string, std::uint32_t>> shoutsRequiredPerks;

    bool blockDiseases {true};
    bool blockCloaks {true};
    float cloakDamageMultiplier {1.0F};
    std::vector<std::pair<std::string, std::uint32_t>> diseaseSpells;
    std::vector<std::pair<std::string, std::uint32_t>> cloakSpells;
    std::vector<std::pair<std::string, std::uint32_t>> diseaseRequiredPerks;
    std::vector<std::pair<std::string, std::uint32_t>> cloakRequiredPerks;

    bool enableSpellReflection {true};
    bool autoAimReflection {true};
    bool reflectionBlameAttacker {true};
    float reflectionForwardOffset {48.0F};
    bool reflectEvenIfWardBroken {true};
    bool restrictReflectionToPlayerTeam {false};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionRequiredPerks;

    std::vector<std::pair<std::string, std::uint32_t>> excludedItems;
    bool excludedItemsDisablePhysicalBlocking {true};
    bool excludedItemsDisableShoutMechanics {true};
    bool excludedItemsDisableDiseaseBlocking {true};
    bool excludedItemsDisableCloakBlocking {true};
    bool excludedItemsDisableReflection {true};
    std::vector<std::pair<std::string, std::uint32_t>> shoutExclusions {{"Skyrim.esm", 0x000E5F68}};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionExclusions;
};
