# PerfectlyValidWards

Make wards valid again

This project uses
[BethesdaModKit (BMK)](https://github.com/gabriel-andreescu/BethesdaModKit) for
project generation and development tooling.

## Development

```powershell
xmake
xmake package
```

- [Build instructions](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/projects.md#build-a-generated-project)
- [Deployment and packaging](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/packaging.md)
- [Formatting setup](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/defaults.md#formatting)
- [Clang tooling](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/clang.md)
- [DevBench native integration](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/skyrim/devbench.md#native-api)
- Papyrus sources require
  [Caprica](https://github.com/gabriel-andreescu/Caprica) with language
  extensions enabled.
- [Settings and MCM](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/settings.md)

Run the native settings tests with `xmake test SettingsTests/settings`.

## Tests

See
[DevBench setup](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/defaults.md#devbench)
and the
[Python client and fixtures](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/skyrim/devbench.md).

See [the game test prerequisites](tests/game/README.md) before running the
mod-specific suite.

## CI

See
[workflow setup](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/github-actions.md)
for build inputs and releases.
