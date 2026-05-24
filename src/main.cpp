#include "EventListener.h"
#include "FormCache.h"
#include "Hooks.h"
#include "Patches.h"
#include "Settings.h"
#include "Tweaks.h"
#include "WardMeter.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

constexpr auto kTrampolineSize = 128;

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
    if (a_msg->type == SKSE::MessagingInterface::kPostLoad) {
        Settings::GetSingleton()->Load();
    }
    if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
        Settings::GetSingleton()->ResolveRuntimeData();
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            logger::critical("TESDataHandler unavailable at kDataLoaded");
            return;
        }
        FormCache::GetSingleton()->Initialize(*data, *Settings::GetSingleton());
        Patches::PatchCollisionLayers();
        Patches::PatchImpactDataSets();
        Patches::PatchHitGrunts();
        Hooks::Install();
        EventListener::Register();
        WardMeter::InstallHook();
        Tweaks::InstallHooks();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    std::shared_ptr<spdlog::sinks::sink> sink;
    if (IsDebuggerPresent()) {
        sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    } else {
        // ReSharper disable once CppLocalVariableMayBeConst
        auto path = SKSE::log::log_directory();
        if (!path) {
            stl::report_and_fail("Failed to find standard logging directory"sv);
        }
        const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
        *path /= std::format("{}.log", plugin->GetName());
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

    const auto* msg = SKSE::GetMessagingInterface();
    if (!msg) {
        logger::critical("Failed to obtain Messaging Interface");
        return false;
    }

    msg->RegisterListener(MessageHandler);
    return true;
}
