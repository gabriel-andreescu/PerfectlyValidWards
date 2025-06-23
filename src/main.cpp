#include "EventListener.h"
#include "Hooks.h"
#include "Settings.h"

constexpr auto kTrampolineSize = 48;

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
    if (a_msg->type == SKSE::MessagingInterface::kPostLoad) {
        Settings::GetSingleton()->Load();
    }
    if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
        Hooks::Install();
        EventListener::Register();
    }
};

SKSEPluginInfo(
    .Version = Plugin::VERSION,
    .Name = Plugin::NAME.data(),
    .Author = "GabonZ",
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
);

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    std::shared_ptr<spdlog::sinks::sink> sink;
    if (IsDebuggerPresent()) {
        sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    } else {
        auto path = SKSE::log::log_directory();
        if (!path) {
            stl::report_and_fail("Failed to find standard logging directory"sv);
        }
        *path /= std::format("{}.log", Plugin::NAME);
        sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    }

    auto logger = std::make_shared<spdlog::logger>("Global", std::move(sink));
    logger->set_level(
#ifdef NDEBUG
        spdlog::level::info
#else
        spdlog::level::debug
#endif
    );
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");

    SKSE::Init(a_skse, false);
    SKSE::AllocTrampoline(kTrampolineSize);

    auto* msg = SKSE::GetMessagingInterface();
    if (!msg) {
        logger::critical("Failed to obtain Messaging Interface");
        return false;
    }

    msg->RegisterListener(MessageHandler);
    return true;
}
