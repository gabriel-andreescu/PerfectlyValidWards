#include "SettingsFile.h"
#include "SettingsData.h"

#include <BMK/Settings.h>
#include <CLIBUtil/distribution.hpp>
#include <ClibUtil/detail/SimpleIni.h>
#include <ClibUtil/simpleINI.hpp>
#include <spdlog/spdlog.h>
#include <string_view>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
constexpr std::string_view kModName = "PerfectlyValidWards";
constexpr std::string_view kConfigRoot = "MCM/Config";
constexpr std::string_view kSettingsRoot = "MCM/Settings";

[[nodiscard]] std::filesystem::path DefaultSettingsPath(const std::filesystem::path& a_dataRoot) {
    return a_dataRoot / kConfigRoot / kModName / "settings.ini";
}

[[nodiscard]] std::filesystem::path UserSettingsPath(const std::filesystem::path& a_dataRoot) {
    return a_dataRoot / kSettingsRoot / std::format("{}.ini", kModName);
}

[[nodiscard]] std::string EncodePluginForms(const std::vector<std::pair<std::string, std::uint32_t>>& a_value) {
    std::string result;
    for (const auto& [plugin, formID] : a_value) {
        if (!result.empty()) {
            result.append(",");
        }
        result.append(std::format("0x{:X}~{}", formID, plugin));
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
        auto entry = clib_util::string::trim_copy(rawEntry);
        if (entry.empty()) {
            continue;
        }

        try {
            auto record = clib_util::distribution::get_record(entry);
            auto* form = std::get_if<clib_util::distribution::formid_pair>(&record);
            if (form != nullptr && form->first && form->second) {
                parsed.emplace_back(std::move(form->second.value()), form->first.value());
                continue;
            }
        } catch (const std::exception& error) {
            spdlog::warn(
                "Settings: ignored invalid form entry | key={} | value='{}' | reason={}",
                a_key,
                entry,
                error.what()
            );
            continue;
        }
        spdlog::warn("Settings: ignored invalid form entry | key={} | value='{}'", a_key, entry);
    }

    return parsed;
}

void ReadStringList(CSimpleIniA& a_ini, std::vector<std::string>& a_value, const char* a_section, const char* a_key) {
    auto raw = std::string {a_value | std::views::join_with(',') | std::ranges::to<std::string>()};
    clib_util::ini::get_value(a_ini, raw, a_section, a_key);

    std::string_view delimiter = ",";
    if (!raw.contains(',') && raw.contains('|')) {
        delimiter = "|";
    }

    a_value.clear();
    auto remaining = std::string_view {raw};
    while (!remaining.empty()) {
        const auto pos = remaining.find(delimiter);
        const auto entry = pos == std::string_view::npos ? remaining : remaining.substr(0, pos);
        if (auto trimmed = clib_util::string::trim_copy(std::string {entry}); !trimmed.empty()) {
            a_value.emplace_back(std::move(trimmed));
        }

        if (pos == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(pos + delimiter.size());
    }
}

void ReadPluginForms(
    CSimpleIniA& a_ini,
    std::vector<std::pair<std::string, std::uint32_t>>& a_value,
    const char* a_section,
    const char* a_key
) {
    auto raw = EncodePluginForms(a_value);
    clib_util::ini::get_value(a_ini, raw, a_section, a_key);

    a_value = ParsePluginForms(clib_util::distribution::split_entry(raw), a_key);
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
        a_ini.SetValue(
            a_section,
            a_key,
            (a_value | std::views::join_with(std::string_view {a_delimiter}) | std::ranges::to<std::string>()).c_str(),
            a_comment
        );
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
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(a_value)) {
            throw std::invalid_argument(std::format("{} must be finite", a_key));
        }
    }
    const auto clamped = std::clamp(a_value, a_min, a_max);
    if (clamped != a_value) {
        spdlog::warn("Settings: clamped | key={} | value={} | clamped={}", a_key, a_value, clamped);
    }
    return clamped;
}

