#pragma once
#include <REX/REX/Singleton.h>

#include <CLIBUtil/simpleINI.hpp>

class Settings : public REX::Singleton<Settings> {
public:
    enum class ShoutMode : std::uint8_t {
        kVanilla = 0,
        kBreakOnly = 1,
        kBreakWithStagger = 2,
        kBreakWithPassThrough = 3
    };

    void Load() {
        using namespace clib_util::ini;

        const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
        const auto path = std::format("Data/SKSE/Plugins/{}.ini", plugin->GetName());

        CSimpleIniA ini;
        ini.SetUnicode();

        ini.LoadFile(path.c_str());

        //
        // General
        //
        get_value(
            ini,
            showWardMeter,
            "General",
            "bShowWardMeter",
            "; Show ward power meter on HUD while casting a ward spell."
        );

        get_value(
            ini,
            perkGatesPlayerOnly,
            "General",
            "bPerkGatesPlayerOnly",
            "; If true, perk requirements only apply to player; NPCs get features automatically."
        );

        //
        // Tweaks
        //
        get_value(
            ini,
            instantWardCharge,
            "Tweaks",
            "bInstantCharge",
            "; Wards charge instantly to full power.\n"
            "; Ignores fChargeRateMultiplier."
        );

        get_value(
            ini,
            wardChargeRateMultiplier,
            "Tweaks",
            "fChargeRateMultiplier",
            "; Ward charge speed multiplier.\n"
            "; Only applies when bInstantCharge is false."
        );
        wardChargeRateMultiplier = std::clamp(wardChargeRateMultiplier, 0.01f, 10.0f);

        get_value(
            ini,
            wardMagnitudeMultiplier,
            "Tweaks",
            "fMagnitudeMultiplier",
            "; Ward strength multiplier (max ward power)."
        );
        wardMagnitudeMultiplier = std::clamp(wardMagnitudeMultiplier, 0.01f, 10.0f);

        get_value(ini, wardCostMultiplier, "Tweaks", "fCostMultiplier", "; Ward magicka cost multiplier.");
        wardCostMultiplier = std::clamp(wardCostMultiplier, 0.01f, 10.0f);

        get_value(
            ini,
            restrictTweaksToPlayerTeam,
            "Tweaks",
            "bRestrictToPlayerTeam",
            "; If true, tweaks only apply to player and teammates."
        );

        //
        // Physical
        //
        get_value(
            ini,
            staggerNormalAttacks,
            "Physical",
            "bStaggerNormalAttacks",
            "; If true, attackers will stagger when their normal (non-power) melee attack is blocked by a ward."
        );

        get_value(
            ini,
            staggerMagnitude,
            "Physical",
            "fStaggerMagnitude",
            "; Stagger magnitude applied when a normal attack is blocked by a ward."
        );
        staggerMagnitude = std::clamp(staggerMagnitude, 0.1f, 1.f);

        get_value(
            ini,
            staggerPowerAttacks,
            "Physical",
            "bStaggerPowerAttacks",
            "; If true, attackers will recoil when their power melee attack is blocked by a ward."
        );

        get_value(
            ini,
            blockingAngle,
            "Physical",
            "fBlockingAngle",
            "; Blocking cone angle for melee and ranged attacks. Spells use their own hit detection."
        );
        blockingAngle = std::clamp(blockingAngle, 45.f, 180.f);

        get_value(
            ini,
            powerDamageMultiplier,
            "Physical",
            "fPowerDamageMultiplier",
            "; Multiplier applied to physical damage before subtracting from ward power."
        );
        powerDamageMultiplier = std::abs(powerDamageMultiplier);

        get_value(
            ini,
            blockXPScale,
            "Physical",
            "fBlockXPScale",
            "; Multiplier for Restoration XP gained when blocking with a ward."
        );
        blockXPScale = std::abs(std::clamp(blockXPScale, 0.1f, 10.f));

        get_value(
            ini,
            physicalRequiredPerks,
            "Physical",
            "sPhysicalRequiredPerks",
            "; Perks required for physical blocking (plugin|formID,...). Empty = always enabled."
        );

        //
        // Magic.Shouts
        //
        auto shoutModeInt = static_cast<std::int32_t>(shoutMode);
        get_value(
            ini,
            shoutModeInt,
            "Magic.Shouts",
            "iShoutMode",
            "; 0 = Vanilla, 1 = BreakOnly, 2 = Break+Stagger, 3 = Break+PassThrough\n"
            "; Controls the overall ward vs shout behavior. Other shout values act as tuning knobs."
        );
        shoutModeInt = std::clamp(shoutModeInt, 0, 3);
        shoutMode = static_cast<ShoutMode>(shoutModeInt);

        get_value(
            ini,
            playerImmuneToShoutMechanics,
            "Magic.Shouts",
            "bPlayerImmuneToShoutMechanics",
            "; If true, player wards behave like vanilla (absorb shouts completely)."
        );

        get_value(
            ini,
            shoutDamage,
            "Magic.Shouts",
            "fShoutDamage",
            "; Flat ward power damage from shouts.\n"
            "; Only used when bShoutInstantBreak is false."
        );
        shoutDamage = std::clamp(shoutDamage, 0.0f, 250.0f);

        get_value(
            ini,
            shoutInstantBreak,
            "Magic.Shouts",
            "bShoutInstantBreak",
            "; If true, shouts instantly break wards instead of dealing fixed damage.\n"
            "; Ignored in Vanilla mode."
        );

        get_value(
            ini,
            staggerMagnitudeTowardDefender,
            "Magic.Shouts",
            "fStaggerMagnitudeTowardDefender",
            "; Stagger magnitude applied when the ward breaks from a shout in Break+Stagger mode."
        );
        staggerMagnitudeTowardDefender = std::clamp(staggerMagnitudeTowardDefender, 0.1f, 1.f);

        get_value(
            ini,
            damageKeywords,
            "Magic.Shouts",
            "sDamageKeywords",
            "; Keywords treated as damaging for ward/shout logic."
        );

        get_value(
            ini,
            shoutsRequiredPerks,
            "Magic.Shouts",
            "sShoutsRequiredPerks",
            "; Perks required for shout mechanics (plugin|formID,...). Empty = always enabled."
        );

        switch (shoutMode) {
            case ShoutMode::kVanilla:
                shoutPassThrough = false;
                staggerDefenderOnBreak = false;
                shoutInstantBreak = false;
                shoutDamage = 0.0f;
                break;

            case ShoutMode::kBreakOnly:
                shoutPassThrough = false;
                staggerDefenderOnBreak = false;
                break;

            case ShoutMode::kBreakWithStagger:
                shoutPassThrough = false;
                staggerDefenderOnBreak = true;
                break;

            case ShoutMode::kBreakWithPassThrough:
                shoutPassThrough = true;
                staggerDefenderOnBreak = false;
                break;
        }

        //
        // Magic.Effects
        //
        get_value(
            ini,
            blockDiseases,
            "Magic.Effects",
            "bBlockDiseases",
            "; If true, wards can block disease spells when facing the source."
        );

        get_value(
            ini,
            blockCloaks,
            "Magic.Effects",
            "bBlockCloaks",
            "; If true, wards can block hostile cloak spell ticks when facing the source."
        );

        get_value(
            ini,
            cloakDamageMultiplier,
            "Magic.Effects",
            "fCloakDamageMultiplier",
            "; Multiplier applied to cloak effect magnitude when calculating ward power drain.\n"
            "; Set to 0 to block cloak ticks without draining ward power."
        );
        cloakDamageMultiplier = std::clamp(cloakDamageMultiplier, 0.f, 10.f);

        get_value(
            ini,
            diseaseSpells,
            "Magic.Effects",
            "sDiseaseSpells",
            "; Extra spells to treat as diseases. Format: plugin|formID,..."
        );

        get_value(
            ini,
            cloakSpells,
            "Magic.Effects",
            "sCloakSpells",
            "; Extra spells to treat as cloaks. Format: plugin|formID,..."
        );

        get_value(
            ini,
            diseaseRequiredPerks,
            "Magic.Effects",
            "sDiseaseRequiredPerks",
            "; Perks required for disease blocking (plugin|formID,...). Empty = always enabled."
        );

        get_value(
            ini,
            cloakRequiredPerks,
            "Magic.Effects",
            "sCloakRequiredPerks",
            "; Perks required for cloak blocking (plugin|formID,...). Empty = always enabled."
        );

        //
        // Magic.Reflection
        //
        get_value(
            ini,
            enableSpellReflection,
            "Magic.Reflection",
            "bEnableSpellReflection",
            "; If true, wards reflect incoming projectile spells back toward the attacker."
        );

        get_value(
            ini,
            autoAimReflection,
            "Magic.Reflection",
            "bAutoAimReflection",
            "; If true, reflected spells aim at the attacker. If false, spells reflect forward."
        );

        get_value(
            ini,
            reflectionBlameAttacker,
            "Magic.Reflection",
            "bReflectionBlameAttacker",
            "; If true, the original attacker is blamed for reflected spell damage."
        );

        get_value(
            ini,
            reflectionForwardOffset,
            "Magic.Reflection",
            "fReflectionForwardOffset",
            "; Spawn offset for reflected projectiles."
        );
        reflectionForwardOffset = std::clamp(reflectionForwardOffset, 16.0f, 256.0f);

        get_value(
            ini,
            reflectEvenIfWardBroken,
            "Magic.Reflection",
            "bReflectEvenIfWardBroken",
            "; If true, spells are reflected even when they break the ward."
        );

        get_value(
            ini,
            restrictReflectionToPlayerTeam,
            "Magic.Reflection",
            "bRestrictReflectionToPlayerTeam",
            "; If true, only player and follower wards reflect spells."
        );

        get_value(
            ini,
            reflectionRequiredPerks,
            "Magic.Reflection",
            "sReflectionRequiredPerks",
            "; Perks required for reflection (plugin|formID,...). Empty = always enabled."
        );

        //
        // Exclusions
        //
        get_value(
            ini,
            excludedItems,
            "Exclusions",
            "sExcludedItems",
            "; Ward-casting items that should disable PerfectlyValidWards features.\n"
            "; Format: plugin|formID,plugin|formID\n"
            "; Example: Skyrim.esm|0x045F96 (Spellbreaker)"
        );

        get_value(
            ini,
            excludedItemsDisablePhysicalBlocking,
            "Exclusions",
            "bExcludedItemsDisablePhysicalBlocking",
            "; If true, excluded ward items disable physical ward mechanics."
        );

        get_value(
            ini,
            excludedItemsDisableShoutMechanics,
            "Exclusions",
            "bExcludedItemsDisableShoutMechanics",
            "; If true, excluded ward items disable shout vs ward mechanics."
        );

        get_value(
            ini,
            excludedItemsDisableDiseaseBlocking,
            "Exclusions",
            "bExcludedItemsDisableDiseaseBlocking",
            "; If true, excluded ward items disable disease blocking."
        );

        get_value(
            ini,
            excludedItemsDisableCloakBlocking,
            "Exclusions",
            "bExcludedItemsDisableCloakBlocking",
            "; If true, excluded ward items disable cloak blocking."
        );

        get_value(
            ini,
            excludedItemsDisableReflection,
            "Exclusions",
            "bExcludedItemsDisableReflection",
            "; If true, excluded ward items disable spell reflection."
        );

        get_value(
            ini,
            shoutExclusions,
            "Exclusions",
            "sShoutExclusions",
            "; Shouts excluded from ward mechanics. Word spells are excluded automatically.\n"
            "; Format: plugin|formID,..."
        );

        get_value(
            ini,
            reflectionExclusions,
            "Exclusions",
            "sReflectionExclusions",
            "; Spells excluded from reflection (plugin|formID,...)"
        );

        (void)ini.SaveFile(path.c_str());
    }

