#include <RE/Skyrim.h> // IWYU pragma: keep

#include "EventListener.h"
#include "Events.h"
#include "GameTasks.h"
#include "Reflection.h"
#include "Shouts.h"
#include <RE/A/Actor.h>
#include <RE/B/BSTEvent.h>
#include <RE/M/MagicItem.h>
#include <RE/M/MagicSystem.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/T/TESForm.h>
#include <SKSE/SKSE.h>

void EventListener::Register() {
    auto* listener = GetSingleton();
    auto* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();

    if (listener == nullptr) {
        SKSE::log::critical("EventListener registration failed: listener singleton is null");
        return;
    }
    if (eventHolder == nullptr) {
        SKSE::log::critical("EventListener registration failed: ScriptEventSourceHolder is null");
        return;
    }

    eventHolder->AddEventSink(listener);
    SKSE::log::info("EventListener registered");
}

EventListener::Control EventListener::ProcessEvent(
    const RE::TESMagicWardHitEvent* a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESMagicWardHitEvent>* a_eventSource
) {
    if (a_event == nullptr) {
        SKSE::log::debug("TESMagicWardHitEvent skipped: null event");
        return Control::kContinue;
    }

    const auto defenderHandle = (a_event->defender != nullptr) ? a_event->defender->CreateRefHandle()
                                                               : RE::ObjectRefHandle {};
    const auto attackerHandle = (a_event->attacker != nullptr) ? a_event->attacker->CreateRefHandle()
                                                               : RE::ObjectRefHandle {};
    const auto spellID = a_event->spell;
    const auto status = a_event->status;

    GameTasks::Add([defenderHandle, attackerHandle, spellID, status] {
        const auto defenderPtr = defenderHandle.get();
        auto* defender = defenderPtr ? defenderPtr->As<RE::Actor>() : nullptr;
        if (!defender) {
            return;
        }

        const auto attackerPtr = attackerHandle.get();
        auto* attacker = attackerPtr ? attackerPtr->As<RE::Actor>() : nullptr;

        auto* spell = RE::TESForm::LookupByID<RE::MagicItem>(spellID);
        if (!spell) {
            SKSE::log::debug("Hit ignored: spell FormID {:08X} not found", spellID);
            return;
        }

        if (spell->GetSpellType() == RE::MagicSystem::SpellType::kVoicePower) {
            Shouts::ProcessWardHit(defender, attacker, spell);
        } else {
            Reflection::ProcessWardHit(defender, attacker, spell, status);
        }
    });
    return Control::kContinue;
}
