import time

import pytest
from bmk.skyrim.devbench import Keyboard

from .support import ward_actor


@pytest.mark.parametrize("instant", [0, 1])
@pytest.mark.keyboard
def test_keyboard_cast_release(wards, instant):
    wards.settings(bInstantCharge=instant)
    wards.p("Actor", "DrawWeapon", self_form="0x14")
    time.sleep(1.5)
    with Keyboard(wards.client) as keyboard:
        key = keyboard.mapped_key("Left Attack/Block")
        for _ in range(3):
            with keyboard.hold(key, max_hold_ms=10000):
                wards.wait(
                    lambda s: (
                        s["left"]["state"] == 6 and abs(s["wardPower"] - 80) < 0.1
                    ),
                    "Keyboard input did not cast Greater Ward",
                )
            wards.wait(
                lambda s: (
                    s["left"]["state"] == 0
                    and s["wardPower"] < 0.01
                    and not s["wardEffects"]
                ),
                "Releasing the casting key retained a ward",
            )


def test_lesser_ward_arrows(wards, attacker):
    wards.settings(bInstantCharge=1)
    wards.equip_attacker(attacker, "0x3B562", "0x1397F", -300.0)
    wards.p("Actor", "EquipSpell", [{"form": "0x13018"}, 0], "0x14")
    time.sleep(1.2)
    for _ in range(5):
        wards.cast(maximum=40, spell=0x13018)
        wards.restore_health(attacker)
        xp_before = wards.restoration_xp()
        wards.p("Weapon", "Fire", [{"form": attacker}, {"form": "0x1397F"}], "0x3B562")
        time.sleep(1.5)
        assert wards.health() >= 1999.9
        assert wards.restoration_xp() > xp_before


@pytest.mark.parametrize("defender", ["player", "npc"])
def test_forward_reflection(wards, attacker, defender):
    wards.settings(bEnableSpellReflection=1, bAutoAimReflection=0, bInstantCharge=1)
    if defender == "player":
        wards.cast()
        source, target = attacker, "0x14"
    else:
        wards.p("Actor", "EquipSpell", [{"form": "0x12FD0"}, 1], "0x14")
        wards.p("Actor", "EquipSpell", [{"form": "0x5DB90"}, 0], "0x14")
        wards.p("Actor", "SetActorValue", ["Magicka", 10000.0], attacker)
        wards.p("Actor", "SetActorValue", ["MagickaRateMult", 0.0], attacker)
        wards.p("Actor", "EquipSpell", [{"form": "0x211F0"}, 0], attacker)
        time.sleep(1.2)
        wards.call(
            "console", {"command": f'"{attacker[2:]}".cast 211f0 {attacker[2:]} left'}
        )
        wards.wait(
            lambda s: (
                (ward := ward_actor(s, int(attacker, 16))) is not None
                and ward["left"]["state"] == 6
                and abs(ward["wardPower"] - 80) < 0.1
            ),
            "The NPC did not raise its ward",
        )
        source, target = "0x14", attacker
    for _ in range(10):
        wards.restore_health(attacker)
        wards.p("Spell", "Cast", [{"form": source}, {"form": target}], "0x12FD0")
        time.sleep(1.2)
        assert wards.health(target) >= 1999.9
        assert 1900 < wards.health(source) < 1990
