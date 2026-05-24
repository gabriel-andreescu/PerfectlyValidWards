#include "Settings.h"

#include <CLIBUtil/simpleINI.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <utility>

namespace {
constexpr auto kModName = "PerfectlyValidWards"sv;
constexpr auto kConfigRoot = "Data/MCM/Config"sv;
constexpr auto kSettingsRoot = "Data/MCM/Settings"sv;
constexpr auto kLegacySettingsPath = "Data/SKSE/Plugins/PerfectlyValidWards.ini"sv;

struct SettingsData {
    bool showWardMeter {true};
    bool perkGatesPlayerOnly {false};

    bool instantWardCharge {false};
    float wardChargeRateMultiplier {1.0f};
    float wardMagnitudeMultiplier {1.0f};
    float wardCostMultiplier {1.0f};
    bool restrictTweaksToPlayerTeam {false};

    bool staggerNormalAttacks {false};
    float staggerMagnitude {0.3f};
    bool staggerPowerAttacks {true};
    float blockingAngle {90.0f};
    float powerDamageMultiplier {1.0f};
    float blockXPScale {0.25f};
    std::vector<std::pair<std::string, std::uint32_t>> physicalRequiredPerks;

    Settings::ShoutMode shoutMode {Settings::ShoutMode::kBreakWithPassThrough};
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

    bool blockDiseases {true};
    bool blockCloaks {true};
    float cloakDamageMultiplier {1.0f};
    std::vector<std::pair<std::string, std::uint32_t>> diseaseSpells;
    std::vector<std::pair<std::string, std::uint32_t>> cloakSpells;
    std::vector<std::pair<std::string, std::uint32_t>> diseaseRequiredPerks;
    std::vector<std::pair<std::string, std::uint32_t>> cloakRequiredPerks;

    bool enableSpellReflection {true};
    bool autoAimReflection {true};
    bool reflectionBlameAttacker {true};
    float reflectionForwardOffset {48.0f};
    bool reflectEvenIfWardBroken {true};
    bool restrictReflectionToPlayerTeam {false};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionRequiredPerks;

