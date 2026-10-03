import time

import pytest
from bmk.testing import wait_for

from .support import ward_actor


def set_spellbreaker_block(wards, actor, npc, blocking):
    """Request blocking or its release until the ward follows.

    A single request can be ignored while the draw animation finishes, and NPC
    combat AI can overwrite an animation event's blocking state either way.
    """

    def ward_follows(state):
        ward = ward_actor(state, int(actor, 16))
        if ward is None:
            return False
        if blocking:
            return abs(ward["wardPower"] - 50) < 0.1
        return (
            abs(ward["wardPower"]) < 0.01
            and all(effect["inactive"] for effect in ward["wardEffects"])
            and not ward["blocking"]
        )

    deadline = time.monotonic() + 10
    while True:
        if npc:
            wards.p(
                "ObjectReference",
                "SetAnimationVariableBool",
                ["IsBlocking", blocking],
                actor,
            )
        wards.animate(actor, "blockStart" if blocking else "blockStop")
        try:
            return wards.wait(
                ward_follows,
                f"Spellbreaker did not {'raise' if blocking else 'lower'} its ward",
                timeout=1,
            )
        except AssertionError:
            if time.monotonic() >= deadline:
                raise


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
        set_spellbreaker_block(wards, actor, defender == "npc", True)
        wards.restore_health(attacker)
        wards.p("Spell", "Cast", [{"form": source}, {"form": actor}], "0x12FD0")
        wards.wait_for_health_damage(source, below=1990)
        assert wards.health(actor) >= 1999.9
        assert wards.health(source) > 1900
        set_spellbreaker_block(wards, actor, defender == "npc", False)
        time.sleep(0.5)
