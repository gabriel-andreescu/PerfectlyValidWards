import time

import pytest


def test_unprotected_firebolt(wards, attacker):
    wards.stop()
    wards.restore_health(attacker)
    wards.enemy_spell(attacker, "0x12FD0")
    time.sleep(1.2)
    assert wards.health() < 1990


@pytest.mark.parametrize("reflect", [0, 1])
def test_reflection(wards, attacker, reflect):
    wards.settings(bEnableSpellReflection=reflect, bInstantCharge=1)
    wards.cast()
    wards.restore_health(attacker)
    wards.enemy_spell(attacker, "0x12FD0")
    time.sleep(1.2)
    assert wards.health() >= 1999.9
    health = wards.health(attacker)
    assert health < 1990 if reflect else health >= 1999.9


@pytest.mark.parametrize("mode", [1, 3])
def test_nonbreaking_shout(wards, attacker, mode):
    wards.settings(
        fMagnitudeMultiplier=10,
        fChargeRateMultiplier=0.01,
        fShoutDamage=10,
        bShoutInstantBreak=0,
        iShoutMode=mode,
        bInstantCharge=1,
    )
    wards.cast(maximum=800)
    wards.settings(bInstantCharge=0)
    wards.restore_health(attacker)
    wards.enemy_spell(attacker, "0x3F9EB")
    time.sleep(0.8)
    power = wards.state()["wardPower"]
    assert 787 < power < 793 and wards.health() >= 1999.9, power


@pytest.mark.parametrize("mode", [0, 1, 2, 3])
def test_shout_modes(wards, attacker, mode):
    wards.settings(
        fMagnitudeMultiplier=10,
        fChargeRateMultiplier=0.01,
        iShoutMode=mode,
        bShoutInstantBreak=1,
        bInstantCharge=1,
    )
    wards.cast(maximum=800)
    wards.settings(bInstantCharge=0)
    wards.restore_health(attacker)
    wards.enemy_spell(attacker, "0x3F9EB")
    time.sleep(1)
    power, health = wards.state()["wardPower"], wards.health()
    assert power > 799 if mode == 0 else power < 1
    assert health < 1990 if mode == 3 else health >= 1999.9


@pytest.mark.parametrize("blocked", [0, 1])
def test_cloak_blocking(wards, attacker, blocked):
    wards.p(
        "ObjectReference",
        "MoveTo",
        [{"form": "0x14"}, -150.0, 0.0, 0.0, False],
        attacker,
    )
    wards.p("Actor", "SetActorValue", ["UnarmedDamage", 0.0], attacker)
    wards.settings(
        fMagnitudeMultiplier=10,
        fChargeRateMultiplier=0.01,
        bBlockCloaks=blocked,
        bInstantCharge=1,
        iShoutMode=0,
    )
    wards.cast(maximum=800)
    wards.settings(bInstantCharge=0)
    wards.restore_health(attacker)
    wards.enemy_spell(attacker, "0x3AE9F", attacker)
    if blocked:
        wards.wait(
            lambda s: s["wardPower"] < 795,
            "The blocked cloak did not consume ward power",
        )
        assert wards.health() >= 1999.9
    else:
        time.sleep(3)
        assert wards.health() < 1999
    wards.p("Actor", "DispelSpell", [{"form": "0x3AE9F"}], attacker)
    time.sleep(1.5)


def test_player_shout_immunity(wards, attacker):
    wards.settings(
        fMagnitudeMultiplier=10,
        fChargeRateMultiplier=0.01,
        iShoutMode=3,
        bShoutInstantBreak=1,
        bPlayerImmuneToShoutMechanics=1,
        bInstantCharge=1,
    )
    wards.cast(maximum=800)
    wards.settings(bInstantCharge=0)
    wards.restore_health(attacker)
    wards.enemy_spell(attacker, "0x3F9EB")
    time.sleep(1)
    assert wards.state()["wardPower"] > 799 and wards.health() >= 1999.9


@pytest.mark.parametrize("enabled,angle", [(0, 270.0), (1, 270.0), (1, 90.0)])
def test_disease_blocking(wards, attacker, enabled, angle):
    wards.settings(bBlockDiseases=enabled, bInstantCharge=1, fMagnitudeMultiplier=10)
    wards.cast(maximum=800)
    wards.p("ObjectReference", "SetAngle", [0.0, 0.0, angle], "0x14")
    wards.p(
        "Actor",
        "DoCombatSpellApply",
        [{"form": "0x10A24A"}, {"form": "0x14"}],
        attacker,
    )
    time.sleep(0.5)
    disease = [
        effect
        for effect in wards.call("inspect", {"kind": "effects"})["activeEffects"]
        if effect["spell"]["formId"] == "0x0010A24A"
    ]
    assert (not disease) == (enabled == 1 and angle == 270.0)
    wards.p("Actor", "DispelSpell", [{"form": "0x10A24A"}], "0x14")
    time.sleep(0.1)
    wards.stop()
