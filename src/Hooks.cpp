#include "Hooks.h"

namespace Hooks
{
    void Install() noexcept
    {
        //const REL::Relocation MyHook_Loc{RELOCATION_ID(34175, 34968), REL::Relocate(0x5D, 0x5D)};
        //stl::write_thunk_call<MyHook>(MyHook_Loc);
    }

    void MyHook::Thunk() noexcept
    {
        func();
    }
}
