import time

import pytest
from bmk.testing import wait_for

from .support import ward_actor


@pytest.mark.parametrize("spell", ["0x12FCD", "0x252C2"])
def test_concentration_stops_after_casting(wards, attacker, spell):
    effect = wards.p("Spell", "GetNthEffectMagicEffect", [0], spell)["formId"]
    assert wards.p("MagicEffect", "GetCastingType", self_form=effect) == 2
    wards.settings(
        bEnableSpellReflection=1,
        iShoutMode=3,
        bShoutInstantBreak=1,
        bInstantCharge=1,
    )
    wards.cast()
    wards.p("Actor", "SetActorValue", ["Magicka", 10000.0], attacker)
    wards.p("Actor", "SetActorValue", ["MagickaRateMult", 0.0], attacker)

    def casters():
        refs = wards.call("inspect", {"kind": "refs", "formType": "ACTI", "limit": 500})
        assert not refs["truncated"]
        return [ref for ref in refs["refs"] if ref["base"]["formId"] == "0x000B79FF"]

    before = len(casters())
    wards.call("console", {"command": f'"{attacker[2:]}".cast {spell[2:]} 14 left'})
    wards.wait(
        lambda state: (
            (actor := ward_actor(state, int(attacker, 16))) is not None
            and actor["left"]["spell"] == int(spell, 16)
            and actor["left"]["state"] == 6
        ),
        "The attacker did not start concentration casting",
    )
    wait_for(
        lambda: wards.health(),
        lambda health: health < 1995,
        "The concentration spell did not reach the defender",
    )
    assert len(casters()) == before, (
        "Concentration created a reflection/pass-through caster"
    )
    wards.p("Actor", "InterruptCast", self_form=attacker)
    wards.wait(
        lambda s: ward_actor(s, int(attacker, 16))["left"]["state"] == 0,
        "Concentration casting did not stop",
    )
    wards.stop()
    # Burning ground can apply a separate damage spell after the breath stops.
    health = wards.health()
    changed_at = time.monotonic()

    def settled(current):
        nonlocal health, changed_at
        if abs(current - health) > 0.1:
            health, changed_at = current, time.monotonic()
        return time.monotonic() - changed_at >= 2

    wait_for(
        wards.health,
        settled,
        "Damage did not stop after the caster stopped and lingering fire expired",
        timeout=12,
    )
    health = wards.health()
    time.sleep(2)
    assert abs(wards.health() - health) < 0.1, (
        "Damage continued after concentration ended"
    )
