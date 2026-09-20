import time

import pytest

FEATURES = ["physical", "shout", "disease", "cloak", "reflection"]
PERK = "0xF2CAA"


def exercise(wards, attacker, feature, enabled):
    wards.settings(
        bEnableSpellReflection=1,
        bInstantCharge=1,
        fMagnitudeMultiplier=10,
        fChargeRateMultiplier=0.01,
        iShoutMode=1,
        bShoutInstantBreak=0,
        fShoutDamage=40,
    )
    if feature == "physical":
        wards.equip_attacker(attacker, "0x1397E")
    elif feature == "cloak":
        # Stay inside the cloak radius and outside the attacker's melee reach.
        wards.p(
            "ObjectReference",
            "MoveTo",
            [{"form": "0x14"}, -200.0, 0.0, 0.0, False],
            attacker,
        )
    wards.p("Actor", "EquipSpell", [{"form": "0x211F0"}, 1], "0x14")
    time.sleep(1.2)
    wards.cast(maximum=800, hand="right")
    wards.settings(bInstantCharge=0)
    wards.restore_health(attacker)
    if feature == "physical":
        wards.animate(attacker, "attackStart")
    elif feature == "disease":
        wards.p(
            "Actor",
            "DoCombatSpellApply",
            [{"form": "0x10A24A"}, {"form": "0x14"}],
            attacker,
        )
    else:
        spell = {"shout": "0x3F9EB", "cloak": "0x3AE9F", "reflection": "0x12FD0"}[
            feature
        ]
        wards.enemy_spell(attacker, spell, attacker if feature == "cloak" else "0x14")
    time.sleep(3 if feature == "cloak" else 1.5)
    state = wards.state()
    health = wards.health()
    if feature == "disease":
        effects = wards.call("inspect", {"kind": "effects"})["activeEffects"]
        disease = any(e["spell"]["formId"] == "0x0010A24A" for e in effects)
        assert disease != enabled
    elif feature == "reflection":
        assert health >= 1999.9
        assert (wards.health(attacker) < 1990) == enabled
    elif feature == "shout":
        power = state["wardPower"]
        assert 757 < power < 765 if enabled else power > 799
    elif enabled:
        assert health >= 1999.9 and state["wardPower"] < 799, (health, state)
    else:
        assert health < 1999.9 and state["wardPower"] > 799, (health, state)


@pytest.mark.startup("perks")
@pytest.mark.parametrize("feature", FEATURES)
@pytest.mark.parametrize("owned", [False, True])
def test_required_perks(wards, attacker, feature, owned):
    wards.p("Actor", "AddPerk" if owned else "RemovePerk", [{"form": PERK}], "0x14")
    assert wards.p("Actor", "HasPerk", [{"form": PERK}], "0x14") == owned
    exercise(wards, attacker, feature, owned)


@pytest.mark.startup("exclusions")
@pytest.mark.parametrize("feature", FEATURES)
@pytest.mark.parametrize("disabled", [False, True])
def test_excluded_ward_item(wards, attacker, feature, disabled):
    key = {
        "physical": "bExcludedItemsDisablePhysicalBlocking",
        "shout": "bExcludedItemsDisableShoutMechanics",
        "disease": "bExcludedItemsDisableDiseaseBlocking",
        "cloak": "bExcludedItemsDisableCloakBlocking",
        "reflection": "bExcludedItemsDisableReflection",
    }[feature]
    wards.settings(**{key: int(disabled)})
    wards.p("ObjectReference", "AddItem", [{"form": "0x45F96"}, 1, True], "0x14")
    wards.p("Actor", "EquipItem", [{"form": "0x45F96"}, False, True], "0x14")
    exercise(wards, attacker, feature, not disabled)
