#include "SettingsData.h"
#include "SettingsFile.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
class ConfigurationDirectory {
public:
    ConfigurationDirectory()
        : _root(
              std::filesystem::temp_directory_path()
              / ("pvw-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))
          ) {
        if (!std::filesystem::create_directory(_root)) {
            throw std::runtime_error("Could not create settings test directory: " + _root.string());
        }
    }
    ~ConfigurationDirectory() {
        try {
            std::filesystem::remove_all(_root);
        } catch (const std::exception& error) {
            std::fputs("Settings test cleanup failed: ", stderr);
            std::fputs(error.what(), stderr);
            std::fputc('\n', stderr);
        }
    }
    ConfigurationDirectory(const ConfigurationDirectory&) = delete;
    ConfigurationDirectory& operator=(const ConfigurationDirectory&) = delete;
    ConfigurationDirectory(ConfigurationDirectory&&) = delete;
    ConfigurationDirectory& operator=(ConfigurationDirectory&&) = delete;

    void Defaults(std::string_view a_text) const {
        Write(_root / "MCM/Config/PerfectlyValidWards/settings.ini", a_text);
    }
    void User(std::string_view a_text) const {
        Write(_root / "MCM/Settings/PerfectlyValidWards.ini", a_text);
    }
    // REQUIRE fails the test on unreadable settings before the value is used.
    [[nodiscard]] SettingsData Read(SettingsReadMode a_mode = SettingsReadMode::kAll) const {
        const auto settings = ReadSettings(_root, a_mode);
        REQUIRE(settings.has_value());
        return settings.value_or(SettingsData {});
    }
    [[nodiscard]] std::string UserText() const {
        std::ifstream file(_root / "MCM/Settings/PerfectlyValidWards.ini");
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

private:
    static void Write(const std::filesystem::path& a_path, std::string_view a_text) {
        std::filesystem::create_directories(a_path.parent_path());
        std::ofstream file(a_path);
        file << a_text;
        if (!file.good()) {
            throw std::runtime_error("Could not write settings fixture: " + a_path.string());
        }
    }
    std::filesystem::path _root;
};
}

TEST_CASE("Invalid form entries do not remove valid configured forms") {
    const ConfigurationDirectory files;
    files.User("[Physical]\nsPhysicalRequiredPerks=Skyrim.esm, 0x456~Skyrim.esm\n");
    const auto settings = files.Read();
    REQUIRE(settings.physicalRequiredPerks.size() == 1);
    CHECK(settings.physicalRequiredPerks.front().first == "Skyrim.esm");
    CHECK(settings.physicalRequiredPerks.front().second == 0x456);
}

TEST_CASE("Reload reads live values without rewriting the user file or importing startup lists") {
    const ConfigurationDirectory files;
    files.User("[Tweaks]\nfMagnitudeMultiplier=3\n[Physical]\nsPhysicalRequiredPerks=0x123~Skyrim.esm\n");
    const auto before = files.UserText();
    const auto data = files.Read(SettingsReadMode::kLiveOnly);
    CHECK(data.wardMagnitudeMultiplier == 3);
    CHECK(data.physicalRequiredPerks.empty());
    CHECK(files.UserText() == before);
}
