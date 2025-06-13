// ReSharper disable CppUnusedIncludeDirective
#pragma once

#include "RE/Skyrim.h"
#include "REL/Relocation.h"
#include "REX/REX/Singleton.h"
#include "SKSE/SKSE.h"

#define WIN32_LEAN_AND_MEAN
#include <ClibUtil/simpleINI.hpp>
#include <chrono>
#include <shared_mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <unordered_map>
#include <windows.h>
#include <xbyak/xbyak.h>

#include "Plugin.h"

using namespace std::literals;
using namespace std::chrono_literals;

namespace logger = SKSE::log;

namespace stl
{
    using namespace SKSE::stl;

    template <typename T, std::size_t Size = 5>
    constexpr auto write_thunk_call(REL::Relocation<> a_target) noexcept
    {
        T::func = a_target.write_call<Size>(T::Thunk);
    }

    template <typename T, std::size_t Size = 5>
    constexpr auto write_thunk_jump(REL::Relocation<> a_target) noexcept
    {
        T::func = a_target.write_branch<Size>(T::Thunk);
    }

    template <typename T>
    constexpr auto write_vfunc(const REL::VariantID variant_id) noexcept
    {
        REL::Relocation vtbl{variant_id};
        T::func = vtbl.write_vfunc(T::idx, T::Thunk);
    }

    template <typename TDest, typename TSource>
    constexpr auto write_vfunc(const std::size_t a_vtableIdx = 0) noexcept
    {
        write_vfunc<TSource>(TDest::VTABLE[a_vtableIdx]);
    }

    namespace detail
    {
        template <typename>
        struct is_chrono_duration : std::false_type
        {};

        template <typename Rep, typename Period>
        struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type
        {};

        template <typename T>
        concept is_duration = is_chrono_duration<T>::value;

        // "Skyrim.esm|0x123"   ->  {"Skyrim.esm", 0x123}
        inline std::optional<std::pair<std::string, std::uint32_t>>
        parse_plugin_form(std::string_view line)
        {
            const auto bar = line.find('|');
            if (bar == std::string_view::npos)
                return std::nullopt;

            std::string plugin{line.substr(0, bar)};
            std::string_view hex = line.substr(bar + 1);

            if (hex.starts_with("0x") || hex.starts_with("0X"))
                hex.remove_prefix(2);

            std::uint32_t id{};
            const auto [_, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), id, 16);
            if (ec != std::errc{})
                return std::nullopt;

            return std::pair{std::move(plugin), id};
        }
    }

    auto add_thread_task(const std::function<void()>& a_fn, const detail::is_duration auto a_wait_for) noexcept
    {
        std::jthread{[=] {
            std::this_thread::sleep_for(a_wait_for);
            SKSE::GetTaskInterface()->AddTask(a_fn);
        }}.detach();
    }

    template <class T>
    [[nodiscard]] T* require_form(std::string_view pluginName, RE::FormID id, std::string_view what = {}, const std::source_location a_loc = std::source_location::current())
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (auto* form = dh ? dh->LookupForm<T>(id, pluginName) : nullptr) {
            return form;
        }

        report_and_fail(std::format("{} {}|0x{:06X} not found", what, pluginName, id), a_loc);
    }

    [[nodiscard]] inline bool has_all_required_perks(
        const RE::Actor* actor,
        std::span<RE::BGSPerk* const> perks) noexcept
    {
        if (perks.empty())
            return true;
        if (!actor)
            return false;

        return std::ranges::all_of(
            perks,
            [actor](RE::BGSPerk* perk) { return actor->HasPerk(perk); });
    }
}

#define DLLEXPORT __declspec(dllexport)
