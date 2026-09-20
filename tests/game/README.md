# In-game tests

Pytest tests for ward casting, charging, magicka cost, physical and magical
blocking, reflection, perk requirements, exclusions and HUD meter state through
[DevBench](https://github.com/alandtse/devbench).

## Requirements

- Python 3.11 or newer and the BMK Python dependencies declared by this project.
- A running game with [DevBench](https://github.com/alandtse/devbench) 1.18.1 or
  newer.
- PerfectlyValidWards, its MCM addon and the addon's requirements.
- Default perk requirements and exclusion lists for the ordinary suite. Startup
  groups below use explicit alternatives. Combat or magic overhauls can change
  the measured outcomes.
- A disposable QASmoke save named `PerfectlyValidWardsTest_QASmoke`, with the
  player standing outside combat and no active ward.

Run `prepare.json` through DevBench's `scenario` tool or the `devbench-scenario`
command to create the save. This replaces a save with the same name. Keep the
default QASmoke arrival position clear so the suite can place an attacker in
front of the player.

## Run

Install the locked development dependencies:

```powershell
uv sync --locked
```

See BMK's
[DevBench fixture reference](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/skyrim/devbench.md)
for client and pytest behavior.

Pass the physical path of the effective `MCM/Settings/PerfectlyValidWards.ini`.
For a mod manager, this may be in its overwrite directory. Launch the game once
to create the file before running the suite.

```powershell
uv run pytest tests/game --game-tests `
  --devbench-url http://127.0.0.1:8920 `
  --settings 'C:/Games/ModOrganizer/overwrite/MCM/Settings/PerfectlyValidWards.ini' `
  --game-results test-results/game `
  --junitxml test-results/game.xml
```

Use `--baseline` to select a differently named baseline. Use pytest's `-k` or a
test path to run a subset. Without `--game-tests`, the cases are skipped before
connecting. Run serially, without pytest-xdist workers.

Add `--keyboard-tests` to include real keyboard casting and release. Bind Left
Attack/Block to a keyboard key.

## Startup groups

Perk requirements and exclusion lists are cached at startup. Back up the user
INI, configure one group below, then restart Skyrim. Append
`--startup-fixture perks` or `--startup-fixture exclusions` to run only that
group. Restore the INI and restart again when finished.

- `perks`: set all five `sPhysicalRequiredPerks`, `sShoutsRequiredPerks`,
  `sDiseaseRequiredPerks`, `sCloakRequiredPerks` and `sReflectionRequiredPerks`
  entries to `0xF2CAA~Skyrim.esm` (Novice Restoration). Leave `sExcludedItems`
  empty. Each feature is tested with and without the perk.
- `exclusions`: leave all required-perk lists empty and set `sExcludedItems` to
  `0x45F96~Skyrim.esm` (Spellbreaker). Each exclusion toggle is tested with the
  shield equipped and a ward in the other hand.

## Results and cleanup

Each test writes `settings-before.ini` and `transcript.json` under
`--game-results`. Tests restore settings and reload the baseline. If a run is
interrupted, restore the settings from that backup and reload the baseline
before continuing.