void Normalize(SettingsData& a_data) {
    a_data
        .wardChargeRateMultiplier = ClampValue(a_data.wardChargeRateMultiplier, 0.01F, 10.0F, "fChargeRateMultiplier");
    a_data.wardMagnitudeMultiplier = ClampValue(a_data.wardMagnitudeMultiplier, 0.01F, 10.0F, "fMagnitudeMultiplier");
    a_data.wardCostMultiplier = ClampValue(a_data.wardCostMultiplier, 0.01F, 10.0F, "fCostMultiplier");
    a_data.staggerMagnitude = ClampValue(a_data.staggerMagnitude, 0.1F, 1.0F, "fStaggerMagnitude");
    a_data.blockingAngle = ClampValue(a_data.blockingAngle, 45.0F, 180.0F, "fBlockingAngle");
    a_data.powerDamageMultiplier = ClampValue(a_data.powerDamageMultiplier, 0.0F, 10.0F, "fPowerDamageMultiplier");
    a_data.blockXPScale = ClampValue(a_data.blockXPScale, 0.1F, 10.0F, "fBlockXPScale");

    auto shoutMode = static_cast<std::int32_t>(a_data.shoutMode);
    shoutMode = ClampValue(shoutMode, 0, 3, "iShoutMode");
    a_data.shoutMode = static_cast<ShoutMode>(shoutMode);
    a_data.shoutDamage = ClampValue(a_data.shoutDamage, 0.0F, 250.0F, "fShoutDamage");
    a_data.staggerMagnitudeTowardDefender = ClampValue(
        a_data.staggerMagnitudeTowardDefender,
        0.1F,
        1.0F,
        "fStaggerMagnitudeTowardDefender"
    );

    a_data.cloakDamageMultiplier = ClampValue(a_data.cloakDamageMultiplier, 0.0F, 10.0F, "fCloakDamageMultiplier");
    a_data.reflectionForwardOffset = ClampValue(
        a_data.reflectionForwardOffset,
        16.0F,
        256.0F,
        "fReflectionForwardOffset"
    );
}

