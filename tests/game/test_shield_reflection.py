import time

import pytest
from bmk.testing import wait_for

from .support import ward_actor


@pytest.mark.parametrize("defender", ["player", "npc"])
@pytest.mark.parametrize("auto_aim", [0, 1])
@pytest.mark.parametrize("distance", [150, 300])
def test_spellbreaker_reflection(wards, attacker, defender, auto_aim, distance):
    actor = "0x14" if defender == "player" else attacker
    source = attacker if defender == "player" else "0x14"
    wards.p(
        "ObjectReference",
        "MoveTo",
        [{"form": "0x14"}, -float(distance), 0.0, 0.0, False],
        attacker,
    )
    wards.settings(
        bEnableSpellReflection=1, bAutoAimReflection=auto_aim, bInstantCharge=1
    )
    wards.p("Actor", "UnequipAll", self_form=actor)
    wards.p("ObjectReference", "RemoveAllItems", self_form=actor)
    for item in ("0x1397E", "0x45F96"):
        wards.p("ObjectReference", "AddItem", [{"form": item}, 1, True], actor)
        wards.p("Actor", "EquipItem", [{"form": item}, False, True], actor)
    wards.p("Actor", "DrawWeapon", self_form=actor)
    wait_for(
        lambda: wards.p("Actor", "IsWeaponDrawn", self_form=actor),
        message="The defender did not draw its weapon",
    )
    time.sleep(2)
    for _ in range(5):
        if defender == "npc":
            # NPC combat AI can overwrite an animation event's blocking state.
            wards.p(
                "ObjectReference",
                "SetAnimationVariableBool",
                ["IsBlocking", True],
                actor,
            )
        wards.animate(actor, "blockStart")
        wards.wait(
            lambda s: (
                (ward := ward_actor(s, int(actor, 16))) is not None
                and abs(ward["wardPower"] - 50) < 0.1
            ),
            "Spellbreaker did not raise its ward",
        )
        wards.restore_health(attacker)
        wards.p("Spell", "Cast", [{"form": source}, {"form": actor}], "0x12FD0")
        time.sleep(1.2)
        assert wards.health(actor) >= 1999.9
        assert 1900 < wards.health(source) < 1990
        if defender == "npc":
            wards.p(
                "ObjectReference",
                "SetAnimationVariableBool",
                ["IsBlocking", False],
                actor,
            )
        wards.animate(actor, "blockStop")
        wards.wait(
            lambda s: (
                (ward := ward_actor(s, int(actor, 16))) is not None
                and abs(ward["wardPower"]) < 0.01
                and all(effect["inactive"] for effect in ward["wardEffects"])
                and not ward["blocking"]
            ),
            "Spellbreaker did not finish lowering its ward",
        )
        time.sleep(0.5)
