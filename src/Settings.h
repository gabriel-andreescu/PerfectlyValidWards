#pragma once

#include <REX/REX/Singleton.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Settings : public REX::Singleton<Settings> {
public:
    enum class ShoutMode : std::uint8_t {
        kVanilla = 0,
        kBreakOnly = 1,
        kBreakWithStagger = 2,
        kBreakWithPassThrough = 3
    };

    struct ReloadResult {
        bool changed {false};
    };

    void Load();
    [[nodiscard]] ReloadResult Reload();
    void ResolveRuntimeData();

    static constexpr auto pluginName {"PerfectlyValidWards.esp"};
    static constexpr auto skyrimESM {"Skyrim.esm"};

    // runtime
    static constexpr std::uint32_t FXEmptyActivator = 0xB79FF;
    RE::TESBoundObject* emptyActivator {nullptr};

    // general
    std::atomic_bool showWardMeter {true};
    std::atomic_bool perkGatesPlayerOnly {false};

    // tweaks
    std::atomic_bool instantWardCharge {false};
    std::atomic<float> wardChargeRateMultiplier {1.0f};
    std::atomic<float> wardMagnitudeMultiplier {1.0f};
    std::atomic<float> wardCostMultiplier {1.0f};
    std::atomic_bool restrictTweaksToPlayerTeam {false};

    // physical
    std::atomic_bool staggerNormalAttacks {false};
    std::atomic<float> staggerMagnitude {0.3f};
    std::atomic_bool staggerPowerAttacks {true};
    std::atomic<float> blockingAngle {90.0f};
    std::atomic<float> powerDamageMultiplier {1.0f};
    std::atomic<float> blockXPScale {0.25f};
    std::vector<std::pair<std::string, std::uint32_t>> physicalRequiredPerks;

    // magic.shouts
    std::atomic<ShoutMode> shoutMode {ShoutMode::kBreakWithPassThrough};
    std::atomic_bool staggerDefenderOnBreak {false};
    std::atomic<float> staggerMagnitudeTowardDefender {0.3f};
    std::atomic_bool shoutPassThrough {false};
    std::atomic_bool playerImmuneToShoutMechanics {false};
    std::atomic<float> shoutDamage {40.0f};
    std::atomic_bool shoutInstantBreak {false};
    std::vector<std::string> damageKeywords {
        "MagicDamageFire",
        "MagicDamageFrost",
        "MagicDamageShock",
        "MagicDamageStamina",
        "MagicDamageMagicka",
        "MagicDamageHealth",
        "MagicDamageDrain",
        "MagicDamagePoison"
    };
    std::vector<std::pair<std::string, std::uint32_t>> shoutsRequiredPerks;

    // magic.effects
    std::atomic_bool blockDiseases {true};
    std::atomic_bool blockCloaks {true};
    std::atomic<float> cloakDamageMultiplier {1.0f};
    std::vector<std::pair<std::string, std::uint32_t>> diseaseSpells;
    std::vector<std::pair<std::string, std::uint32_t>> cloakSpells;
    std::vector<std::pair<std::string, std::uint32_t>> diseaseRequiredPerks;
    std::vector<std::pair<std::string, std::uint32_t>> cloakRequiredPerks;

    // magic.reflection
    std::atomic_bool enableSpellReflection {true};
    std::atomic_bool autoAimReflection {true};
    std::atomic_bool reflectionBlameAttacker {true};
    std::atomic<float> reflectionForwardOffset {48.0f};
    std::atomic_bool reflectEvenIfWardBroken {true};
    std::atomic_bool restrictReflectionToPlayerTeam {false};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionRequiredPerks;

    // exclusions
    std::vector<std::pair<std::string, std::uint32_t>> excludedItems;
    std::atomic_bool excludedItemsDisablePhysicalBlocking {true};
    std::atomic_bool excludedItemsDisableShoutMechanics {true};
    std::atomic_bool excludedItemsDisableDiseaseBlocking {true};
    std::atomic_bool excludedItemsDisableCloakBlocking {true};
    std::atomic_bool excludedItemsDisableReflection {true};
    std::vector<std::pair<std::string, std::uint32_t>> shoutExclusions {{skyrimESM, 0x000E5F68}};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionExclusions;
};
