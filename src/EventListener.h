#pragma once

class EventListener final :
        REX::Singleton<EventListener>,
        public RE::BSTEventSink<RE::TESHitEvent> {
public:
    static void Register();

protected:
    using Control = RE::BSEventNotifyControl;

    Control EventListener::ProcessEvent(
        const RE::TESHitEvent* a_event,
        [[maybe_unused]] RE::BSTEventSource<RE::TESHitEvent>* a_eventSource
    ) override;
};
