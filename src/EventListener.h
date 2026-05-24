#pragma once
#include "Events.h"
#include <REX/REX/Singleton.h>

class EventListener final : REX::Singleton<EventListener>, public RE::BSTEventSink<RE::TESMagicWardHitEvent> {
public:
    static void Register();

protected:
    using Control = RE::BSEventNotifyControl;

    Control ProcessEvent(
        const RE::TESMagicWardHitEvent* a_event,
        [[maybe_unused]] RE::BSTEventSource<RE::TESMagicWardHitEvent>* a_eventSource
    ) override;
};
