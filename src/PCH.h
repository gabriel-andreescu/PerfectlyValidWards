// ReSharper disable CppUnusedIncludeDirective
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI

#include <RE/Skyrim.h>
#include <REX/REX/Singleton.h>
#include <SKSE/SKSE.h>

#include <CLIBUtil/simpleINI.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;

using namespace std::literals;

namespace stl {
using namespace SKSE::stl;

namespace detail {
    template <typename>
    struct is_chrono_duration : std::false_type {};

    template <typename Rep, typename Period>
    struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

    template <typename T>
    concept is_duration = is_chrono_duration<T>::value;

    [[nodiscard]] inline bool initialize_sound(RE::BSSoundHandle& a_handle, const std::string& a_editorID) {
        auto* const man = RE::BSAudioManager::GetSingleton();
        man->GetSoundHandleByName(a_handle, a_editorID.c_str(), 16);
        return a_handle.IsValid();
    }

    inline RE::BSSoundHandle make_sound_handle() {
        RE::BSSoundHandle handle;
        handle.soundID = static_cast<std::uint32_t>(-1);
        handle.assumeSuccess = false;
        *reinterpret_cast<std::uint32_t*>(&handle.state) = 0;
        return handle;
    }
}

template <typename T, std::size_t Size = 5>
void write_thunk_call(REL::Relocation<> a_target) noexcept {
    T::func = a_target.write_call<Size>(T::thunk);
}

template <typename T>
void write_vfunc(const REL::VariantID a_variant_id) noexcept {
    REL::Relocation target {a_variant_id};
    T::func = target.write_vfunc(T::idx, T::thunk);
}

template <typename TDest, typename TSource>
void write_vfunc(const std::size_t a_vtableIdx = 0) noexcept {
    write_vfunc<TSource>(TDest::VTABLE[a_vtableIdx]);
}

auto add_thread_task(const std::function<void()>& a_fn, const detail::is_duration auto a_wait_for) noexcept {
    std::jthread {[=] {
        std::this_thread::sleep_for(a_wait_for);
        SKSE::GetTaskInterface()->AddTask(a_fn);
    }}.detach();
}

inline bool play_sound(const RE::Actor* a_actor, const std::string& a_editorID, const float a_volume = 1.f) {
    auto handle = detail::make_sound_handle();

    if (!detail::initialize_sound(handle, a_editorID)) {
        return false;
    }

    if (!handle.SetPosition(a_actor->GetPosition())) {
        return false;
    }

    handle.SetVolume(a_volume);

    if (auto* actor3D = a_actor->Get3D()) {
        handle.SetObjectToFollow(actor3D);
    }

    handle.Play();
    return handle.IsPlaying();
}
}
