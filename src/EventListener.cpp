#include "EventListener.h"

#include "Settings.h"
#include "WardManager.h"

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
    const RE::TESHitEvent* a_event,
    [[maybe_unused]] RE::BSTEventSource<RE::TESHitEvent>* a_eventSource
) {
    if (!a_event || !a_event->target || !a_event->cause) {
        return Control::kContinue;
    }

    return Control::kContinue;
}