    std::vector<std::pair<std::string, std::uint32_t>> excludedItems;
    bool excludedItemsDisablePhysicalBlocking {true};
    bool excludedItemsDisableShoutMechanics {true};
    bool excludedItemsDisableDiseaseBlocking {true};
    bool excludedItemsDisableCloakBlocking {true};
    bool excludedItemsDisableReflection {true};
    std::vector<std::pair<std::string, std::uint32_t>> shoutExclusions {{Settings::skyrimESM, 0x000E5F68}};
    std::vector<std::pair<std::string, std::uint32_t>> reflectionExclusions;
};

enum class ReadMode {
    kLiveOnly,
    kAll
};

[[nodiscard]] std::filesystem::path DefaultSettingsPath() {
    return std::filesystem::path {kConfigRoot} / kModName / "settings.ini";
}

[[nodiscard]] std::filesystem::path UserSettingsPath() {
    return std::filesystem::path {kSettingsRoot} / std::format("{}.ini", kModName);
}

[[nodiscard]] std::string Trim(std::string_view a_value) {
    constexpr auto whitespace = " \t\r\n"sv;
    const auto start = a_value.find_first_not_of(whitespace);
    if (start == std::string_view::npos) {
        return {};
    }

    const auto end = a_value.find_last_not_of(whitespace);
    return std::string {a_value.substr(start, end - start + 1)};
}

[[nodiscard]] std::string EncodePluginForms(const std::vector<std::pair<std::string, std::uint32_t>>& a_value) {
    std::string result;
    for (const auto& [plugin, id] : a_value) {
        if (!result.empty()) {
            result.append(",");
        }
        result.append(std::format("{}|0x{:X}", plugin, id));
    }
    return result;
}

[[nodiscard]] std::vector<std::pair<std::string, std::uint32_t>> ParsePluginForms(
    const std::vector<std::string>& a_raw,
    const std::string_view a_key
) {
    std::vector<std::pair<std::string, std::uint32_t>> parsed;
    parsed.reserve(a_raw.size());

    for (const auto& rawEntry : a_raw) {
        const auto entry = Trim(rawEntry);
        if (entry.empty()) {
            continue;
        }

        if (auto form = stl::detail::parse_plugin_form(entry)) {
            parsed.emplace_back(std::move(*form));
        } else {
            logger::warn("Settings: ignored invalid form entry | key={} | value='{}'", a_key, entry);
        }
    }

    return parsed;
}

template <class T>
void ReadValue(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key) {
    clib_util::ini::get_value(a_ini, a_value, a_section, a_key);
}

void ReadValue(CSimpleIniA& a_ini, std::vector<std::string>& a_value, const char* a_section, const char* a_key) {
    auto raw = std::string {clib_util::string::join(a_value, ",")};
    clib_util::ini::get_value(a_ini, raw, a_section, a_key);

    auto delimiter = ","sv;
    if (!raw.contains(',') && raw.contains('|')) {
        delimiter = "|"sv;
    }

    a_value.clear();
    auto remaining = std::string_view {raw};
    while (!remaining.empty()) {
        const auto pos = remaining.find(delimiter);
        const auto entry = pos == std::string_view::npos ? remaining : remaining.substr(0, pos);
        if (auto trimmed = Trim(entry); !trimmed.empty()) {
            a_value.emplace_back(std::move(trimmed));
        }

        if (pos == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(pos + delimiter.size());
    }
}

void ReadValue(
    CSimpleIniA& a_ini,
    std::vector<std::pair<std::string, std::uint32_t>>& a_value,
    const char* a_section,
    const char* a_key
) {
    auto raw = EncodePluginForms(a_value);
    clib_util::ini::get_value(a_ini, raw, a_section, a_key);

    std::vector<std::string> entries;
    auto remaining = std::string_view {raw};
    while (!remaining.empty()) {
        const auto pos = remaining.find(',');
        const auto entry = pos == std::string_view::npos ? remaining : remaining.substr(0, pos);
        entries.emplace_back(Trim(entry));

        if (pos == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(pos + 1);
    }

    a_value = ParsePluginForms(entries, a_key);
}

template <class T>
void WriteValue(
    CSimpleIniA& a_ini,
    const T& a_value,
    const char* a_section,
    const char* a_key,
    const char* a_comment,
    const char* a_delimiter = R"(|)"
) {
    if constexpr (std::is_same_v<T, bool>) {
        a_ini.SetValue(a_section, a_key, a_value ? "1" : "0", a_comment);
    } else if constexpr (std::is_floating_point_v<T>) {
        a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
    } else if constexpr (std::is_enum_v<T>) {
        a_ini.SetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str(), a_comment);
    } else if constexpr (std::is_arithmetic_v<T>) {
        a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str(), a_comment);
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        a_ini.SetValue(a_section, a_key, clib_util::string::join(a_value, a_delimiter).c_str(), a_comment);
    } else {
        a_ini.SetValue(a_section, a_key, a_value.c_str(), a_comment);
    }
}

void WriteValue(
    CSimpleIniA& a_ini,
    const std::vector<std::pair<std::string, std::uint32_t>>& a_value,
    const char* a_section,
    const char* a_key,
    const char* a_comment
) {
    a_ini.SetValue(a_section, a_key, EncodePluginForms(a_value).c_str(), a_comment);
}

template <class T>
[[nodiscard]] T ClampValue(T a_value, T a_min, T a_max, const std::string_view a_key) {
    const auto clamped = std::clamp(a_value, a_min, a_max);
    if (clamped != a_value) {
        logger::warn("Settings: clamped | key={} | value={} | clamped={}", a_key, a_value, clamped);
    }
    return clamped;
}

void ApplyShoutMode(SettingsData& a_data) {
    switch (a_data.shoutMode) {
        case Settings::ShoutMode::kVanilla:
            a_data.shoutPassThrough = false;
            a_data.staggerDefenderOnBreak = false;
            a_data.shoutInstantBreak = false;
            a_data.shoutDamage = 0.0f;
            break;

        case Settings::ShoutMode::kBreakOnly:
            a_data.shoutPassThrough = false;
            a_data.staggerDefenderOnBreak = false;
            break;

        case Settings::ShoutMode::kBreakWithStagger:
            a_data.shoutPassThrough = false;
            a_data.staggerDefenderOnBreak = true;
            break;

        case Settings::ShoutMode::kBreakWithPassThrough:
            a_data.shoutPassThrough = true;
            a_data.staggerDefenderOnBreak = false;
            break;
    }
}

void Normalize(SettingsData& a_data) {
    a_data.wardChargeRateMultiplier = ClampValue(
        a_data.wardChargeRateMultiplier,
        0.01f,
        10.0f,
        "fChargeRateMultiplier"sv
    );
    a_data.wardMagnitudeMultiplier = ClampValue(a_data.wardMagnitudeMultiplier, 0.01f, 10.0f, "fMagnitudeMultiplier"sv);
    a_data.wardCostMultiplier = ClampValue(a_data.wardCostMultiplier, 0.01f, 10.0f, "fCostMultiplier"sv);
    a_data.staggerMagnitude = ClampValue(a_data.staggerMagnitude, 0.1f, 1.0f, "fStaggerMagnitude"sv);
    a_data.blockingAngle = ClampValue(a_data.blockingAngle, 45.0f, 180.0f, "fBlockingAngle"sv);
    a_data.powerDamageMultiplier = ClampValue(a_data.powerDamageMultiplier, 0.0f, 10.0f, "fPowerDamageMultiplier"sv);
    a_data.blockXPScale = ClampValue(a_data.blockXPScale, 0.1f, 10.0f, "fBlockXPScale"sv);

    auto shoutMode = static_cast<std::int32_t>(a_data.shoutMode);
    shoutMode = ClampValue(shoutMode, 0, 3, "iShoutMode"sv);
    a_data.shoutMode = static_cast<Settings::ShoutMode>(shoutMode);
    a_data.shoutDamage = ClampValue(a_data.shoutDamage, 0.0f, 250.0f, "fShoutDamage"sv);
    a_data.staggerMagnitudeTowardDefender = ClampValue(
        a_data.staggerMagnitudeTowardDefender,
        0.1f,
        1.0f,
        "fStaggerMagnitudeTowardDefender"sv
    );

    a_data.cloakDamageMultiplier = ClampValue(a_data.cloakDamageMultiplier, 0.0f, 10.0f, "fCloakDamageMultiplier"sv);
    a_data.reflectionForwardOffset = ClampValue(
        a_data.reflectionForwardOffset,
        16.0f,
        256.0f,
        "fReflectionForwardOffset"sv
    );

    ApplyShoutMode(a_data);
}

void ReadSettingsFile(CSimpleIniA& a_ini, SettingsData& a_data, const ReadMode a_mode) {
    ReadValue(a_ini, a_data.showWardMeter, "General", "bShowWardMeter");
    ReadValue(a_ini, a_data.perkGatesPlayerOnly, "General", "bPerkGatesPlayerOnly");

    ReadValue(a_ini, a_data.instantWardCharge, "Tweaks", "bInstantCharge");
    ReadValue(a_ini, a_data.wardChargeRateMultiplier, "Tweaks", "fChargeRateMultiplier");
    ReadValue(a_ini, a_data.wardMagnitudeMultiplier, "Tweaks", "fMagnitudeMultiplier");
    ReadValue(a_ini, a_data.wardCostMultiplier, "Tweaks", "fCostMultiplier");
    ReadValue(a_ini, a_data.restrictTweaksToPlayerTeam, "Tweaks", "bRestrictToPlayerTeam");

    ReadValue(a_ini, a_data.staggerNormalAttacks, "Physical", "bStaggerNormalAttacks");
    ReadValue(a_ini, a_data.staggerMagnitude, "Physical", "fStaggerMagnitude");
    ReadValue(a_ini, a_data.staggerPowerAttacks, "Physical", "bStaggerPowerAttacks");
    ReadValue(a_ini, a_data.blockingAngle, "Physical", "fBlockingAngle");
    ReadValue(a_ini, a_data.powerDamageMultiplier, "Physical", "fPowerDamageMultiplier");
    ReadValue(a_ini, a_data.blockXPScale, "Physical", "fBlockXPScale");

    auto shoutMode = static_cast<std::int32_t>(a_data.shoutMode);
    ReadValue(a_ini, shoutMode, "Magic.Shouts", "iShoutMode");
    a_data.shoutMode = static_cast<Settings::ShoutMode>(shoutMode);
    ReadValue(a_ini, a_data.playerImmuneToShoutMechanics, "Magic.Shouts", "bPlayerImmuneToShoutMechanics");
    ReadValue(a_ini, a_data.shoutDamage, "Magic.Shouts", "fShoutDamage");
    ReadValue(a_ini, a_data.shoutInstantBreak, "Magic.Shouts", "bShoutInstantBreak");
    ReadValue(a_ini, a_data.staggerMagnitudeTowardDefender, "Magic.Shouts", "fStaggerMagnitudeTowardDefender");

    ReadValue(a_ini, a_data.blockDiseases, "Magic.Effects", "bBlockDiseases");
    ReadValue(a_ini, a_data.blockCloaks, "Magic.Effects", "bBlockCloaks");
    ReadValue(a_ini, a_data.cloakDamageMultiplier, "Magic.Effects", "fCloakDamageMultiplier");

    ReadValue(a_ini, a_data.enableSpellReflection, "Magic.Reflection", "bEnableSpellReflection");
    ReadValue(a_ini, a_data.autoAimReflection, "Magic.Reflection", "bAutoAimReflection");
    ReadValue(a_ini, a_data.reflectionBlameAttacker, "Magic.Reflection", "bReflectionBlameAttacker");
    ReadValue(a_ini, a_data.reflectionForwardOffset, "Magic.Reflection", "fReflectionForwardOffset");
    ReadValue(a_ini, a_data.reflectEvenIfWardBroken, "Magic.Reflection", "bReflectEvenIfWardBroken");
    ReadValue(a_ini, a_data.restrictReflectionToPlayerTeam, "Magic.Reflection", "bRestrictReflectionToPlayerTeam");

    ReadValue(
        a_ini,
        a_data.excludedItemsDisablePhysicalBlocking,
        "Exclusions",
        "bExcludedItemsDisablePhysicalBlocking"
    );
    ReadValue(a_ini, a_data.excludedItemsDisableShoutMechanics, "Exclusions", "bExcludedItemsDisableShoutMechanics");
    ReadValue(a_ini, a_data.excludedItemsDisableDiseaseBlocking, "Exclusions", "bExcludedItemsDisableDiseaseBlocking");
    ReadValue(a_ini, a_data.excludedItemsDisableCloakBlocking, "Exclusions", "bExcludedItemsDisableCloakBlocking");
    ReadValue(a_ini, a_data.excludedItemsDisableReflection, "Exclusions", "bExcludedItemsDisableReflection");

    if (a_mode == ReadMode::kLiveOnly) {
        Normalize(a_data);
        return;
    }

    ReadValue(a_ini, a_data.physicalRequiredPerks, "Physical", "sPhysicalRequiredPerks");
    ReadValue(a_ini, a_data.damageKeywords, "Magic.Shouts", "sDamageKeywords");
    ReadValue(a_ini, a_data.shoutsRequiredPerks, "Magic.Shouts", "sShoutsRequiredPerks");
    ReadValue(a_ini, a_data.diseaseSpells, "Magic.Effects", "sDiseaseSpells");
    ReadValue(a_ini, a_data.cloakSpells, "Magic.Effects", "sCloakSpells");
    ReadValue(a_ini, a_data.diseaseRequiredPerks, "Magic.Effects", "sDiseaseRequiredPerks");
    ReadValue(a_ini, a_data.cloakRequiredPerks, "Magic.Effects", "sCloakRequiredPerks");
    ReadValue(a_ini, a_data.reflectionRequiredPerks, "Magic.Reflection", "sReflectionRequiredPerks");
    ReadValue(a_ini, a_data.excludedItems, "Exclusions", "sExcludedItems");
    ReadValue(a_ini, a_data.shoutExclusions, "Exclusions", "sShoutExclusions");
    ReadValue(a_ini, a_data.reflectionExclusions, "Exclusions", "sReflectionExclusions");

    Normalize(a_data);
}

void WriteSettingsFile(CSimpleIniA& a_ini, const SettingsData& a_data) {
    WriteValue(
        a_ini,
        a_data.showWardMeter,
        "General",
        "bShowWardMeter",
        "; Show ward power meter on HUD while casting a ward spell.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.perkGatesPlayerOnly,
        "General",
        "bPerkGatesPlayerOnly",
        "; If true, perk requirements only apply to player; NPCs get features automatically.\n"
        "; Default: 0"
    );

    WriteValue(
        a_ini,
        a_data.instantWardCharge,
        "Tweaks",
        "bInstantCharge",
        "; Wards charge instantly to full power.\n"
        "; Ignores fChargeRateMultiplier.\n"
        "; Default: 0"
    );
    WriteValue(
        a_ini,
        a_data.wardChargeRateMultiplier,
        "Tweaks",
        "fChargeRateMultiplier",
        "; Ward charge speed multiplier.\n"
        "; Only applies when bInstantCharge is false.\n"
        "; Valid range: 0.01-10.0.\n"
        "; Default: 1.0"
    );
    WriteValue(
        a_ini,
        a_data.wardMagnitudeMultiplier,
        "Tweaks",
        "fMagnitudeMultiplier",
        "; Ward strength multiplier (max ward power).\n"
        "; Valid range: 0.01-10.0.\n"
        "; Default: 1.0"
    );
    WriteValue(
        a_ini,
        a_data.wardCostMultiplier,
        "Tweaks",
        "fCostMultiplier",
        "; Ward magicka cost multiplier.\n"
        "; Valid range: 0.01-10.0.\n"
        "; Default: 1.0"
    );
    WriteValue(
        a_ini,
        a_data.restrictTweaksToPlayerTeam,
        "Tweaks",
        "bRestrictToPlayerTeam",
        "; If true, tweaks only apply to player and teammates.\n"
        "; Default: 0"
    );

    WriteValue(
        a_ini,
        a_data.staggerNormalAttacks,
        "Physical",
        "bStaggerNormalAttacks",
        "; If true, attackers will stagger when their normal (non-power) melee attack is blocked by a ward.\n"
        "; Default: 0"
    );
    WriteValue(
        a_ini,
        a_data.staggerMagnitude,
        "Physical",
        "fStaggerMagnitude",
        "; Stagger magnitude applied when a normal attack is blocked by a ward.\n"
        "; Valid range: 0.1-1.0.\n"
        "; Default: 0.3"
    );
    WriteValue(
        a_ini,
        a_data.staggerPowerAttacks,
        "Physical",
        "bStaggerPowerAttacks",
        "; If true, attackers recoil when their power melee attack is blocked by a ward.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.blockingAngle,
        "Physical",
        "fBlockingAngle",
        "; Blocking cone angle for melee and ranged attacks. Spells use their own hit detection.\n"
        "; Valid range: 45.0-180.0.\n"
        "; Default: 90.0"
    );
    WriteValue(
        a_ini,
        a_data.powerDamageMultiplier,
        "Physical",
        "fPowerDamageMultiplier",
        "; Multiplier applied to physical damage before subtracting from ward power.\n"
        "; Set to 0 to prevent physical hits from draining ward power.\n"
        "; Valid range: 0.0-10.0.\n"
        "; Default: 1.0"
    );
    WriteValue(
        a_ini,
        a_data.blockXPScale,
        "Physical",
        "fBlockXPScale",
        "; Multiplier for Restoration XP gained when blocking with a ward.\n"
        "; Valid range: 0.1-10.0.\n"
        "; Default: 0.25"
    );
    WriteValue(
        a_ini,
        a_data.physicalRequiredPerks,
        "Physical",
        "sPhysicalRequiredPerks",
        "; Perks required for physical blocking (plugin|formID,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );

    WriteValue(
        a_ini,
        static_cast<std::int32_t>(a_data.shoutMode),
        "Magic.Shouts",
        "iShoutMode",
        "; 0 = Vanilla, 1 = BreakOnly, 2 = Break+Stagger, 3 = Break+PassThrough\n"
        "; Controls the overall ward vs shout behavior. Other shout values act as tuning knobs.\n"
        "; Valid range: 0-3.\n"
        "; Default: 3"
    );
    WriteValue(
        a_ini,
        a_data.playerImmuneToShoutMechanics,
        "Magic.Shouts",
        "bPlayerImmuneToShoutMechanics",
        "; If true, player wards behave like vanilla (absorb shouts completely).\n"
        "; Default: 0"
    );
    WriteValue(
        a_ini,
        a_data.shoutDamage,
        "Magic.Shouts",
        "fShoutDamage",
        "; Flat ward power damage from shouts.\n"
        "; Only used when bShoutInstantBreak is false.\n"
        "; Valid range: 0.0-250.0.\n"
        "; Default: 40.0"
    );
    WriteValue(
        a_ini,
        a_data.shoutInstantBreak,
        "Magic.Shouts",
        "bShoutInstantBreak",
        "; If true, shouts instantly break wards instead of dealing fixed damage.\n"
        "; Ignored in Vanilla mode.\n"
        "; Default: 0"
    );
    WriteValue(
        a_ini,
        a_data.staggerMagnitudeTowardDefender,
        "Magic.Shouts",
        "fStaggerMagnitudeTowardDefender",
        "; Stagger magnitude applied when the ward breaks from a shout in Break+Stagger mode.\n"
        "; Valid range: 0.1-1.0.\n"
        "; Default: 0.3"
    );
    WriteValue(
        a_ini,
        a_data.damageKeywords,
        "Magic.Shouts",
        "sDamageKeywords",
        "; Keywords treated as damaging for ward/shout logic.\n"
        "; Default: vanilla magic damage keywords"
    );
    WriteValue(
        a_ini,
        a_data.shoutsRequiredPerks,
        "Magic.Shouts",
        "sShoutsRequiredPerks",
        "; Perks required for shout mechanics (plugin|formID,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );

    WriteValue(
        a_ini,
        a_data.blockDiseases,
        "Magic.Effects",
        "bBlockDiseases",
        "; If true, wards can block disease spells when facing the source.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.blockCloaks,
        "Magic.Effects",
        "bBlockCloaks",
        "; If true, wards can block hostile cloak spell ticks when facing the source.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.cloakDamageMultiplier,
        "Magic.Effects",
        "fCloakDamageMultiplier",
        "; Multiplier applied to cloak effect magnitude when calculating ward power drain.\n"
        "; Set to 0 to block cloak ticks without draining ward power.\n"
        "; Valid range: 0.0-10.0.\n"
        "; Default: 1.0"
    );
    WriteValue(
        a_ini,
        a_data.diseaseSpells,
        "Magic.Effects",
        "sDiseaseSpells",
        "; Extra spells to treat as diseases. Format: plugin|formID,...\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.cloakSpells,
        "Magic.Effects",
        "sCloakSpells",
        "; Extra spells to treat as cloaks. Format: plugin|formID,...\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.diseaseRequiredPerks,
        "Magic.Effects",
        "sDiseaseRequiredPerks",
        "; Perks required for disease blocking (plugin|formID,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.cloakRequiredPerks,
        "Magic.Effects",
        "sCloakRequiredPerks",
        "; Perks required for cloak blocking (plugin|formID,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );

    WriteValue(
        a_ini,
        a_data.enableSpellReflection,
        "Magic.Reflection",
        "bEnableSpellReflection",
        "; If true, wards reflect incoming projectile spells back toward the attacker.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.autoAimReflection,
        "Magic.Reflection",
        "bAutoAimReflection",
        "; If true, reflected spells aim at the attacker. If false, spells reflect forward.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.reflectionBlameAttacker,
        "Magic.Reflection",
        "bReflectionBlameAttacker",
        "; If true, the original attacker is blamed for reflected spell damage.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.reflectionForwardOffset,
        "Magic.Reflection",
        "fReflectionForwardOffset",
        "; Spawn offset for reflected projectiles.\n"
        "; Valid range: 16.0-256.0.\n"
        "; Default: 48.0"
    );
    WriteValue(
        a_ini,
        a_data.reflectEvenIfWardBroken,
        "Magic.Reflection",
        "bReflectEvenIfWardBroken",
        "; If true, spells are reflected even when they break the ward.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.restrictReflectionToPlayerTeam,
        "Magic.Reflection",
        "bRestrictReflectionToPlayerTeam",
        "; If true, only player and follower wards reflect spells.\n"
        "; Default: 0"
    );
    WriteValue(
        a_ini,
        a_data.reflectionRequiredPerks,
        "Magic.Reflection",
        "sReflectionRequiredPerks",
        "; Perks required for reflection (plugin|formID,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );

    WriteValue(
        a_ini,
        a_data.excludedItems,
        "Exclusions",
        "sExcludedItems",
        "; Ward-casting items that should disable PerfectlyValidWards features.\n"
        "; Format: plugin|formID,plugin|formID\n"
        "; Example: Skyrim.esm|0x045F96 (Spellbreaker)\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.excludedItemsDisablePhysicalBlocking,
        "Exclusions",
        "bExcludedItemsDisablePhysicalBlocking",
        "; If true, excluded ward items disable physical ward mechanics.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.excludedItemsDisableShoutMechanics,
        "Exclusions",
        "bExcludedItemsDisableShoutMechanics",
        "; If true, excluded ward items disable shout vs ward mechanics.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.excludedItemsDisableDiseaseBlocking,
        "Exclusions",
        "bExcludedItemsDisableDiseaseBlocking",
        "; If true, excluded ward items disable disease blocking.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.excludedItemsDisableCloakBlocking,
        "Exclusions",
        "bExcludedItemsDisableCloakBlocking",
        "; If true, excluded ward items disable cloak blocking.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.excludedItemsDisableReflection,
        "Exclusions",
        "bExcludedItemsDisableReflection",
        "; If true, excluded ward items disable spell reflection.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.shoutExclusions,
        "Exclusions",
        "sShoutExclusions",
        "; Shouts excluded from ward mechanics. Word spells are excluded automatically.\n"
        "; Format: plugin|formID,...\n"
        "; Default: Skyrim.esm|0xE5F68"
    );
    WriteValue(
        a_ini,
        a_data.reflectionExclusions,
        "Exclusions",
        "sReflectionExclusions",
        "; Spells excluded from reflection (plugin|formID,...)\n"
        "; Default: empty"
    );
}

void SaveUserSettingsFile(const SettingsData& a_data) {
    const auto userPath = UserSettingsPath();
    std::filesystem::create_directories(userPath.parent_path());

    CSimpleIniA user;
    user.SetUnicode();
    if (std::filesystem::exists(userPath)) {
        if (const auto rc = user.LoadFile(userPath.string().c_str()); rc < 0) {
            logger::warn("Settings: failed to parse user settings | path={}", userPath.string());
        }
    }

    WriteSettingsFile(user, a_data);
    if (const auto rc = user.SaveFile(userPath.string().c_str()); rc < 0) {
        logger::warn("Settings: failed to save user settings | path={}", userPath.string());
    }
}

void LoadDefaultSettings(SettingsData& a_data, const ReadMode a_mode) {
    const auto defaultPath = DefaultSettingsPath();
    if (!std::filesystem::exists(defaultPath)) {
        return;
    }

    CSimpleIniA defaults;
    defaults.SetUnicode();
    if (const auto rc = defaults.LoadFile(defaultPath.string().c_str()); rc < 0) {
        logger::warn("Settings: failed to parse default settings | path={}", defaultPath.string());
        return;
    }

    ReadSettingsFile(defaults, a_data, a_mode);
}

[[nodiscard]] bool TryMigrateLegacySettings(SettingsData& a_data) {
    const auto legacyPath = std::filesystem::path {kLegacySettingsPath};
    if (!std::filesystem::exists(legacyPath)) {
        return false;
    }

    CSimpleIniA legacy;
    legacy.SetUnicode();
    if (const auto rc = legacy.LoadFile(legacyPath.string().c_str()); rc < 0) {
        logger::warn("Settings: failed to parse legacy settings | path={}", legacyPath.string());
        return false;
    }

    ReadSettingsFile(legacy, a_data, ReadMode::kAll);
    logger::info(
        "Settings: migrated legacy settings | from={} | to={}",
        legacyPath.string(),
        UserSettingsPath().string()
    );
    return true;
}

void LoadUserSettings(SettingsData& a_data, const ReadMode a_mode) {
    const auto userPath = UserSettingsPath();
    if (!std::filesystem::exists(userPath)) {
        if (a_mode == ReadMode::kAll && TryMigrateLegacySettings(a_data)) {
            return;
        }
        return;
    }

    CSimpleIniA user;
    user.SetUnicode();
    if (const auto rc = user.LoadFile(userPath.string().c_str()); rc < 0) {
        logger::warn("Settings: failed to parse user settings | path={}", userPath.string());
        return;
    }

    ReadSettingsFile(user, a_data, a_mode);
}

[[nodiscard]] SettingsData ReadSettings(const ReadMode a_mode) {
    SettingsData data;
    LoadDefaultSettings(data, a_mode);
    LoadUserSettings(data, a_mode);
    Normalize(data);
    if (a_mode == ReadMode::kAll) {
        SaveUserSettingsFile(data);
    }
    return data;
}

[[nodiscard]] bool LiveSettingsEqual(const Settings& a_settings, const SettingsData& a_data) {
    return a_settings.showWardMeter.load()
           == a_data.showWardMeter
           && a_settings.perkGatesPlayerOnly.load()
           == a_data.perkGatesPlayerOnly
           && a_settings.instantWardCharge.load()
           == a_data.instantWardCharge
           && a_settings.wardChargeRateMultiplier.load()
           == a_data.wardChargeRateMultiplier
           && a_settings.wardMagnitudeMultiplier.load()
           == a_data.wardMagnitudeMultiplier
           && a_settings.wardCostMultiplier.load()
           == a_data.wardCostMultiplier
           && a_settings.restrictTweaksToPlayerTeam.load()
           == a_data.restrictTweaksToPlayerTeam
           && a_settings.staggerNormalAttacks.load()
           == a_data.staggerNormalAttacks
           && a_settings.staggerMagnitude.load()
           == a_data.staggerMagnitude
           && a_settings.staggerPowerAttacks.load()
           == a_data.staggerPowerAttacks
           && a_settings.blockingAngle.load()
           == a_data.blockingAngle
           && a_settings.powerDamageMultiplier.load()
           == a_data.powerDamageMultiplier
           && a_settings.blockXPScale.load()
           == a_data.blockXPScale
           && a_settings.shoutMode.load()
           == a_data.shoutMode
           && a_settings.staggerDefenderOnBreak.load()
           == a_data.staggerDefenderOnBreak
           && a_settings.staggerMagnitudeTowardDefender.load()
           == a_data.staggerMagnitudeTowardDefender
           && a_settings.shoutPassThrough.load()
           == a_data.shoutPassThrough
           && a_settings.playerImmuneToShoutMechanics.load()
           == a_data.playerImmuneToShoutMechanics
           && a_settings.shoutDamage.load()
           == a_data.shoutDamage
           && a_settings.shoutInstantBreak.load()
           == a_data.shoutInstantBreak
           && a_settings.blockDiseases.load()
           == a_data.blockDiseases
           && a_settings.blockCloaks.load()
           == a_data.blockCloaks
           && a_settings.cloakDamageMultiplier.load()
           == a_data.cloakDamageMultiplier
           && a_settings.enableSpellReflection.load()
           == a_data.enableSpellReflection
           && a_settings.autoAimReflection.load()
           == a_data.autoAimReflection
           && a_settings.reflectionBlameAttacker.load()
           == a_data.reflectionBlameAttacker
           && a_settings.reflectionForwardOffset.load()
           == a_data.reflectionForwardOffset
           && a_settings.reflectEvenIfWardBroken.load()
           == a_data.reflectEvenIfWardBroken
           && a_settings.restrictReflectionToPlayerTeam.load()
           == a_data.restrictReflectionToPlayerTeam
           && a_settings.excludedItemsDisablePhysicalBlocking.load()
           == a_data.excludedItemsDisablePhysicalBlocking
           && a_settings.excludedItemsDisableShoutMechanics.load()
           == a_data.excludedItemsDisableShoutMechanics
           && a_settings.excludedItemsDisableDiseaseBlocking.load()
           == a_data.excludedItemsDisableDiseaseBlocking
           && a_settings.excludedItemsDisableCloakBlocking.load()
           == a_data.excludedItemsDisableCloakBlocking
           && a_settings.excludedItemsDisableReflection.load()
           == a_data.excludedItemsDisableReflection;
}

void ApplyLiveSettings(Settings& a_settings, const SettingsData& a_data) {
    a_settings.showWardMeter.store(a_data.showWardMeter);
    a_settings.perkGatesPlayerOnly.store(a_data.perkGatesPlayerOnly);

    a_settings.instantWardCharge.store(a_data.instantWardCharge);
    a_settings.wardChargeRateMultiplier.store(a_data.wardChargeRateMultiplier);
    a_settings.wardMagnitudeMultiplier.store(a_data.wardMagnitudeMultiplier);
    a_settings.wardCostMultiplier.store(a_data.wardCostMultiplier);
    a_settings.restrictTweaksToPlayerTeam.store(a_data.restrictTweaksToPlayerTeam);

    a_settings.staggerNormalAttacks.store(a_data.staggerNormalAttacks);
    a_settings.staggerMagnitude.store(a_data.staggerMagnitude);
    a_settings.staggerPowerAttacks.store(a_data.staggerPowerAttacks);
    a_settings.blockingAngle.store(a_data.blockingAngle);
    a_settings.powerDamageMultiplier.store(a_data.powerDamageMultiplier);
    a_settings.blockXPScale.store(a_data.blockXPScale);

    a_settings.shoutMode.store(a_data.shoutMode);
    a_settings.staggerDefenderOnBreak.store(a_data.staggerDefenderOnBreak);
    a_settings.staggerMagnitudeTowardDefender.store(a_data.staggerMagnitudeTowardDefender);
    a_settings.shoutPassThrough.store(a_data.shoutPassThrough);
    a_settings.playerImmuneToShoutMechanics.store(a_data.playerImmuneToShoutMechanics);
    a_settings.shoutDamage.store(a_data.shoutDamage);
    a_settings.shoutInstantBreak.store(a_data.shoutInstantBreak);

    a_settings.blockDiseases.store(a_data.blockDiseases);
    a_settings.blockCloaks.store(a_data.blockCloaks);
    a_settings.cloakDamageMultiplier.store(a_data.cloakDamageMultiplier);

    a_settings.enableSpellReflection.store(a_data.enableSpellReflection);
    a_settings.autoAimReflection.store(a_data.autoAimReflection);
    a_settings.reflectionBlameAttacker.store(a_data.reflectionBlameAttacker);
    a_settings.reflectionForwardOffset.store(a_data.reflectionForwardOffset);
    a_settings.reflectEvenIfWardBroken.store(a_data.reflectEvenIfWardBroken);
    a_settings.restrictReflectionToPlayerTeam.store(a_data.restrictReflectionToPlayerTeam);

    a_settings.excludedItemsDisablePhysicalBlocking.store(a_data.excludedItemsDisablePhysicalBlocking);
    a_settings.excludedItemsDisableShoutMechanics.store(a_data.excludedItemsDisableShoutMechanics);
    a_settings.excludedItemsDisableDiseaseBlocking.store(a_data.excludedItemsDisableDiseaseBlocking);
    a_settings.excludedItemsDisableCloakBlocking.store(a_data.excludedItemsDisableCloakBlocking);
    a_settings.excludedItemsDisableReflection.store(a_data.excludedItemsDisableReflection);
}

void ApplyStaticSettings(Settings& a_settings, const SettingsData& a_data) {
    a_settings.physicalRequiredPerks = a_data.physicalRequiredPerks;
    a_settings.damageKeywords = a_data.damageKeywords;
    a_settings.shoutsRequiredPerks = a_data.shoutsRequiredPerks;
    a_settings.diseaseSpells = a_data.diseaseSpells;
    a_settings.cloakSpells = a_data.cloakSpells;
    a_settings.diseaseRequiredPerks = a_data.diseaseRequiredPerks;
    a_settings.cloakRequiredPerks = a_data.cloakRequiredPerks;
    a_settings.reflectionRequiredPerks = a_data.reflectionRequiredPerks;
    a_settings.excludedItems = a_data.excludedItems;
    a_settings.shoutExclusions = a_data.shoutExclusions;
    a_settings.reflectionExclusions = a_data.reflectionExclusions;
}
}

void Settings::Load() {
    const auto data = ReadSettings(ReadMode::kAll);
    ApplyLiveSettings(*this, data);
    ApplyStaticSettings(*this, data);

    logger::info(
        "Settings: loaded | path={} | shoutMode={} | physicalDamageMultiplier={} | reflectionEnabled={}",
        UserSettingsPath().string(),
        std::to_underlying(shoutMode.load()),
        powerDamageMultiplier.load(),
        enableSpellReflection.load()
    );
}

Settings::ReloadResult Settings::Reload() {
    const auto data = ReadSettings(ReadMode::kLiveOnly);
    const auto changed = !LiveSettingsEqual(*this, data);
    ApplyLiveSettings(*this, data);

    logger::info(
        "Settings: reloaded | changed={} | path={} | physicalDamageMultiplier={}",
        changed,
        UserSettingsPath().string(),
        powerDamageMultiplier.load()
    );
    return {.changed = changed};
}

void Settings::ResolveRuntimeData() {
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
