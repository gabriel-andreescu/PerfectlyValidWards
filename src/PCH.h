#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

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

    // "Skyrim.esm|0x123"   ->  {"Skyrim.esm", 0x123}
    [[nodiscard]] inline std::optional<std::pair<std::string, std::uint32_t>> parse_plugin_form(std::string_view line) {
        const auto bar = line.find('|');
        if (bar == std::string_view::npos) {
            return std::nullopt;
        }

        std::string plugin {line.substr(0, bar)};
        std::string_view hex = line.substr(bar + 1);

        if (hex.starts_with("0x") || hex.starts_with("0X")) {
            hex.remove_prefix(2);
        }

        std::uint32_t id {};
        const char* first = std::to_address(hex.begin());
        const char* last = std::to_address(hex.end());
        const auto [_, ec] = std::from_chars(first, last, id, 16);
        if (ec != std::errc {}) {
            return std::nullopt;
        }

        return std::make_pair(std::move(plugin), id);
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

[[nodiscard]] inline bool has_all_required_perks(
    const RE::Actor* a_actor,
    std::span<RE::BGSPerk* const> a_perks
) noexcept {
    if (a_perks.empty()) {
        return true;
    }
    if (!a_actor) {
        return false;
    }
    return std::ranges::all_of(a_perks, [a_actor](RE::BGSPerk* a_perk) {
        return a_actor->HasPerk(a_perk);
    });
}
}
