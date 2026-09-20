import json
import statistics
import time

import pytest
from bmk.skyrim.devbench import Keyboard


def released(wards):
    wards.wait(
        lambda s: (
            s["left"]["state"] == 0 and s["wardPower"] < 0.01 and not s["wardEffects"]
        ),
        "Releasing the casting key retained a ward",
    )
    time.sleep(0.4)
    state = wards.state()
    assert state["wardPower"] < 0.01 and not state["wardEffects"]


def prepare(wards, spell="0x211F0", camera="first"):
    wards.p("Game", "ForceFirstPerson" if camera == "first" else "ForceThirdPerson")
    wards.p("Actor", "EquipSpell", [{"form": spell}, 0], "0x14")
    wards.p("Actor", "DrawWeapon", self_form="0x14")
    time.sleep(1.5)


@pytest.mark.keyboard
@pytest.mark.parametrize("camera", ["first", "third"])
@pytest.mark.parametrize(
    "spell,power", [("0x13018", 40), ("0x211F1", 60), ("0x211F0", 80)]
)
def test_instant_cast_latency_and_release(wards, game_artifacts, camera, spell, power):
    prepare(wards, spell, camera)
    wards.settings(bInstantCharge=1, bRestrictToPlayerTeam=1)
    measurements = []
    with Keyboard(wards.client) as keys:
        key = keys.mapped_key("Left Attack/Block")
        for enabled in (0, 1, 0):
            wards.settings(bInstantCast=enabled)
            elapsed = []
            for _ in range(3):
                start = time.monotonic()
                with keys.hold(key, max_hold_ms=3000):
                    state = wards.wait(
                        lambda s: abs(s["wardPower"] - power) < 0.1,
                        "Ward did not reach full power",
                        timeout=2,
                    )
                    elapsed.append(time.monotonic() - start)
                    assert len(state["wardEffects"]) == 1
                released(wards)
            measurements.append({"enabled": enabled, "seconds": elapsed})
    (game_artifacts / "latency.json").write_text(json.dumps(measurements, indent=2))
    normal, fast, restored = [statistics.median(m["seconds"]) for m in measurements]
    assert fast < 0.18, measurements
    assert min(normal, restored) - fast > 0.10, measurements


@pytest.mark.keyboard
def test_short_ward_taps_and_empty_magicka(wards):
    prepare(wards)
    wards.settings(bInstantCast=1, bInstantCharge=1)
    with Keyboard(wards.client) as keys:
        key = keys.mapped_key("Left Attack/Block")
        for _ in range(5):
            with keys.hold(key, max_hold_ms=1000):
                time.sleep(0.06)
            released(wards)
        wards.p("Actor", "DamageActorValue", ["Magicka", 10000.0], "0x14")
        with keys.hold(key, max_hold_ms=1000):
            time.sleep(0.5)
            assert wards.state()["wardPower"] < 0.01
        released(wards)


@pytest.mark.keyboard
def test_instant_cast_preserves_charging_and_magicka_cost(wards):
    prepare(wards)
    costs = []
    with Keyboard(wards.client) as keys:
        key = keys.mapped_key("Left Attack/Block")
        for enabled in (0, 1):
            wards.settings(bInstantCast=enabled, bInstantCharge=0)
            with keys.hold(key, max_hold_ms=5000):
                state = wards.wait(
                    lambda s: s["wardPower"] > 0, "Ward did not start charging"
                )
                assert state["wardPower"] < 40
                wards.wait(
                    lambda s: s["wardPower"] > 79.9, "Ward did not finish charging"
                )
                before = wards.p("Actor", "GetActorValue", ["Magicka"], "0x14")
                start = time.monotonic()
                time.sleep(1.5)
                after = wards.p("Actor", "GetActorValue", ["Magicka"], "0x14")
                costs.append((before - after) / (time.monotonic() - start))
                assert len(wards.state()["wardEffects"]) == 1
            released(wards)
    assert costs[0] > 0 and 0.9 < costs[1] / costs[0] < 1.1, costs


@pytest.mark.keyboard
def test_instant_cast_skips_effect_charge_time(wards, game_artifacts):
    prepare(wards)
    wards.settings(bInstantCharge=1)
    effect = wards.p("Spell", "GetNthEffectMagicEffect", [0], "0x211F0")["formId"]
    original = wards.p("MagicEffect", "GetCastTime", self_form=effect)
    measurements = []
    try:
        wards.p("MagicEffect", "SetCastTime", [0.6], effect)
        with Keyboard(wards.client) as keys:
            key = keys.mapped_key("Left Attack/Block")
            for enabled in (0, 1):
                wards.settings(bInstantCast=enabled)
                start = time.monotonic()
                with keys.hold(key, max_hold_ms=3000):
                    state = wards.wait(
                        lambda s: s["wardPower"] > 79.9, "Ward did not start"
                    )
                    assert state["left"]["chargeTime"] == pytest.approx(0.6)
                    measurements.append(time.monotonic() - start)
                released(wards)
    finally:
        wards.p("MagicEffect", "SetCastTime", [float(original)], effect)
    (game_artifacts / "charge-time.json").write_text(json.dumps(measurements))
    assert measurements[0] > 0.6 and measurements[1] < 0.18, measurements


@pytest.mark.keyboard
def test_instant_cast_does_not_shorten_firebolt_charge(wards):
    prepare(wards, "0x12FD0")
    wards.settings(bInstantCast=1)
    with (
        Keyboard(wards.client) as keys,
        keys.hold(keys.mapped_key("Left Attack/Block"), max_hold_ms=3000),
    ):
        state = wards.wait(
            lambda s: s["left"]["castingTimer"] > 0.1,
            "Firebolt lost its charge time",
        )
        assert state["left"]["spell"] == 0x12FD0 and not state["wardEffects"]
    wards.stop()


@pytest.mark.keyboard
@pytest.mark.parametrize("projectile", ["arrow", "Firebolt"])
def test_ward_protection_during_cast_start(wards, attacker, game_artifacts, projectile):
    wards.equip_attacker(attacker, "0x3B562", "0x1397F", -300.0)
    prepare(wards)
    wards.settings(bInstantCharge=1)
    observations = []
    with Keyboard(wards.client) as keys:
        key = keys.mapped_key("Left Attack/Block")
        for enabled in (0, 1):
            wards.settings(bInstantCast=enabled)
            wards.restore_health(attacker)
            start = time.monotonic()
            with keys.hold(key, max_hold_ms=3000):
                time.sleep(0.01)
                if projectile == "arrow":
                    wards.p(
                        "Weapon",
                        "Fire",
                        [{"form": attacker}, {"form": "0x1397F"}],
                        "0x3B562",
                    )
                else:
                    wards.enemy_spell(attacker, "0x12FD0")
                fired = time.monotonic() - start
                time.sleep(0.8)
                health = wards.health()
            observations.append({"enabled": enabled, "fired": fired, "health": health})
            released(wards)
    (game_artifacts / "early-hit.json").write_text(json.dumps(observations, indent=2))
    assert all(o["fired"] < 0.20 for o in observations), observations
    assert observations[0]["health"] < 1999.9, observations
    assert observations[1]["health"] >= 1999.9, observations
