import time


def test_armor_experience_after_ward_block(wards, attacker):
    wards.equip_attacker(attacker, "0x1397E")
    for item in ("0x12E49", "0x12E4D", "0x12E4B", "0x12E46"):
        wards.p("ObjectReference", "AddItem", [{"form": item}, 1, True], "0x14")
        wards.p("Actor", "EquipItem", [{"form": item}, False, True], "0x14")
    info = wards.p("ActorValueInfo", "GetActorValueInfoByName", ["HeavyArmor"])[
        "formId"
    ]

    def armor_xp():
        return wards.p("ActorValueInfo", "GetSkillExperience", self_form=info)

    for ward in (False, True, False):
        if ward:
            wards.settings(bInstantCharge=1)
            wards.cast()
        else:
            wards.stop()
        wards.restore_health(attacker)
        armor_before, restoration_before = armor_xp(), wards.restoration_xp()
        wards.animate(attacker, "attackStart")
        time.sleep(1.5)
        if ward:
            assert wards.health() >= 1999.9
            assert abs(armor_xp() - armor_before) < 0.001
            assert wards.restoration_xp() > restoration_before
        else:
            assert wards.health() < 1999.9
            assert armor_xp() > armor_before, (
                "A previous ward block suppressed normal armor XP"
            )


def test_ward_blocks_grant_only_restoration_experience(wards, attacker):
    wards.equip_attacker(attacker, "0x1397E")
    wards.settings(bInstantCharge=1)
    wards.cast()
    info = wards.p("ActorValueInfo", "GetActorValueInfoByName", ["Block"])["formId"]
    before = wards.p("ActorValueInfo", "GetSkillExperience", self_form=info)
    restoration = wards.restoration_xp()
    wards.animate(attacker, "attackStart")
    time.sleep(1.5)
    assert wards.health() >= 1999.9
    assert wards.restoration_xp() > restoration
    assert (
        abs(wards.p("ActorValueInfo", "GetSkillExperience", self_form=info) - before)
        < 0.001
    )
