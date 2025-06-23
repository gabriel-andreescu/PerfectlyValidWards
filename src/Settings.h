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
            maxCarryWeight,
            "General",
            "fMaxCarryWeight",
            "; Freeze player's carry weight to this number (0 to disable)"
        );

        (void) ini.SaveFile(path.c_str());
    }

    // members
    float maxCarryWeight{ 1'000'000.0f };
};
