# Perfectly Valid Wards

Make wards valid again.

---

## Clone and Build

Open terminal (e.g., PowerShell) and run the following commands:

```
git clone --recurse-submodules -j8 https://github.com/gabriel-andreescu/PerfectlyValidWards.git
cd PerfectlyValidWards
cmake --preset default
cmake --build build --config Release
```

Optionally:

```
cp CMakeUserPresets.json.example CMakeUserPresets.json
```

### **Debugging**

- [Steamless](https://github.com/atom0s/Steamless/releases)

- build [SKSE](https://github.com/ianpatt/skse64) from sources with the Debug config
- copy the built files and their PDB to the Skyrim folder
- run `skse64_loader.exe`
- attach debugger to `SkyrimSE.exe`

### **Deployment**

Use the user preset (see `CMakeUserPresets.json.example`) to automatically copy the plugin to your Skyrim Data
directory, if configured.

```
cmake --preset user-default
cmake --build --preset release
```

For deployment to multiple targets split the paths with a `;` (e.g., `C:/path1/data;C:/path2/data`)
Set `DEPLOY_DIR_OPTIONAL` to deploy the optional MCM addon to a separate MO2 mod, such as
`C:/MO2/mods/PerfectlyValidWards - MCM Addon`.

---

## Requirements

- [Git](https://git-scm.com/downloads)
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
    - Desktop development with C++
- [CMake](https://cmake.org/)
    - Add the cmake.exe install path to the `PATH` environment variable
- [Vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-powershell#1---set-up-vcpkg)
    - Add a new `VCPKG_ROOT` environment variable pointing to the root folder of vcpkg (e.g., `C:\vcpkg`)

This project is developed using the **non-commercial** version of [CLion](https://www.jetbrains.com/clion/)

### **Register Visual Studio as a Generator**

Open PowerShell and run the following command:

```
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64
```

---

## User Requirements

- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444) - needed for SE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101) - needed for VR

---

## Credits

Thanks to the entire open source community, it provided a ton of invaluable information during my learning.
A special thanks to [Doodlum](https://github.com/doodlum), who pointed me in the right direction when I started.

The following projects and repositories were directly used or served as important references:

- [CommonLibSSE NG](https://github.com/alandtse/CommonLibVR/tree/ng)
- [CLibUtil](https://github.com/powerof3/CLibUtil)
- [skyrim-community-shaders](https://github.com/doodlum/skyrim-community-shaders)
- [CLibNGPluginTemplate](https://github.com/ThirdEyeSqueegee/CLibNGPluginTemplate)
- [powerof3's repos](https://github.com/powerof3)
- [Monitor221hz's repos](https://github.com/Monitor221hz)
- ThirdEyeSqueegee's repos
- the xRE SE Discord channel

---
