# PerfectlyValidWards

This project uses
[BethesdaModKit](https://github.com/gabriel-andreescu/BethesdaModKit) for
project generation and development tooling. Follow BMK's
[project build instructions](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/projects.md#build-a-generated-project),
[deployment and packaging guide](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/packaging.md),
[formatting setup](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/defaults.md#formatting),
[Clang tooling](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/tooling/clang.md),
and the
[settings and MCM guide](https://github.com/gabriel-andreescu/BethesdaModKit/blob/main/docs/mod-authors/template/settings.md).

Papyrus sources require [Caprica](https://github.com/gabriel-andreescu/Caprica)
with language extensions enabled.

Run the native settings tests with `xmake test SettingsTests/settings`. See
[the in-game test prerequisites](tests/game/README.md) before running the
mod-specific suite.
