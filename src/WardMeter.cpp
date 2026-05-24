#include "WardMeter.h"
#include "Mechanics.h"
#include "Settings.h"

namespace WardMeter {
namespace {
    bool g_isActive {false};
    bool g_useLeftMeter {false};

    [[nodiscard]] std::optional<bool> GetActiveWardHand(RE::PlayerCharacter* a_player) {
        if (!a_player) {
            return std::nullopt;
        }

        if (const auto* caster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
            if (const auto* spell = caster->currentSpell ? caster->currentSpell->As<RE::SpellItem>() : nullptr) {
                if (Mechanics::IsWardSpell(spell) && caster->state == RE::MagicCaster::State::kCasting) {
                    return false;
                }
            }
        }

        if (const auto* caster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
            if (const auto* spell = caster->currentSpell ? caster->currentSpell->As<RE::SpellItem>() : nullptr) {
                if (Mechanics::IsWardSpell(spell) && caster->state == RE::MagicCaster::State::kCasting) {
                    return true;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<double> GetWardPowerPercent(RE::PlayerCharacter* a_player) {
        if (!a_player) {
            return std::nullopt;
        }

        const auto* process = a_player->GetActorRuntimeData().currentProcess;
        const auto* middleHigh = process ? process->middleHigh : nullptr;
        if (!middleHigh || middleHigh->maximumWardPower <= 0.f) {
            return std::nullopt;
        }

        const auto* av = a_player->AsActorValueOwner();
        if (!av) {
            return std::nullopt;
        }

        const float current = av->GetActorValue(RE::ActorValue::kWardPower);
        if (current <= 0.f) {
            return std::nullopt;
        }

        const double pct = current / middleHigh->maximumWardPower * 100.0;
        return std::clamp(pct, 0.0, 100.0);
    }
}

struct HUDChargeMeterUpdate {
    static void thunk(RE::HUDChargeMeter* a_this) {
        if (!a_this) {
            return;
        }

        if (!Settings::GetSingleton()->showWardMeter) {
            func(a_this);
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
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
    static constexpr std::size_t idx = 0x1;
};

void InstallHook() {
    stl::write_vfunc<RE::HUDChargeMeter, HUDChargeMeterUpdate>();
    logger::info("WardMeter: hook installed");
}
}