void ReadGeneral(CSimpleIniA& a_ini, SettingsData& a_data) {
    clib_util::ini::get_value(
        a_ini,
        a_data.showWardMeter,
        "General",
        "bShowWardMeter",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.perkGatesPlayerOnly,
        "General",
        "bPerkGatesPlayerOnly",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.debugLogging,
        "General",
        "bDebugLogging",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
}

void ReadTweaks(CSimpleIniA& a_ini, SettingsData& a_data) {
    clib_util::ini::get_value(
        a_ini,
        a_data.instantWardCharge,
        "Tweaks",
        "bInstantCharge",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.instantWardCast,
        "Tweaks",
        "bInstantCast",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(a_ini, a_data.wardChargeRateMultiplier, "Tweaks", "fChargeRateMultiplier");
    clib_util::ini::get_value(a_ini, a_data.wardMagnitudeMultiplier, "Tweaks", "fMagnitudeMultiplier");
    clib_util::ini::get_value(a_ini, a_data.wardCostMultiplier, "Tweaks", "fCostMultiplier");
    clib_util::ini::get_value(
        a_ini,
        a_data.restrictTweaksToPlayerTeam,
        "Tweaks",
        "bRestrictToPlayerTeam",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
}

void ReadPhysical(CSimpleIniA& a_ini, SettingsData& a_data, const bool a_includeStartupSettings) {
    clib_util::ini::get_value(
        a_ini,
        a_data.blockMelee,
        "Physical",
        "bBlockMelee",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.blockArrows,
        "Physical",
        "bBlockArrows",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.staggerNormalAttacks,
        "Physical",
        "bStaggerNormalAttacks",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(a_ini, a_data.staggerMagnitude, "Physical", "fStaggerMagnitude");
    clib_util::ini::get_value(
        a_ini,
        a_data.staggerPowerAttacks,
        "Physical",
        "bStaggerPowerAttacks",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(a_ini, a_data.blockingAngle, "Physical", "fBlockingAngle");
    clib_util::ini::get_value(a_ini, a_data.powerDamageMultiplier, "Physical", "fPowerDamageMultiplier");
    clib_util::ini::get_value(a_ini, a_data.blockXPScale, "Physical", "fBlockXPScale");
    if (a_includeStartupSettings) {
        ReadPluginForms(a_ini, a_data.physicalRequiredPerks, "Physical", "sPhysicalRequiredPerks");
    }
}

void ReadMagicShouts(CSimpleIniA& a_ini, SettingsData& a_data, const bool a_includeStartupSettings) {
    auto shoutMode = static_cast<std::int32_t>(a_data.shoutMode);
    clib_util::ini::get_value(a_ini, shoutMode, "Magic.Shouts", "iShoutMode");
    a_data.shoutMode = static_cast<ShoutMode>(ClampValue(shoutMode, 0, 3, "iShoutMode"));
    clib_util::ini::get_value(
        a_ini,
        a_data.playerImmuneToShoutMechanics,
        "Magic.Shouts",
        "bPlayerImmuneToShoutMechanics",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(a_ini, a_data.shoutDamage, "Magic.Shouts", "fShoutDamage");
    clib_util::ini::get_value(
        a_ini,
        a_data.shoutInstantBreak,
        "Magic.Shouts",
        "bShoutInstantBreak",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.staggerMagnitudeTowardDefender,
        "Magic.Shouts",
        "fStaggerMagnitudeTowardDefender"
    );
    if (a_includeStartupSettings) {
        ReadStringList(a_ini, a_data.damageKeywords, "Magic.Shouts", "sDamageKeywords");
        ReadPluginForms(a_ini, a_data.shoutsRequiredPerks, "Magic.Shouts", "sShoutsRequiredPerks");
    }
}

void ReadMagicEffects(CSimpleIniA& a_ini, SettingsData& a_data, const bool a_includeStartupSettings) {
    clib_util::ini::get_value(
        a_ini,
        a_data.blockDiseases,
        "Magic.Effects",
        "bBlockDiseases",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.blockCloaks,
        "Magic.Effects",
        "bBlockCloaks",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(a_ini, a_data.cloakDamageMultiplier, "Magic.Effects", "fCloakDamageMultiplier");
    if (a_includeStartupSettings) {
        ReadPluginForms(a_ini, a_data.diseaseSpells, "Magic.Effects", "sDiseaseSpells");
        ReadPluginForms(a_ini, a_data.cloakSpells, "Magic.Effects", "sCloakSpells");
        ReadPluginForms(a_ini, a_data.diseaseRequiredPerks, "Magic.Effects", "sDiseaseRequiredPerks");
        ReadPluginForms(a_ini, a_data.cloakRequiredPerks, "Magic.Effects", "sCloakRequiredPerks");
    }
}

void ReadMagicReflection(CSimpleIniA& a_ini, SettingsData& a_data, const bool a_includeStartupSettings) {
    clib_util::ini::get_value(
        a_ini,
        a_data.enableSpellReflection,
        "Magic.Reflection",
        "bEnableSpellReflection",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.autoAimReflection,
        "Magic.Reflection",
        "bAutoAimReflection",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.reflectionBlameAttacker,
        "Magic.Reflection",
        "bReflectionBlameAttacker",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(a_ini, a_data.reflectionForwardOffset, "Magic.Reflection", "fReflectionForwardOffset");
    clib_util::ini::get_value(
        a_ini,
        a_data.reflectEvenIfWardBroken,
        "Magic.Reflection",
        "bReflectEvenIfWardBroken",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.restrictReflectionToPlayerTeam,
        "Magic.Reflection",
        "bRestrictReflectionToPlayerTeam",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    if (a_includeStartupSettings) {
        ReadPluginForms(a_ini, a_data.reflectionRequiredPerks, "Magic.Reflection", "sReflectionRequiredPerks");
    }
}

void ReadExclusions(CSimpleIniA& a_ini, SettingsData& a_data, const bool a_includeStartupSettings) {
    clib_util::ini::get_value(
        a_ini,
        a_data.excludedItemsDisablePhysicalBlocking,
        "Exclusions",
        "bExcludedItemsDisablePhysicalBlocking",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.excludedItemsDisableShoutMechanics,
        "Exclusions",
        "bExcludedItemsDisableShoutMechanics",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.excludedItemsDisableDiseaseBlocking,
        "Exclusions",
        "bExcludedItemsDisableDiseaseBlocking",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.excludedItemsDisableCloakBlocking,
        "Exclusions",
        "bExcludedItemsDisableCloakBlocking",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_data.excludedItemsDisableReflection,
        "Exclusions",
        "bExcludedItemsDisableReflection",
        nullptr,
        clib_util::ini::bool_format::kNumeric
    );
    if (a_includeStartupSettings) {
        ReadPluginForms(a_ini, a_data.excludedItems, "Exclusions", "sExcludedItems");
        ReadPluginForms(a_ini, a_data.shoutExclusions, "Exclusions", "sShoutExclusions");
        ReadPluginForms(a_ini, a_data.reflectionExclusions, "Exclusions", "sReflectionExclusions");
    }
}

void ReadSettingsFile(CSimpleIniA& a_ini, SettingsData& a_data, const SettingsReadMode a_mode) {
    const auto includeStartupSettings = a_mode == SettingsReadMode::kAll;
    ReadGeneral(a_ini, a_data);
    ReadTweaks(a_ini, a_data);
    ReadPhysical(a_ini, a_data, includeStartupSettings);
    ReadMagicShouts(a_ini, a_data, includeStartupSettings);
    ReadMagicEffects(a_ini, a_data, includeStartupSettings);
    ReadMagicReflection(a_ini, a_data, includeStartupSettings);
    ReadExclusions(a_ini, a_data, includeStartupSettings);
}

void WriteGeneral(CSimpleIniA& a_ini, const SettingsData& a_data) {
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
        "; If true, perk requirements only apply to the player. NPCs do not need these perks.\n"
        "; Default: 0"
    );
    WriteValue(
        a_ini,
        a_data.debugLogging,
        "General",
        "bDebugLogging",
        "; Enable debug logging.\n"
        "; Default: 0"
    );
}

void WriteTweaks(CSimpleIniA& a_ini, const SettingsData& a_data) {
    WriteValue(
        a_ini,
        a_data.instantWardCast,
        "Tweaks",
        "bInstantCast",
        "; Skip spell charge-up and the casting animation delay before a ward activates.\n"
        "; Ward power charging is controlled separately by bInstantCharge.\n"
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
}

void WritePhysical(CSimpleIniA& a_ini, const SettingsData& a_data) {
    WriteValue(
        a_ini,
        a_data.blockMelee,
        "Physical",
        "bBlockMelee",
        "; If true, wards block melee attacks, including unarmed attacks and bashes.\n"
        "; Default: 1"
    );
    WriteValue(
        a_ini,
        a_data.blockArrows,
        "Physical",
        "bBlockArrows",
        "; If true, wards block arrows and crossbow bolts.\n"
        "; Default: 1"
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
        "; Perks required for physical blocking (0xFORMID~Plugin.esp,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );
}

void WriteMagicShouts(CSimpleIniA& a_ini, const SettingsData& a_data) {
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
        "; Perks required for shout mechanics (0xFORMID~Plugin.esp,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );
}

void WriteMagicEffects(CSimpleIniA& a_ini, const SettingsData& a_data) {
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
        "; Extra spells to treat as diseases. Format: 0xFORMID~Plugin.esp,...\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.cloakSpells,
        "Magic.Effects",
        "sCloakSpells",
        "; Extra spells to treat as cloaks. Format: 0xFORMID~Plugin.esp,...\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.diseaseRequiredPerks,
        "Magic.Effects",
        "sDiseaseRequiredPerks",
        "; Perks required for disease blocking (0xFORMID~Plugin.esp,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );
    WriteValue(
        a_ini,
        a_data.cloakRequiredPerks,
        "Magic.Effects",
        "sCloakRequiredPerks",
        "; Perks required for cloak blocking (0xFORMID~Plugin.esp,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );
}

void WriteMagicReflection(CSimpleIniA& a_ini, const SettingsData& a_data) {
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
        "; Perks required for reflection (0xFORMID~Plugin.esp,...).\n"
        "; Empty = always enabled.\n"
        "; Default: empty"
    );
}

void WriteExclusions(CSimpleIniA& a_ini, const SettingsData& a_data) {
    WriteValue(
        a_ini,
        a_data.excludedItems,
        "Exclusions",
        "sExcludedItems",
        "; Ward-casting items that should disable PerfectlyValidWards features.\n"
        "; Format: 0xFORMID~Plugin.esp,...\n"
        "; Example: 0x045F96~Skyrim.esm (Spellbreaker)\n"
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
        "; Format: 0xFORMID~Plugin.esp,...\n"
        "; Default: 0xE5F68~Skyrim.esm"
    );
    WriteValue(
        a_ini,
        a_data.reflectionExclusions,
        "Exclusions",
        "sReflectionExclusions",
        "; Spells excluded from reflection (0xFORMID~Plugin.esp,...)\n"
        "; Default: empty"
    );
}

void WriteSettingsFile(CSimpleIniA& a_ini, const SettingsData& a_data) {
    WriteGeneral(a_ini, a_data);
    WriteTweaks(a_ini, a_data);
    WritePhysical(a_ini, a_data);
    WriteMagicShouts(a_ini, a_data);
    WriteMagicEffects(a_ini, a_data);
    WriteMagicReflection(a_ini, a_data);
    WriteExclusions(a_ini, a_data);
}

}

std::optional<SettingsData> ReadSettings(const std::filesystem::path& a_dataRoot, const SettingsReadMode a_mode) {
    auto initialValues = SettingsData {};
    auto loaded = BMK::Settings::Load(
        {
            .defaults = DefaultSettingsPath(a_dataRoot),
            .user = UserSettingsPath(a_dataRoot),
        },
        std::move(initialValues),
        [a_mode](CSimpleIniA& a_defaults, CSimpleIniA& a_user, SettingsData& a_data) {
            ReadSettingsFile(a_defaults, a_data, a_mode);
            ReadSettingsFile(a_user, a_data, a_mode);
            Normalize(a_data);
            if (a_mode == SettingsReadMode::kAll) {
                WriteSettingsFile(a_user, a_data);
            }
        },
        a_mode == SettingsReadMode::kAll ? BMK::Settings::SaveUserFile::kYes : BMK::Settings::SaveUserFile::kNo
    );
    if (!loaded) {
        spdlog::warn("Settings: {}", loaded.error().message);
        return std::nullopt;
    }
    if (loaded->saveFailure) {
        spdlog::warn("Settings: {}", loaded->saveFailure->message);
    }
    return std::move(loaded->values);
}
