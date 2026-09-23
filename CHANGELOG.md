# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed

- Fix a crash when arrows or bolts hit wards with older versions of Cold Breath
  NG installed
- Include arrow and bolt damage in physical damage calculations for wards

## [3.0.0] - 2026-09-20

### Added

- Configure melee blocking separately from arrow and bolt blocking
- Add an option to skip the casting delay before a ward activates
- Add an MCM toggle for debug logging

### Changed

- **Breaking change:** Remove automatic import of settings from
  `SKSE/Plugins/PerfectlyValidWards.ini`
- **Breaking change:** Change INI form lists from `Plugin.esp|0xFORMID` to
  `0xFORMID~Plugin.esp`

### Fixed

- Support Skyrim 1.7.104
- Apply the magnitude multiplier to maximum ward power
- Apply ward tweaks to the player and followers when the player-team restriction
  is enabled
- Stop reflected spells from striking the same ward again
- Apply the cost multiplier to wards with automatically calculated costs
- Calculate melee damage bonuses as percentages when draining wards
- Pass shouts through wards only when the ward breaks
- Preserve shout tuning when switching to Vanilla mode
- Reject NaN and infinite values in numeric settings

## [2.1.0] - 2026-05-25

### Added

- Add MCM support

### Fixed

- Make Instant Ward Charge fully charge wards when enabled

## [2.0.4] - 2026-04-26

### Fixed

- Prevent ward checks from crashing on non-character targets
- Stop concentration shouts from passing through wards to prevent stuck or
  replayed audio
- Prevent rare crashes when wards block diseases or cloak effects
