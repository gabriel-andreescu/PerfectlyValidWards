import time

import pytest


@pytest.mark.parametrize("attack", ["melee", "unarmed", "arrow", "bolt", "Firebolt"])
@pytest.mark.parametrize("melee", [0, 1])
@pytest.mark.parametrize("arrows", [0, 1])
def test_physical_toggles(wards, attacker, attack, melee, arrows):
    weapon, ammo, distance = {
        "melee": ("0x1397E", None, -75.0),
        "unarmed": (None, None, -75.0),
        "arrow": ("0x3B562", "0x1397F", -300.0),
        "bolt": (None, None, -300.0),
        "Firebolt": (None, None, -300.0),
    }[attack]
    if attack == "bolt":
        weapon = f"0x{wards.client.form_id(0x801, 'Dawnguard.esm'):08X}"
        ammo = f"0x{wards.client.form_id(0xBB3, 'Dawnguard.esm'):08X}"
    wards.settings(
        fMagnitudeMultiplier=10,
        fChargeRateMultiplier=0.01,
        bBlockMelee=melee,
        bBlockArrows=arrows,
        bInstantCharge=1,
    )
    wards.equip_attacker(attacker, weapon, ammo, distance)
    wards.cast(maximum=800)
    wards.settings(bInstantCharge=0)
    patches = wards.state()["physicalPatches"]
    assert patches["weaponToWard"] == patches["wardToWeapon"] == bool(melee)
    assert patches["projectileToWard"] == patches["wardToProjectile"] == bool(arrows)
    impact = wards.client.form_id(0x800, "PerfectlyValidWards.esp") if arrows else 0
    assert patches["arrowWardImpact"] == impact
    wards.restore_health(attacker)
    actual_distance = wards.p(
        "ObjectReference", "GetDistance", [{"form": attacker}], "0x14"
    )
    assert actual_distance < abs(distance) + 50
    xp_before = wards.restoration_xp()
    if ammo:
        wards.p("Weapon", "Fire", [{"form": attacker}, {"form": ammo}], weapon)
        blocked = bool(arrows)
    elif attack == "Firebolt":
        wards.enemy_spell(attacker, "0x12FD0")
        blocked = True
    else:
        wards.animate(attacker, "attackStart")
        blocked = bool(melee)
    time.sleep(1.5)
    health, power = wards.health(), wards.state()["wardPower"]
    xp_after = wards.restoration_xp()
    if blocked:
        assert health >= 1999.9 and 0 < power < 799, (health, power)
        if attack != "Firebolt":
            assert xp_after > xp_before
    else:
        assert health < 1999.9 and abs(power - 800) < 0.1, (health, power)
        assert abs(xp_after - xp_before) < 0.001
    wards.stop()


def melee_hit(wards, attacker, modifier):
    wards.p("Actor", "SetActorValue", ["OneHandedMod", float(modifier)], attacker)
    wards.settings(
        fMagnitudeMultiplier=10, bInstantCharge=1, fChargeRateMultiplier=0.01
    )
    wards.cast(maximum=800)
    wards.settings(bInstantCharge=0)
    wards.restore_health(attacker)
    before = wards.state()["wardPower"]
    wards.animate(attacker, "attackStart")
    hit = wards.wait(
        lambda s: s["wardPower"] < before - 0.5, "The melee attack did not hit the ward"
    )
    time.sleep(1.5)
    return before - hit["wardPower"]


def test_melee_damage_percentage_and_experience(wards, attacker):
    wards.equip_attacker(attacker, "0x1397E")
    assert (
        wards.p("Actor", "GetEquippedWeapon", [False], attacker)["formId"]
        == "0x0001397E"
    )
    xp_before = wards.restoration_xp()
    normal = melee_hit(wards, attacker, 0)
    xp_after = wards.restoration_xp()
    assert normal > 1 and wards.health() >= 1999.9 and xp_after > xp_before
    boosted = melee_hit(wards, attacker, 25)
    assert normal < boosted < normal * 1.5, (normal, boosted)
    wards.stop()
    wards.restore_health(attacker)
    wards.animate(attacker, "attackStart")
    time.sleep(1.5)
    assert wards.health() < 1999.9


def test_unarmed_ward_damage(wards, attacker):
    wards.equip_attacker(attacker)
    damage = melee_hit(wards, attacker, 0)
    assert 8 < damage < 12 and wards.health() >= 1999.9
