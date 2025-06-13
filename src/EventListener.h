#pragma once

class EventListener final :
    REX::Singleton<EventListener>,
    public RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent>
{
public:
    static void Register() noexcept;

protected:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActiveEffectApplyRemoveEvent* event,
                                          RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>* source) override;
};
