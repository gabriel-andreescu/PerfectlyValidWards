import time

import pytest

from .support import ward_actor


@pytest.mark.parametrize("identity", ["player", "npc"])
@pytest.mark.parametrize("instant", [0, 1])
@pytest.mark.parametrize("charge_rate", [1, 0.75])
@pytest.mark.parametrize(
    "ending", ["interrupt", "unequip", "exhaust", "dispel", "stagger"]
)
def test_cast_cleanup(wards, request, identity, instant, charge_rate, ending):
    actor = "0x14" if identity == "player" else request.getfixturevalue("attacker")
    form_id = int(actor, 16)
    console_actor = "player" if identity == "player" else f'"{actor[2:]}"'
    wards.p("Actor", "SetActorValue", ["Magicka", 10000.0], actor)
    wards.p("Actor", "SetActorValue", ["MagickaRateMult", 0.0], actor)
    wards.settings(bInstantCharge=instant, fChargeRateMultiplier=charge_rate)
    wards.p("Actor", "InterruptCast", self_form=actor)
    wards.p("Actor", "RestoreActorValue", ["Magicka", 10000.0], actor)
    wards.p("Actor", "EquipSpell", [{"form": "0x211F0"}, 0], actor)
    # Equipping can interrupt casting until its animation finishes.
    time.sleep(1.2)
    wards.call("console", {"command": f"{console_actor}.cast 211f0 {actor[2:]} left"})

    def casting(state):
        ward = ward_actor(state, form_id)
        return ward and ward["left"]["state"] == 6 and abs(ward["wardPower"] - 80) < 0.1

    wards.wait(casting, f"Ward did not start for {identity} before {ending}")
    if ending == "interrupt":
        wards.p("Actor", "InterruptCast", self_form=actor)
    elif ending == "unequip":
        wards.p("Actor", "UnequipSpell", [{"form": "0x211F0"}, 0], actor)
    elif ending == "exhaust":
        wards.p("Actor", "DamageActorValue", ["Magicka", 10000.0], actor)
    elif ending == "dispel":
        wards.p("Actor", "DispelSpell", [{"form": "0x211F0"}], actor)
    else:
        wards.animate(actor, "staggerStart")

    def stopped(state):
        ward = ward_actor(state, form_id)
        return (
            ward
            and abs(ward["wardPower"]) < 0.01
            and not ward["wardEffects"]
            and ward["left"]["state"] == 0
        )

    wards.wait(
        stopped, f"Ward remained after {ending} for {identity}, instant={instant}"
    )
    time.sleep(1.5)


@pytest.mark.parametrize("instant", [0, 1])
def test_spellbreaker_cleanup(wards, instant):
    wards.p("Actor", "UnequipAll", self_form="0x14")
    wards.p("ObjectReference", "AddItem", [{"form": "0x45F96"}, 1, True], "0x14")
    wards.p("Actor", "EquipItem", [{"form": "0x45F96"}, False, True], "0x14")
    wards.p("Actor", "DrawWeapon", self_form="0x14")
    time.sleep(2)
    wards.settings(bInstantCharge=instant)
    wards.animate("0x14", "blockStart")
    wards.wait(
        lambda s: (
            abs(s["wardPower"] - 50) < 0.1
            and s["left"]["state"] == s["right"]["state"] == 0
        ),
        "Spellbreaker did not raise its ward without hand casting",
    )
    wards.animate("0x14", "blockStop")
    wards.wait(lambda s: abs(s["wardPower"]) < 0.01, "Spellbreaker retained ward power")
