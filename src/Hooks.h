#pragma once

namespace Hooks
{
    void Install() noexcept;

    class MyHook : REX::Singleton<MyHook>
    {
    public:
        static void Thunk() noexcept;

        inline static REL::Relocation<decltype(Thunk)> func;
    };
}
