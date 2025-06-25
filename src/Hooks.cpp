#include "Hooks.h"
#include "Settings.h"
#include "WardManager.h"

namespace Hooks {
    namespace {
        [[nodiscard]] bool IsWardBlocking(RE::Actor* defender, RE::Actor* attacker) {
            if (!defender || !attacker) {
                return false;
            }
            if (WardManager::GetCurrentWardPower(defender) <= 0.f) {
                return false;
            }
            const auto angle = defender->GetHeadingAngle(attacker->GetPosition(), true);
            return angle <= Settings::GetSingleton()->wardBlockingAngle;
        }

        template<class Fn>
        void AddActorTaskSafe(RE::Actor* a_actor, Fn&& a_fn, std::chrono::nanoseconds a_delay = 0ns) {
            const RE::ActorHandle handle = a_actor ? a_actor->GetHandle() : RE::ActorHandle{};
            stl::add_thread_task(
                [handle, fn = std::forward<Fn>(a_fn)]() mutable {
                    if (auto* actor = handle.get().get(); actor) { fn(actor); }
                },
                a_delay
            );
        }

        void PlayDeflectSound(RE::Actor* defender) {
            AddActorTaskSafe(
                defender,
                [](RE::Actor* a_defender) {
                    stl::play_sound(a_defender, "MAGWardTestDeflect");
                }
            );
        }

        bool IsExperienceModLoaded() noexcept {
            static const bool loaded = GetModuleHandleW(L"Experience.dll") != nullptr;
            return loaded;
        }
    }

    void Install() {
        stl::write_thunk_call<Actor_CombatHit>(
            REL::Relocation{ RELOCATION_ID(37673, 38627), REL::Relocate(0x3c0, 0x4A8) }
        );

        // SSE: Up      p   HitData__Populate_140742850+37C                     call    HitData__Resolve_140743510
        // AE: Up       p   HitData__Populate_1407DAFF0+358                     call    HitData__Resolve_1407DBD20
        stl::write_thunk_call<HitData_Resolve>(
            REL::Relocation{ RELOCATION_ID(42832, 44001), REL::Relocate(0x37C, 0x358, 0x3CF) }
        );

        stl::write_thunk_call<Actor_GetBlockCost>(
            REL::Relocation{ RELOCATION_ID(37633, 38586), REL::Relocate(0x8D4, 0xB39) }
        );

        // SSE: Up      p   PlayerCharacter__AdvanceSkill_1406A2540+25          call    sub_1406E61D0
        // AE:  Up      p   PlayerCharacter__AddSkillExperience_140736E20+25    call    sub_7FF7F480AE60
        if (!IsExperienceModLoaded()) {
            stl::write_thunk_call<AddSkillExperience>(
                REL::Relocation{ RELOCATION_ID(39413, 40488), 0x25 }
            );
        }
    }

    // needed for melee attacks to not use metal impact sound
    bool HitData_Resolve::thunk(RE::HitData* a_hitData, bool a_ignoreBlocking) {
        auto* attacker = a_hitData->aggressor.get().get();
        auto* defender = a_hitData->target.get().get();

        if (IsWardBlocking(defender, attacker)) {
            a_hitData->totalDamage = 0.f;
            a_hitData->percentBlocked = 1.f;
            a_hitData->flags.set(RE::HitData::Flag::kBlocked);
            a_hitData->flags.set(RE::HitData::Flag::kBlockWithWeapon);
        }
        return func(a_hitData, a_ignoreBlocking);
    }

    float Actor_GetBlockCost::thunk(RE::HitData& a_hitData) {
        auto* attacker = a_hitData.aggressor.get().get();
        auto* defender = a_hitData.target.get().get();

        if (IsWardBlocking(defender, attacker)) {
            return 0.f; // ward covers the cost entirely
        }
        return func(a_hitData);
    }

