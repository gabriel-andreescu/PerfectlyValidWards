import pytest
from bmk.testing import wait_for


@pytest.mark.parametrize("hand", ["left", "right"])
def test_ward_meter(wards, hand):
    wards.settings(bShowWardMeter=1, bInstantCharge=1)
    wards.cast(hand=hand)
    root = f"_root.HUDMovieBaseInstance.{hand.title()}ChargeMeter"

    def value(path):
        return wards.p("UI", "GetFloat", ["HUD Menu", path])

    wait_for(
        lambda: value(root + ".CurrentPercent"),
        lambda percent: abs(percent - 100) < 0.1,
        "The ward meter did not fill",
    )
    assert value(root + "Anim._currentframe") > 1
    wards.settings(bInstantCharge=0, fChargeRateMultiplier=0.01)
    wards.p("Actor", "DamageActorValue", ["WardPower", 40.0], "0x14")
    wait_for(
        lambda: value(root + ".CurrentPercent"),
        lambda percent: 45 < percent < 60,
        "The meter did not show ward damage",
    )
    wards.stop()
    wait_for(
        lambda: value(root + "Anim._currentframe"),
        lambda frame: frame == 1,
        "The ward meter did not fade after casting stopped",
    )