    void ResolveRuntimeData() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return;
        }

        emptyActivator = data->LookupForm<RE::TESObjectACTI>(FXEmptyActivator, skyrimESM);
        if (!emptyActivator) {
            logger::warn("Settings: failed to resolve FXEmptyActivator");
        } else {
            logger::info("Settings: resolved FXEmptyActivator <{:08X}>", emptyActivator->GetFormID());
        }
    }

    // members
    static constexpr auto pluginName {"PerfectlyValidWards.esp"};
    static constexpr auto skyrimESM {"Skyrim.esm"};

    // runtime
    static constexpr uint32_t FXEmptyActivator = 0xB79FF;
    RE::TESBoundObject* emptyActivator {nullptr};

    // general
    bool showWardMeter {true};
    bool perkGatesPlayerOnly {false};

    // tweaks
    bool instantWardCharge {false};
    float wardChargeRateMultiplier {1.0f};
    float wardMagnitudeMultiplier {1.0f};
    float wardCostMultiplier {1.0f};
    bool restrictTweaksToPlayerTeam {false};

    // physical
    bool staggerNormalAttacks {false};
    float staggerMagnitude {0.3f};
    bool staggerPowerAttacks {true};
    float blockingAngle {90.f};
    float powerDamageMultiplier {1.f};
    float blockXPScale {0.25f};
    std::vector<std::pair<std::string, std::uint32_t>> physicalRequiredPerks;

    // magic.shouts
    ShoutMode shoutMode {ShoutMode::kBreakWithPassThrough};
    bool staggerDefenderOnBreak {false};
    float staggerMagnitudeTowardDefender {0.3f};
    bool shoutPassThrough {false};
    bool playerImmuneToShoutMechanics {false};
    float shoutDamage {40.0f};
    bool shoutInstantBreak {false};
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
    bool blockDiseases {true};
    bool blockCloaks {true};
    float cloakDamageMultiplier {1.0f};
    std::vector<std::pair<std::string, std::uint32_t>> diseaseSpells;
    std::vector<std::pair<std::string, std::uint32_t>> cloakSpells;
    std::vector<std::pair<std::string, std::uint32_t>> diseaseRequiredPerks;
    std::vector<std::pair<std::string, std::uint32_t>> cloakRequiredPerks;

    // magic.reflection
    bool enableSpellReflection {true};
    bool autoAimReflection {true};
    bool reflectionBlameAttacker {true};
    float reflectionForwardOffset {48.0f};
    bool reflectEvenIfWardBroken {true};
    bool restrictReflectionToPlayerTeam {false};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionRequiredPerks;

    // exclusions
    std::vector<std::pair<std::string, std::uint32_t>> excludedItems;
    bool excludedItemsDisablePhysicalBlocking {true};
    bool excludedItemsDisableShoutMechanics {true};
    bool excludedItemsDisableDiseaseBlocking {true};
    bool excludedItemsDisableCloakBlocking {true};
    bool excludedItemsDisableReflection {true};
    std::vector<std::pair<std::string, std::uint32_t>> shoutExclusions {{skyrimESM, 0x000E5F68}};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionExclusions;

private:
    template <class T>
    static void get_value(
        CSimpleIniA& a_ini,
        T& a_value,
        const char* a_section,
        const char* a_key,
        const char* a_comment,
        const char* a_delimiter = R"(|)"
    ) {
        clib_util::ini::get_value(a_ini, a_value, a_section, a_key, a_comment, a_delimiter);
    }

    static void get_value(
        CSimpleIniA& a_ini,
        std::vector<std::pair<std::string, uint32_t>>& a_value,
        const char* a_section,
        const char* a_key,
        const char* a_comment
    ) {
        std::vector<std::string> raw;
        raw.reserve(a_value.size());
        for (auto&& [plugin, id] : a_value) {
            raw.emplace_back(std::format("{}|0x{:X}", plugin, id));
        }

        clib_util::ini::get_value(a_ini, raw, a_section, a_key, a_comment, ",");

        a_value.clear();
        for (auto&& entry : raw) {
            if (auto parsed = stl::detail::parse_plugin_form(entry)) {
                a_value.emplace_back(std::move(*parsed));
            }
        }
    }
};
