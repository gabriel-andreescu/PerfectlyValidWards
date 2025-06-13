// ReSharper disable CppDFAConstantParameter
#pragma once

class Settings : public REX::Singleton<Settings>
{
public:
    void Load()
    {
        const auto path = std::format("Data/SKSE/Plugins/{}.ini", Plugin::NAME);

        CSimpleIniA ini;
        ini.SetUnicode();

        ini.LoadFile(path.c_str());

        get_value(ini, debugLogging, "General", "bDebugLogging", "; Enable debug logging");

        (void)ini.SaveFile(path.c_str());
    }

    // members
    bool debugLogging{false};

private:
    template <class T>
    static void get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key, const char* a_comment)
    {
        clib_util::ini::get_value(a_ini, a_value, a_section, a_key, a_comment);
    }

    static void get_value(CSimpleIniA& a_ini, std::vector<std::pair<std::string, uint32_t>>& a_value, const char* a_section, const char* a_key, const char* a_comment)
    {
        std::vector<std::string> raw;
        raw.reserve(a_value.size());
        for (auto&& [plugin, id] : a_value)
            raw.emplace_back(std::format("{}|0x{:X}", plugin, id));

        clib_util::ini::get_value(a_ini, raw, a_section, a_key, a_comment, ",");

        a_value.clear();
        for (auto&& entry : raw)
            if (auto parsed = stl::detail::parse_plugin_form(entry))
                a_value.emplace_back(std::move(*parsed));
    }
};
