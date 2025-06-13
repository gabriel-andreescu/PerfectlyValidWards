#include "EventListener.h"

void EventListener::Register()
{
    const auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    if (!scriptEventSourceHolder) {
        logger::critical("Failed to get ScriptEventSourceHolder");
        return;
    }

    const auto listener = GetSingleton();
    if (!listener) {
        logger::error("Failed to get EventListener singleton");
        return;
    }

    //scriptEventSourceHolder->GetEventSource<RE::TESActiveEffectApplyRemoveEvent>()->AddEventSink(listener);
    logger::info("EventListener registered successfully");
}

RE::BSEventNotifyControl
EventListener::ProcessEvent([[maybe_unused]] const RE::TESActiveEffectApplyRemoveEvent* event,
                            [[maybe_unused]] RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>* source)
{
    return RE::BSEventNotifyControl::kContinue;
}
