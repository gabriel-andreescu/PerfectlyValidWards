#pragma once

#include "SettingsData.h"

#include <filesystem>
#include <optional>

enum class SettingsReadMode {
    kLiveOnly,
    kAll,
};

[[nodiscard]] std::optional<SettingsData> ReadSettings(
    const std::filesystem::path& a_dataRoot,
    SettingsReadMode a_mode
);
