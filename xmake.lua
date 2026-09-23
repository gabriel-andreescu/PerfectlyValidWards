set_xmakever("3.1.1")
set_project("PerfectlyValidWards")
set_license("GPL-3.0")
set_policy("package.requires_lock", true)

local version = "3.0.1"

add_repositories("bmk https://github.com/gabriel-andreescu/BethesdaModKit.git")
add_addons("bmk 0.3.0")
includes("@addon/bmk/project")
includes("@addon/bmk/native")

-- Dependencies
add_requires("commonlibsse-ng 9.0.0", { system = false })
add_requires("clib-util 1.5.0", { system = false })
add_requires("bmk", "devbench-api 2026.09.13", { system = false })
add_requires("catch2 3.15.2", { system = false })

add_requires("caprica", { host = true })
add_requires("skyrim-papyrus-sdk", { configs = { mcm = true } })
option("papyrus_imports", { description = "Papyrus import directories separated by ;" })
option("papyrus_flags", { description = "Optional Papyrus flags file override" })

-- Build targets
target("Native", function()
    set_default(false)
    set_basename("PerfectlyValidWards")
    set_version(version)
    add_rules("@commonlibsse-ng/plugin", {
        author = "GabonZ",
        description = "Make wards valid again",
    })
    add_rules("@addon/bmk/skyrim.plugin")
    add_files("$(projectdir)/src/native/**.cpp")
    add_includedirs("$(projectdir)/src/native")
    set_pcxxheader("src/native/PCH.h")
    add_defines("WIN32_LEAN_AND_MEAN", "NOGDI")
    add_packages("commonlibsse-ng", "clib-util")
    add_rules("@devbench-api/integration")
    add_packages("bmk", "devbench-api")
end)

target("SettingsTests", function()
    set_kind("binary")
    set_default(false)
    add_rules("platform.windows.subsystem")
    set_values("windows.subsystem", "console")
    add_rules("@addon/bmk/native.compiler")
    add_files("src/native/SettingsFile.cpp", "tests/SettingsTests.cpp")
    add_includedirs("src/native")
    add_packages("commonlibsse-ng", "clib-util", "bmk")
    add_packages("catch2", { components = { "main", "lib" } })
    add_tests("settings")
end)

target("Mutagen", function()
    set_default(false)
    add_extrafiles("src/mutagen/PerfectlyValidWards/FormIDs.txt")
    add_rules("@addon/bmk/dotnet", {
        project = "src/mutagen/PerfectlyValidWards/PerfectlyValidWards.csproj",
        arguments = { "$(outputdir)", path.absolute("src/mutagen/PerfectlyValidWards/FormIDs.txt") },
    })
    before_build(function()
        assert(
            not os.isfile(path.join(os.projectdir(), "assets/PerfectlyValidWards.esp")),
            "assets/PerfectlyValidWards.esp conflicts with the generated MCM variant. "
                .. "The MCM generator does not import this ESP. Add its records to the generator or configure a separate MCM plugin."
        )
    end)
end)

target("MCMScripts", function()
    set_default(false)
    add_rules("@addon/bmk/skyrim.papyrus", {
        root = "src/papyrus/mcm",
        imports = (get_config("papyrus_imports") or ""):split(";", { plain = true }),
        flags = get_config("papyrus_flags"),
        arguments = { "--strict", "--enable-language-extensions=true" },
    })
    add_packages("caprica", "skyrim-papyrus-sdk")
    add_files("$(projectdir)/src/papyrus/mcm/**.psc")
    add_installfiles("$(projectdir)/src/papyrus/mcm/(**.psc)", { prefixdir = "Source/Scripts" })
end)

-- Packages
target("PerfectlyValidWards", function()
    set_version(version)
    add_rules("@addon/bmk/skyrim.package", {
        targets = {
            "Native",
            "Mutagen",
        },
        nexus = {
            mod_id = "7318624425785",
            file_id = "3238990",
            category = "main",
            primary = true,
            display_name = "Perfectly Valid Wards",
            description = "Updating from an older version? Follow the Updating to 3.0.0 instructions in the mod description before installing.",
        },
    })
    add_installfiles("$(projectdir)/assets/(**)|optional/**")
    add_installfiles("$(builddir)/artifacts/Mutagen/main/(PerfectlyValidWards.esp)")
end)

target("PerfectlyValidWardsMCM", function()
    set_version(version)
    add_deps("Mutagen", { inherit = false })
    add_rules("@addon/bmk/skyrim.package", {
        targets = { "MCMScripts" },
        package_name = "Perfectly Valid Wards - MCM Addon",
        nexus = {
            mod_id = "7318624425785",
            file_id = "7459724",
            category = "optional",
            description = "MCM addon for Perfectly Valid Wards 3.0.1. Requires SkyUI and MCM Helper. Install after the main file and let it overwrite.",
        },
    })
    add_installfiles("$(builddir)/artifacts/Mutagen/mcm/(PerfectlyValidWards.esp)")
    add_installfiles("$(projectdir)/assets/optional/mcm/(**)")
end)
