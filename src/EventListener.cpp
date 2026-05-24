#include "EventListener.h"
#include "Reflection.h"
#include "Shouts.h"

void EventListener::Register() {
    auto* listener = GetSingleton();
    auto* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();

    if (!listener) {
        logger::critical("EventListener registration failed: listener singleton is null");
        return;
    }
    if (!eventHolder) {
        logger::critical("EventListener registration failed: ScriptEventSourceHolder is null");
        return;
    }

    eventHolder->AddEventSink(listener);
    logger::info("EventListener registered");
}

EventListener::Control EventListener::ProcessEvent(
    const RE::TESMagicWardHitEvent* a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESMagicWardHitEvent>* a_eventSource
) {
    if (!a_event) {
        logger::debug("TESMagicWardHitEvent skipped: null event");
        return Control::kContinue;
    }

    const auto defenderHandle = a_event->defender ? a_event->defender->CreateRefHandle() : RE::ObjectRefHandle {};
    const auto attackerHandle = a_event->attacker ? a_event->attacker->CreateRefHandle() : RE::ObjectRefHandle {};
    const auto spellID = a_event->spell;
    const auto status = a_event->status;

    const auto* task = SKSE::GetTaskInterface();
    if (!task) {
        logger::critical("TESMagicWardHitEvent skipped: TaskInterface unavailable");
        return Control::kContinue;
    }

    task->AddTask([defenderHandle, attackerHandle, spellID, status] {
        const auto defenderPtr = defenderHandle.get();
        auto* defender = defenderPtr ? defenderPtr->As<RE::Actor>() : nullptr;
        if (!defender) {
            return;
        }

        const auto attackerPtr = attackerHandle.get();
        auto* attacker = attackerPtr ? attackerPtr->As<RE::Actor>() : nullptr;

        auto* spell = RE::TESForm::LookupByID<RE::MagicItem>(spellID);
        if (!spell) {
            logger::debug("Hit ignored: spell FormID {:08X} not found", spellID);
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
