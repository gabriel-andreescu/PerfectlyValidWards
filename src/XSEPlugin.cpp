#include "Settings.h"

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void MessageHandler(SKSE::MessagingInterface::Message* message)
{
    switch (message->type) {
        // ---------- Skyrim Lifecycle Events ----------

        // Called after all plugins have finished running SKSEPlugin_Load.
        // Safe to initialize threads or interact with other plugins.
    case SKSE::MessagingInterface::kPostLoad:    // NOLINT(*-branch-clone)
        break;

        // Called after all kPostLoad handlers are done.
    case SKSE::MessagingInterface::kPostPostLoad:
        break;

        // Called once all game data has been loaded.
    case SKSE::MessagingInterface::kInputLoaded:
        break;

        // Called when all ESM/ESL/ESP plugins have loaded and the main menu is active.
        // Safe to access game form data.
    case SKSE::MessagingInterface::kDataLoaded:
        break;

        // ---------- Skyrim Game Events ----------

        // Player starts a new game from the main menu.
    case SKSE::MessagingInterface::kNewGame:
        break;

        // Player selected a save, but it hasn't loaded yet.
        // 'Data' holds the name of the loaded save.
    case SKSE::MessagingInterface::kPreLoadGame:
        break;

        // Called after the selected save game finishes loading.
        // 'Data' indicates whether the load was successful.
    case SKSE::MessagingInterface::kPostLoadGame:
        break;

        // Called when the player saves the game.
        // 'Data' is the save name.
    case SKSE::MessagingInterface::kSaveGame:
        break;

        // Called when a saved game is deleted from the load menu.
    case SKSE::MessagingInterface::kDeleteGame:
        break;
    default:;
    }
}

SKSEPluginInfo(
        .Version = Plugin::VERSION,
        .Name = Plugin::NAME.data(),
        .Author = "GabonZ",
        .StructCompatibility = SKSE::StructCompatibility::Independent,
        .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary);

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
#ifdef WAIT_FOR_DEBUGGER
    // Pause execution until the debugger is attached.
    while (!IsDebuggerPresent()) Sleep(1000);
#endif

    std::shared_ptr<spdlog::logger> log;

    if (IsDebuggerPresent()) {
        // Use MSVC sink for debug output.
        log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
    } else {
        // Set up file logging.
        auto path = SKSE::log::log_directory();
        if (!path) {
            stl::report_and_fail("Failed to find standard logging directory"sv);
        }

        *path /= std::format("{}.log", Plugin::NAME);
        log = std::make_shared<spdlog::logger>(
            "Global",
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
    }

#ifdef NDEBUG
    spdlog::level::level_enum logLevel = spdlog::level::info;
#else
    spdlog::level::level_enum logLevel = spdlog::level::debug;
#endif

    bool loadError = false;
    try {
        const auto settings = Settings::GetSingleton();
        settings->Load();

        if (settings->debugLogging) {
            logLevel = spdlog::level::debug;
        }
    } catch (...) {
        loadError = true;
    }
    log->set_level(logLevel);
    log->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
    if (loadError) {
        logger::warn("Exception caught when loading settings! Default settings will be used");
    }

    SKSE::Init(a_skse, false);

    if (!SKSE::GetMessagingInterface()->RegisterListener(MessageHandler)) {
        stl::report_and_fail("Unable to register message listener.");
    }

    return true;
}
