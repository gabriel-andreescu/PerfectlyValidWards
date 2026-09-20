from pathlib import Path

import pytest
from bmk.testing import preserved_file

from .support import WardSession

pytest_plugins = ["bmk.skyrim.devbench.pytest_plugin"]


def pytest_addoption(parser):
    group = parser.getgroup("perfectly-valid-wards")
    group.addoption(
        "--settings", type=Path, help="Physical MCM/Settings/PerfectlyValidWards.ini"
    )
    group.addoption("--baseline", default="PerfectlyValidWardsTest_QASmoke")
    group.addoption(
        "--keyboard-tests", action="store_true", help="Include keyboard casting cases"
    )
    group.addoption(
        "--startup-fixture",
        choices=["perks", "exclusions"],
        help="Run a configured startup group",
    )


def pytest_collection_modifyitems(config, items):
    for item in items:
        item.add_marker(pytest.mark.game)
    selected, deselected = [], []
    for item in items:
        startup = item.get_closest_marker("startup")
        group = startup.args[0] if startup else None
        include = group == config.getoption("startup_fixture")
        include &= config.getoption("keyboard_tests") or not item.get_closest_marker(
            "keyboard"
        )
        (selected if include else deselected).append(item)
    config.hook.pytest_deselected(items=deselected)
    items[:] = selected


@pytest.fixture
def wards(request, devbench, game_artifacts):
    settings = request.config.getoption("settings")
    if settings is None:
        raise pytest.UsageError("PerfectlyValidWards tests require --settings")
    bench = WardSession(devbench, settings, request.config.getoption("baseline"))
    with preserved_file(settings, game_artifacts / "settings-before.ini"):
        loaded = False
        try:
            bench.restore()
            loaded = True
            bench.mcm_quest = (
                f"0x{devbench.form_id(0xD66, 'PerfectlyValidWards.esp'):08X}"
            )
            bench.initialize()
            yield bench
        finally:
            settings.write_bytes((game_artifacts / "settings-before.ini").read_bytes())
            if loaded:
                bench.restore()
                if bench.mcm_quest:
                    bench.reload_settings()
                assert abs(bench.state()["wardPower"]) < 0.01, (
                    "Loading retained ward power"
                )


@pytest.fixture
def attacker(wards):
    return wards.attacker()
