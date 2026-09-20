import time

import pytest

from .support import ward_actor


@pytest.mark.parametrize("restricted", [0, 1])
def test_player_tweaks_player_team(wards, restricted):
    wards.settings(
        bRestrictToPlayerTeam=restricted, fMagnitudeMultiplier=2, bInstantCharge=1
    )
    wards.cast(maximum=160)


@pytest.mark.parametrize("restricted,teammate", [(0, False), (1, False), (1, True)])
def test_npc_tweaks_player_team(wards, attacker, restricted, teammate):
    wards.p("Actor", "SetPlayerTeammate", [teammate], attacker)
    wards.settings(
        bRestrictToPlayerTeam=restricted, fMagnitudeMultiplier=2, bInstantCharge=1
    )
    expected = 80 if restricted and not teammate else 160
    wards.cast_npc_ward(attacker, maximum=expected)
    wards.cast(maximum=160)


@pytest.mark.parametrize("restricted", [0, 1])
def test_npc_reflection_player_team(wards, attacker, restricted):
    wards.settings(
        bEnableSpellReflection=1,
        bRestrictReflectionToPlayerTeam=restricted,
        bInstantCharge=1,
    )
    wards.cast_npc_ward(attacker)
    wards.restore_health(attacker)
    wards.p("Spell", "Cast", [{"form": "0x14"}, {"form": attacker}], "0x12FD0")
    time.sleep(1.2)
    assert wards.health(attacker) >= 1999.9
    assert wards.health() >= 1999.9 if restricted else 1900 < wards.health() < 1990


@pytest.mark.parametrize("breaks", [False, True])
def test_npc_shout_damage(wards, attacker, breaks):
    wards.settings(
        iShoutMode=3,
        fShoutDamage=40,
        bShoutInstantBreak=0,
        fMagnitudeMultiplier=0.5 if breaks else 10,
        bInstantCharge=1,
    )
    wards.cast_npc_ward(attacker, maximum=40 if breaks else 800)
    wards.settings(bInstantCharge=0, fChargeRateMultiplier=0.01)
    wards.restore_health(attacker)
    wards.p("Spell", "Cast", [{"form": "0x14"}, {"form": attacker}], "0x3F9EB")
    time.sleep(1)
    state = ward_actor(wards.state(), int(attacker, 16))
    assert state is not None
    assert state["wardPower"] < 1 if breaks else 755 < state["wardPower"] < 765
    health = wards.health(attacker)
    assert 1900 < health < 1990 if breaks else health >= 1999.9
    assert wards.health() >= 1999.9
