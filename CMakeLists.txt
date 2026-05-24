cmake_minimum_required(VERSION 3.24...4.2)

set(PLUGIN_AUTHOR "GabonZ" CACHE STRING "Plugin author name")
set(PLUGIN_LICENSE "GPL-3.0-only" CACHE STRING "Plugin license")

set(VCPKG_DISABLE_METRICS ON)
set(VCPKG_INSTALL_OPTIONS "--no-print-usage")

project(
        PerfectlyValidWards
        VERSION 2.0.4
        DESCRIPTION "Make wards valid again"
        LANGUAGES CXX
)

set(OPTIONAL_ASSETS_VERSION 0.1.0)

option(PVW_ENABLE_PRECOMPILED_HEADERS "Enable CMake precompiled headers" ON)

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy run-clang-tidy.py)
find_package(Python3 COMPONENTS Interpreter QUIET)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a build directory and run CMake from there.")
endif ()

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

include(CheckIPOSupported)
check_ipo_supported(RESULT SUPPORTS_IPO OUTPUT IPO_OUTPUT)
if (SUPPORTS_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
endif ()

set(DEPLOY_DIR "" CACHE PATH "Deployment directory for base mod assets")
set(DEPLOY_DIR_OPTIONAL "" CACHE PATH "Deployment directory for optional mod assets")
set(ASSET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Skyrim/Data")

add_subdirectory(extern/CLibUtil)
add_subdirectory(extern/CommonLibSSE-NG)

include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY
)

set(SOURCES
        src/Effects.cpp
        src/Effects.h
        src/EventListener.cpp
        src/EventListener.h
        src/Events.h
        src/FormCache.cpp
        src/FormCache.h
        src/Hooks.cpp
        src/Hooks.h
        src/main.cpp
        src/Mechanics.cpp
        src/Mechanics.h
        src/Patches.cpp
        src/Patches.h
        src/PCH.h
        src/Physical.cpp
        src/Physical.h
        src/Reflection.cpp
        src/Reflection.h
        src/Settings.h
        src/Shouts.cpp
        src/Shouts.h
        src/Tweaks.cpp
        src/Tweaks.h
        src/WardMeter.cpp
        src/WardMeter.h
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

add_commonlibsse_plugin(
        ${PROJECT_NAME}
        SOURCES ${SOURCES}
        AUTHOR "${PLUGIN_AUTHOR}"
)

target_compile_features(
        ${PROJECT_NAME}
        PUBLIC
        cxx_std_23
)

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(CLibUtil PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:CLibUtil,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        CommonLibSSE::CommonLibSSE
        CLibUtil::CLibUtil
)

if (PVW_ENABLE_PRECOMPILED_HEADERS)
    target_precompile_headers(
            ${PROJECT_NAME}
            PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h"
    )
else ()
    target_compile_options(
            ${PROJECT_NAME}
            PRIVATE
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h>
    )
endif ()

if (MSVC)
    target_compile_options(
            ${PROJECT_NAME}
            PRIVATE
            /EHsc
            /W4
            /WX
            /utf-8
            /permissive-
            $<$<CONFIG:RelWithDebInfo>:/Ox>
    )

    target_link_options(
            ${PROJECT_NAME}
            PRIVATE
            $<$<CONFIG:Debug>:/DEBUG>
            $<$<CONFIG:Release>:/DEBUG>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG>
            $<$<CONFIG:RelWithDebInfo>:/OPT:REF>
            $<$<CONFIG:RelWithDebInfo>:/OPT:ICF>
            $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
    )
endif ()

add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/.cache"
        VERBATIM
)

if (DEPLOY_DIR)
    foreach (deploy_dir IN LISTS DEPLOY_DIR)
        set(PLUGIN_DIR "${deploy_dir}/SKSE/Plugins")

        add_custom_command(
                TARGET ${PROJECT_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Deploying to: ${PLUGIN_DIR}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${PLUGIN_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${PLUGIN_DIR}"
                VERBATIM
        )
    endforeach ()

    add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
            "-DASSET_SOURCE_DIR=${ASSET_SOURCE_DIR}"
            "-DDEPLOY_DIR=${DEPLOY_DIR}"
            "-DDEPLOY_DIR_OPTIONAL=${DEPLOY_DIR_OPTIONAL}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_assets.cmake"
            VERBATIM
    )
endif ()

set(DIST_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist" CACHE PATH "Directory where release ZIP files are written")

add_custom_target(
        ${PROJECT_NAME}_dist
        COMMAND "${CMAKE_COMMAND}"
        "-DMOD_NAME=${PROJECT_NAME}"
        "-DMOD_VERSION=${PROJECT_VERSION}"
        "-DOPTIONAL_ASSETS_VERSION=${OPTIONAL_ASSETS_VERSION}"
        "-DPLUGIN_DLL=$<TARGET_FILE:${PROJECT_NAME}>"
        "-DPLUGIN_PDB=$<TARGET_PDB_FILE:${PROJECT_NAME}>"
        "-DASSET_SOURCE_DIR=${ASSET_SOURCE_DIR}"
        "-DOUTPUT_DIR=${DIST_OUTPUT_DIR}"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/dist/$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_skse_mod.cmake"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Creating ${PROJECT_NAME} distribution archives"
        VERBATIM
)

if (PVW_ENABLE_PRECOMPILED_HEADERS)
    add_custom_target(
            ${PROJECT_NAME}_clang_tidy
            COMMAND "${CMAKE_COMMAND}" -E echo "clang-tidy is disabled in this PCH-enabled build tree."
            COMMAND "${CMAKE_COMMAND}" -E echo "Use: cmake --preset ninja-lint"
            COMMAND "${CMAKE_COMMAND}" -E echo "Then: cmake --build --preset clang-tidy"
            COMMAND "${CMAKE_COMMAND}" -E false
            COMMENT "clang-tidy requires the ninja-lint preset"
            VERBATIM
    )
elseif (CLANG_TIDY_EXECUTABLE)
    add_custom_target(
            ${PROJECT_NAME}_clang_tidy
            COMMAND "${CMAKE_COMMAND}"
            "-DCLANG_TIDY_EXECUTABLE=${CLANG_TIDY_EXECUTABLE}"
            "-DRUN_CLANG_TIDY_EXECUTABLE=${RUN_CLANG_TIDY_EXECUTABLE}"
            "-DPython3_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DCLANG_TIDY_JOBS=8"
            "-DCOMPILE_COMMANDS_FILE=${CMAKE_BINARY_DIR}/compile_commands.json"
            "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/src"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_clang_tidy.cmake"
            COMMENT "Running clang-tidy on ${PROJECT_NAME} source files"
            VERBATIM
    )
else ()
    add_custom_target(
            ${PROJECT_NAME}_clang_tidy
            COMMAND "${CMAKE_COMMAND}" -E false
            COMMENT "clang-tidy was not found"
            VERBATIM
    )
endif ()
