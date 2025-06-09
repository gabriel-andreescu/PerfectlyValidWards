// ReSharper disable CppUnusedIncludeDirective
#pragma once

#include "RE/Skyrim.h"
#include "REL/Relocation.h"
#include "REX/REX/Singleton.h"
#include "SKSE/SKSE.h"

#include "Plugin.h"

#include <ClibUtil/simpleINI.hpp>
#include <xbyak/xbyak.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

using namespace std::literals;

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
    }

    auto add_thread_task(const std::function<void()>& a_fn, const detail::is_duration auto a_wait_for) noexcept
    {
        std::jthread{[=] {
            std::this_thread::sleep_for(a_wait_for);
            SKSE::GetTaskInterface()->AddTask(a_fn);
        }}.detach();
    }
}

#define DLLEXPORT __declspec(dllexport)
