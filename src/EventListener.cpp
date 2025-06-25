#include "EventListener.h"

#include "RE.h"

void EventListener::Register() {
    auto* listener = GetSingleton();
    auto* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();

    if (!listener || !eventHolder) {
        logger::critical(
            "EventListener registration failed (listener={}, holder={})",
            fmt::ptr(listener),
            fmt::ptr(eventHolder)
        );
        return;
    }
    eventHolder->AddEventSink(listener);
    logger::info("EventListener registered");
}

EventListener::Control EventListener::ProcessEvent(
    const RE::TESMagicWardHitEvent* a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESMagicWardHitEvent>* a_eventSource
) {
    if (!a_event || !a_event->defender || !a_event->attacker) {
        return Control::kContinue;
    }

    /*if (a_event->status == RE::TESMagicWardHitEvent::Status::kAbsorbed && a_event->spell) {
        auto* magicItem = RE::TESForm::LookupByID<RE::MagicItem>(a_event->spell);

        if (magicItem && magicItem->GetSpellType() == RE::MagicSystem::SpellType::kVoicePower) {
            logger::debug("Ward got hit by a shout");
        }
    }*/

    return Control::kContinue;
}
