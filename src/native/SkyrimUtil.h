#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep

#include <SKSE/SKSE.h>

using namespace std::literals;

namespace stl {
using namespace SKSE::stl;

template <typename T, std::size_t Size = 5>
void WriteThunkCall(REL::Relocation<> a_target) noexcept {
    T::func = a_target.write_call<Size>(T::thunk);
}

template <typename T>
void WriteVFunc(const REL::VariantID a_variantId) {
    REL::Relocation target {a_variantId};
    T::func = target.write_vfunc(T::kIdx, T::thunk);
}

template <typename TDest, typename TSource>
void WriteVFunc(const std::size_t a_vtableIdx = 0) {
    WriteVFunc<TSource>(TDest::VTABLE[a_vtableIdx]);
}

inline bool PlaySound(const RE::Actor* a_actor, const std::string& a_editorID, const float a_volume = 1.F) {
    RE::BSSoundHandle handle;
    RE::BSAudioManager::GetSingleton()->GetSoundHandleByName(handle, a_editorID.c_str(), 16);
    if (!handle.IsValid()) {
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

[[nodiscard]] inline bool HasAllRequiredPerks(
    const RE::Actor* a_actor,
    std::span<RE::BGSPerk* const> a_perks
) noexcept {
    if (a_perks.empty()) {
        return true;
    }
    if (a_actor == nullptr) {
        return false;
    }
    return std::ranges::all_of(a_perks, [a_actor](RE::BGSPerk* a_perk) { return a_actor->HasPerk(a_perk); });
}
}
