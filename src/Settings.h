#pragma once

class Settings : public REX::Singleton<Settings> {
public:
    void Load() {
        using namespace clib_util::ini;

        const auto path = std::format("Data/SKSE/Plugins/{}.ini", Plugin::NAME);

        CSimpleIniA ini;
        ini.SetUnicode();

        ini.LoadFile(path.c_str());

        get_value(
            ini,
            staggerNormalAttacks,
            "General",
            "bStaggerNormalAttacks",
            "; If true, attackers will stagger when their normal (non-power) melee attack is blocked by a ward."
        );

        get_value(
            ini,
            staggerMagnitude,
            "General",
            "fStaggerMagnitude",
            "; Stagger magnitude applied when a normal attack is blocked by a ward."
        );
        staggerMagnitude = std::clamp(staggerMagnitude, 0.1f, 1.f);

        get_value(
            ini,
            staggerPowerAttacks,
            "General",
            "bStaggerPowerAttacks",
            "; If true, attackers will recoil when their power melee attack is blocked by a ward."
        );

        get_value(
            ini,
            wardBlockingAngle,
            "General",
            "fWardBlockingAngle",
            "; Maximum angle (in degrees) at which a ward can block incoming melee and ranged attacks (excluding spells)."
        );
        wardBlockingAngle = std::clamp(wardBlockingAngle, 45.f, 180.f);

        get_value(
            ini,
            wardPowerDamageMultiplier,
            "General",
            "fWardPowerDamageMultiplier",
            "; Multiplier applied to melee/arrow damage before subtracting from ward power."
        );
        wardPowerDamageMultiplier = std::abs(wardPowerDamageMultiplier);

        get_value(
            ini,
            wardBlockXPScale,
            "General",
            "fWardBlockXPScale",
            "; Multiplier for Restoration XP gained when blocking with a ward"
        );
        wardBlockXPScale = std::abs(std::clamp(wardBlockXPScale, 0.1f, 10.f));

        (void) ini.SaveFile(path.c_str());
    }

    // members
    static constexpr auto pluginName{ "PerfectlyValidWards.esp" };
    static constexpr auto skyrimESM{ "Skyrim.esm" };

    bool staggerNormalAttacks{ false };
    float staggerMagnitude{ 0.3f };
    bool staggerPowerAttacks{ true };
    float wardBlockingAngle{ 90.f };
    float wardPowerDamageMultiplier{ 1.f };
    float wardBlockXPScale{ 0.25f };
};