    static void ApplyMeleeFeedback(RE::Actor* attacker, RE::Actor* defender, bool isPowerAttack) {
        const auto* settings = Settings::GetSingleton();

        // power attack recoils attacker
        if (isPowerAttack && settings->staggerPowerAttacks) {
            AddActorTaskSafe(
                attacker,
                [](RE::Actor* actor) {
                    actor->NotifyAnimationGraph("recoilLargeStart");
                }
            );
        }
        // normal swing staggers attacker
        else if (!isPowerAttack && settings->staggerNormalAttacks) {
            AddActorTaskSafe(
                attacker,
                [defHandle = defender->GetHandle(), settings](RE::Actor* actor) {
                    if (auto* l_defender = defHandle.get().get()) {
                        const auto heading = actor->GetHeadingAngle(l_defender->GetPosition(), false);
                        const auto dir = heading >= 0.f ? heading / 360.f : (360.f + heading) / 360.f;
                        actor->SetGraphVariableFloat("staggerDirection", dir);
                        actor->SetGraphVariableFloat("StaggerMagnitude", settings->staggerMagnitude);
                        actor->NotifyAnimationGraph("staggerStart");
                    }
                }
            );
        }
    }

    float Actor_CombatHit::thunk(RE::Actor* a_this, RE::HitData* a_hitData) {
        auto* attacker = a_hitData->aggressor.get().get();
        auto* defender = a_hitData->target.get().get();

        if (IsWardBlocking(defender, attacker)) {
            const auto* settings = Settings::GetSingleton();
            const auto weapon = a_hitData->weapon;
            const bool isMelee = weapon ? weapon->IsMelee() : false;
            const bool isPowerAtk = a_hitData->flags.any(RE::HitData::Flag::kPowerAttack);

            if (isMelee) {
                ApplyMeleeFeedback(attacker, defender, isPowerAtk);
            }

            a_hitData->totalDamage = 0.f;

            // needed for projectiles to not spawn blood particles
            a_hitData->percentBlocked = 1.f;
            a_hitData->flags.set(RE::HitData::Flag::kBlocked);
            a_hitData->flags.set(RE::HitData::Flag::kBlockWithWeapon);

            const float wardDamage = WardManager::EstimateWardPowerDamage(defender, attacker, weapon, isPowerAtk);
            WardManager::DamageWardPower(defender, wardDamage);

            if (defender->IsPlayer() || defender->IsPlayerRef()) {
                auto* player = RE::PlayerCharacter::GetSingleton();
                auto* avOwner = player->AsActorValueOwner();

                // need to investigate if this would benefit other actors
                WardManager::FlagWardBlock(player);

                if (!IsExperienceModLoaded() && avOwner && wardDamage > 0.f) {
                    const float skill = avOwner->GetActorValue(RE::ActorValue::kRestoration);
                    const float skillScale = std::clamp(1.f - skill / 100.f, 0.1f, 1.f);
                    const float xp = wardDamage * settings->wardBlockXPScale * skillScale;

                    if (xp > 0.f) {
                        AddActorTaskSafe(
                            player,
                            [xp](RE::Actor* a_player) {
                                // cast is safe because we only pass PlayerCharacter* in
                                dynamic_cast<RE::PlayerCharacter*>(a_player)
                                        ->AddSkillExperience(RE::ActorValue::kRestoration, xp);
                            }
                        );
                        logger::debug(
                            "Granted {:.2f} Restoration XP (wardDamage: {:.2f}, skillLvl: {:.0f}, scale: {:.2f})",
                            xp,
                            wardDamage,
                            skill,
                            skillScale
                        );
                    }
                }
            }

            PlayDeflectSound(defender);
        }

        return func(a_this, a_hitData);
    }

    void AddSkillExperience::thunk(
        float** a_skills,
        RE::ActorValue a_av,
        float a_xp,
        std::uint64_t a_unk1,
        std::uint32_t a_unk2,
        bool a_applyMult,
        bool a_silent
    ) {
        // need to investigate if this would benefit other actors (would need a different hook if so)
        if ((a_av == RE::ActorValue::kBlock || a_av == RE::ActorValue::kLightArmor || a_av ==
             RE::ActorValue::kHeavyArmor) &&
            WardManager::ConsumeWardBlock(RE::PlayerCharacter::GetSingleton())) {
            logger::debug("Ignored {:.2f} XP for {} because it came from ward‑blocking", a_xp, a_av);
            return func(a_skills, RE::ActorValue::kRestoration, 0, a_unk1, a_unk2, a_applyMult, a_silent);
        }
        return func(a_skills, a_av, a_xp, a_unk1, a_unk2, a_applyMult, a_silent);
    }
}
