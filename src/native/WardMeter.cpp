#include <RE/Skyrim.h> // IWYU pragma: keep

#include "Mechanics.h"
#include "Settings.h"
#include "SkyrimUtil.h"
#include "WardMeter.h"
#include <RE/A/ActorValues.h>
#include <RE/H/HUDChargeMeter.h>
#include <RE/M/MagicSystem.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/SpellItem.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

namespace WardMeter {
namespace {

    [[nodiscard]] std::optional<bool> GetActiveWardHand(RE::PlayerCharacter* a_player) {
        if (a_player == nullptr) {
            return std::nullopt;
        }

        const auto* rightCaster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
        const auto* rightSpell = (rightCaster != nullptr && rightCaster->currentSpell != nullptr)
                                     ? rightCaster->currentSpell->As<RE::SpellItem>()
                                     : nullptr;
        if (Mechanics::IsWardSpell(rightSpell) && rightCaster->state == RE::MagicCaster::State::kCasting) {
            return false;
        }

        const auto* leftCaster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
        const auto* leftSpell = (leftCaster != nullptr && leftCaster->currentSpell != nullptr)
                                    ? leftCaster->currentSpell->As<RE::SpellItem>()
                                    : nullptr;
        if (Mechanics::IsWardSpell(leftSpell) && leftCaster->state == RE::MagicCaster::State::kCasting) {
            return true;
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<double> GetWardPowerPercent(RE::PlayerCharacter* a_player) {
        if (a_player == nullptr) {
            return std::nullopt;
        }

        const auto* process = a_player->GetActorRuntimeData().currentProcess;
        const auto* middleHigh = (process != nullptr) ? process->middleHigh : nullptr;
        if ((middleHigh == nullptr) || middleHigh->maximumWardPower <= 0.F) {
            return std::nullopt;
        }

        const auto* actorValues = a_player->AsActorValueOwner();
        if (actorValues == nullptr) {
            return std::nullopt;
        }

        const float current = actorValues->GetActorValue(RE::ActorValue::kWardPower);
        if (current <= 0.F) {
            return std::nullopt;
        }

        const double pct = current / middleHigh->maximumWardPower * 100.0;
        return std::clamp(pct, 0.0, 100.0);
    }
    struct HUDChargeMeterUpdate {
    private:
        static inline bool g_isActive {false};
        static inline bool g_useLeftMeter {false};

    public:
        static void thunk(RE::HUDChargeMeter* a_this) {
            if (a_this == nullptr) {
                return;
            }

            if (!Settings::GetSingleton()->showWardMeter) {
                func(a_this);
                return;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player == nullptr) {
                func(a_this);
                return;
            }

            if (const auto activeHand = GetActiveWardHand(player)) {
                if (const auto fillPct = GetWardPowerPercent(player)) {
                    if (g_isActive && g_useLeftMeter != *activeHand) {
                        a_this->root.Invoke("FadeOutChargeMeters");
                    }

                    g_isActive = true;
                    g_useLeftMeter = *activeHand;

                    const std::array<RE::GFxValue, 4> args {*fillPct, true, g_useLeftMeter, true};
                    a_this->root.Invoke("SetChargeMeterPercent", nullptr, args);
                    return;
                }
            }

            if (g_isActive) {
                g_isActive = false;
                a_this->root.Invoke("FadeOutChargeMeters");
            }

            func(a_this);
        }

        static inline REL::Relocation<decltype(thunk)> func;
        static constexpr std::size_t kIdx = 0x1;
    };
}

void InstallHook() {
    // The vtable address is resolved from the running Skyrim executable.
    // NOLINTNEXTLINE(clang-analyzer-core.FixedAddressDereference)
    stl::WriteVFunc<RE::HUDChargeMeter, HUDChargeMeterUpdate>();
    SKSE::log::info("WardMeter: hook installed");
}
}
