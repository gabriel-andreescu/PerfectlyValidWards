import time

import pytest


@pytest.mark.parametrize("hand", ["left", "right"])
def test_casting_and_interruption(wards, hand):
    state = wards.cast(hand=hand)
    assert abs(state["maximumWardPower"] - 80) < 0.1
    wards.stop()


@pytest.mark.parametrize("magnitude", [2, 1])
def test_magnitude(wards, magnitude):
    wards.settings(fMagnitudeMultiplier=magnitude)
    wards.cast(maximum=80 * magnitude)


def test_slow_and_instant_charge(wards):
    wards.stop()
    wards.settings(fChargeRateMultiplier=0.1)
    wards.call("console", {"command": "player.cast 211f0 14 left"})
    slow = wards.wait(
        lambda s: s["left"]["state"] == 6 and s["wardPower"] > 0,
        "Slow ward did not begin charging",
    )
    assert slow["wardPower"] < 40
    wards.settings(bInstantCharge=1)
    wards.wait(lambda s: s["wardPower"] >= 79.9, "Instant charge did not fill the ward")


def test_magicka_cost_multiplier(wards):
    costs = []
    for cost in (1, 2):
        wards.settings(fCostMultiplier=cost, bInstantCharge=1, fChargeRateMultiplier=1)
        wards.cast()
        before = wards.p("Actor", "GetActorValue", ["Magicka"], "0x14")
        start = time.monotonic()
        time.sleep(2)
        after = wards.p("Actor", "GetActorValue", ["Magicka"], "0x14")
        costs.append((before - after) / (time.monotonic() - start))
        wards.stop()
    assert costs[0] > 0 and 1.8 < costs[1] / costs[0] < 2.2, costs
