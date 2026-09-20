"""Ward casting and combat setup used by the in-game tests."""

import time

from bmk.testing import set_ini_values, wait_for


def ward_actor(state, form_id):
    if state["formId"] == form_id:
        return state
    return next((actor for actor in state["npcs"] if actor["formId"] == form_id), None)


class WardSession:
    def __init__(self, client, settings_path, baseline):
        self.client = client
        self.call = client.call
        self.p = client.papyrus
        self.settings_path = settings_path
        self.baseline = baseline
        self.mcm_quest = None

    def state(self):
        return self.call("inspect", {"kind": "perfectlyvalidwards"})

    def wait(self, predicate, message, timeout=10):
        return wait_for(self.state, predicate, message, timeout=timeout, interval=0.05)

    def restore(self):
        self.client.load(self.baseline, cell="QASmoke", settle_ms=2000)

    def reload_settings(self):
        self.p("PerfectlyValidWards_MCM", "OnConfigClose", self_form=self.mcm_quest)

    def settings(self, **values):
        set_ini_values(self.settings_path, values)
        self.reload_settings()

    def initialize(self):
        self.p("ObjectReference", "SetAngle", [0.0, 0.0, 270.0], "0x14")
        self.p("Actor", "SetActorValue", ["Magicka", 10000.0], "0x14")
        self.p("Actor", "SetActorValue", ["MagickaRateMult", 0.0], "0x14")
        self.p("Actor", "EquipSpell", [{"form": "0x211F0"}, 0], "0x14")
        self.settings(
            fMagnitudeMultiplier=1,
            fChargeRateMultiplier=1,
            fCostMultiplier=1,
            bInstantCharge=0,
            bInstantCast=0,
            bRestrictToPlayerTeam=0,
            bBlockMelee=1,
            bBlockArrows=1,
            fPowerDamageMultiplier=1,
            fBlockXPScale=0.25,
            bEnableSpellReflection=0,
            bAutoAimReflection=1,
            bRestrictReflectionToPlayerTeam=0,
            bStaggerNormalAttacks=0,
            bStaggerPowerAttacks=0,
            fBlockingAngle=90,
            bPlayerImmuneToShoutMechanics=0,
            bBlockCloaks=1,
            bBlockDiseases=1,
            fCloakDamageMultiplier=1,
        )

    def stop(self):
        self.p("Actor", "InterruptCast", self_form="0x14")
        self.wait(
            lambda s: (
                s["wardPower"] <= 0.01
                and s["left"]["state"] == s["right"]["state"] == 0
            ),
            "Casting did not stop",
        )

    def cast(self, maximum=80, hand="left", spell=0x211F0):
        self.stop()
        self.p("Actor", "RestoreActorValue", ["Magicka", 10000.0], "0x14")
        self.call("console", {"command": f"player.cast {spell:X} 14 {hand}"})
        return self.wait(
            lambda s: s[hand]["state"] == 6 and abs(s["wardPower"] - maximum) < 0.1,
            f"The {hand} ward did not reach {maximum} power",
        )

    def health(self, actor="0x14"):
        return self.p("Actor", "GetActorValue", ["Health"], actor)

    def cast_npc_ward(self, actor, maximum=80):
        self.p("Actor", "SetActorValue", ["Magicka", 10000.0], actor)
        self.p("Actor", "SetActorValue", ["MagickaRateMult", 0.0], actor)
        self.p("Actor", "EquipSpell", [{"form": "0x211F0"}, 0], actor)
        time.sleep(1.2)
        self.call("console", {"command": f'"{actor[2:]}".cast 211f0 {actor[2:]} left'})
        return self.wait(
            lambda s: (
                (ward := ward_actor(s, int(actor, 16))) is not None
                and ward["left"]["state"] == 6
                and abs(ward["wardPower"] - maximum) < 0.1
            ),
            f"The NPC ward did not reach {maximum} power",
        )

    def restore_health(self, attacker):
        for actor in ("0x14", attacker):
            self.p("Actor", "RestoreActorValue", ["Health", 2000.0], actor)

    def enemy_spell(self, attacker, spell, target="0x14"):
        self.p("Spell", "Cast", [{"form": attacker}, {"form": target}], spell)

    def attacker(self):
        actor = self.client.spawn(0x9F358, "ACHR")
        self.p("Actor", "SetActorValue", ["Aggression", 3.0], actor)
        self.p("Actor", "StartCombat", [{"form": "0x14"}], actor)
        wait_for(
            lambda: self.p("Actor", "IsHostileToActor", [{"form": actor}], "0x14"),
            message="The player did not enter combat with the attacker",
        )
        self.p("Actor", "SetRestrained", [True], actor)
        self.p("Actor", "SetDontMove", [True], actor)
        self.p("Actor", "SetActorValue", ["Stamina", 0.0], actor)
        self.p("Actor", "SetActorValue", ["StaminaRateMult", 0.0], actor)
        self.p(
            "ObjectReference",
            "MoveTo",
            [{"form": "0x14"}, -300.0, 0.0, 0.0, False],
            actor,
        )
        self.p("ObjectReference", "SetAngle", [0.0, 0.0, 90.0], actor)
        for identity in ("0x14", actor):
            self.p("Actor", "SetActorValue", ["Health", 2000.0], identity)
            self.p("Actor", "SetActorValue", ["HealRateMult", 0.0], identity)
        self.restore_health(actor)
        reference = self.call("inspect", {"kind": "refs", "formId": actor})["refs"][0]
        assert reference["actor"]["hostileToPlayer"]
        assert self.p("Actor", "HasLOS", [{"form": actor}], "0x14")
        return actor

    def equip_attacker(self, attacker, weapon=None, ammo=None, distance=-75.0):
        self.p(
            "ObjectReference",
            "MoveTo",
            [{"form": "0x14"}, distance, 0.0, 0.0, False],
            attacker,
        )
        self.p("Actor", "UnequipAll", self_form=attacker)
        self.p("ObjectReference", "RemoveAllItems", self_form=attacker)
        for item, count in ((weapon, 1), (ammo, 10)):
            if item:
                self.p(
                    "ObjectReference",
                    "AddItem",
                    [{"form": item}, count, True],
                    attacker,
                )
                self.p("Actor", "EquipItem", [{"form": item}, False, True], attacker)
        self.p("Actor", "SetActorValue", ["UnarmedDamage", 10.0], attacker)
        self.p("Actor", "DrawWeapon", self_form=attacker)
        self.p("ObjectReference", "SetAngle", [0.0, 0.0, 90.0], attacker)
        time.sleep(2)

    def restoration_xp(self):
        info = self.p("ActorValueInfo", "GetActorValueInfoByName", ["Restoration"])[
            "formId"
        ]
        return self.p("ActorValueInfo", "GetSkillExperience", self_form=info)

    def animate(self, actor, event):
        self.p("Debug", "SendAnimationEvent", [{"form": actor}, event])
