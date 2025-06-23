#include "EventListener.h"

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

    auto* target = a_event->target->As<RE::Actor>();
    auto* cause = a_event->cause->As<RE::Actor>();
    if (!target || !cause) {
        return Control::kContinue;
    }

    logger::debug("{} hit {}", cause->GetDisplayFullName(), target->GetDisplayFullName());

    return Control::kContinue;
}
