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
        : root_(
              std::filesystem::temp_directory_path()
              / ("pvw-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))
          ) {
        if (!std::filesystem::create_directory(root_)) {
            throw std::runtime_error("Could not create settings test directory: " + root_.string());
        }
    }
    ~ConfigurationDirectory() {
        try {
            std::filesystem::remove_all(root_);
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

    void Defaults(std::string_view text) const {
        Write(root_ / "MCM/Config/PerfectlyValidWards/settings.ini", text);
    }
    void User(std::string_view text) const {
        Write(root_ / "MCM/Settings/PerfectlyValidWards.ini", text);
    }
    [[nodiscard]] std::optional<SettingsData> Read(SettingsReadMode mode = SettingsReadMode::kAll) const {
        return ReadSettings(root_, mode);
    }
    [[nodiscard]] std::string UserText() const {
        std::ifstream file(root_ / "MCM/Settings/PerfectlyValidWards.ini");
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

private:
    static void Write(const std::filesystem::path& path, std::string_view text) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
        if (!file.good()) {
            throw std::runtime_error("Could not write settings fixture: " + path.string());
        }
    }
    std::filesystem::path root_;
};
}

TEST_CASE("Invalid form entries do not remove valid configured forms") {
    const ConfigurationDirectory files;
    files.User("[Physical]\nsPhysicalRequiredPerks=Skyrim.esm, 0x456~Skyrim.esm\n");
    const auto settings = files.Read();
    REQUIRE(settings);
    REQUIRE(settings->physicalRequiredPerks.size() == 1);
    CHECK(settings->physicalRequiredPerks.front().first == "Skyrim.esm");
    CHECK(settings->physicalRequiredPerks.front().second == 0x456);
}

TEST_CASE("Reload reads live values without rewriting the user file or importing startup lists") {
    const ConfigurationDirectory files;
    files.User("[Tweaks]\nfMagnitudeMultiplier=3\n[Physical]\nsPhysicalRequiredPerks=0x123~Skyrim.esm\n");
    const auto before = files.UserText();
    const auto data = files.Read(SettingsReadMode::kLiveOnly);
    REQUIRE(data);
    CHECK(data->wardMagnitudeMultiplier == 3);
    CHECK(data->physicalRequiredPerks.empty());
    CHECK(files.UserText() == before);
}
