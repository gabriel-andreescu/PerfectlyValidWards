cmake_minimum_required(VERSION 3.31)

set(BUILD_TESTS OFF)
set(VCPKG_DISABLE_METRICS ON)
set(VCPKG_INSTALL_OPTIONS "--no-print-usage")

project(
        PerfectlyValidWards
        VERSION 1.0.0
        LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Build options
option(AUTO_PLUGIN_DEPLOYMENT "Copy the build output to env:AUTO_PLUGIN_DEPLOYMENT_DIRS." OFF)

message(STATUS "")
message(STATUS "==== Plugin Build Configuration ====")
message(STATUS "  Auto plugin deployment: ${AUTO_PLUGIN_DEPLOYMENT}")
message(STATUS "====================================")

add_compile_definitions(SKYRIM)
set(CommonLibPath "extern/CommonLibSSE-NG")
set(CommonLibName "CommonLibSSE")

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Plugin.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/Plugin.h
        @ONLY
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/version.rc
        @ONLY
)

add_library("${PROJECT_NAME}" SHARED
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/Plugin.h
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/version.rc
        src/PCH.h
        src/Settings.h
        src/WardManager.h
        src/Hooks.h
        src/EventListener.h
        src/main.cpp
        src/WardManager.cpp
        src/Hooks.cpp
        src/EventListener.cpp)

target_compile_features(
        "${PROJECT_NAME}"
        PRIVATE
        cxx_std_23
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_definitions(_UNICODE)

    target_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<CONFIG:DEBUG>:DEBUG>")

    set(SC_DEBUG_OPTS "/fp:strict;/ZI;/Od;/Gy")
    set(SC_RELEASE_OPTS "/Zi;/fp:fast;/GL;/Gy-;/Gm-;/Gw;/sdl-;/GS-;/guard:cf-;/O2;/Ob2;/Oi;/Ot;/Oy;/fp:except-")

    target_compile_options(
            "${PROJECT_NAME}"
            PRIVATE
            /MP
            /W4
            /WX
            /permissive-
            /Zc:alignedNew
            /Zc:auto
            /Zc:__cplusplus
            /Zc:externC
            /Zc:externConstexpr
            /Zc:forScope
            /Zc:hiddenFriend
            /Zc:implicitNoexcept
            /Zc:lambda
            /Zc:noexceptTypes
            /Zc:preprocessor
            /Zc:referenceBinding
            /Zc:rvalueCast
            /Zc:sizedDealloc
            /Zc:strictStrings
            /Zc:ternary
            /Zc:threadSafeInit
            /Zc:trigraphs
            /Zc:wchar_t
            /wd4200 # nonstandard extension used : zero-sized array in struct/union
    )

    target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:${SC_DEBUG_OPTS}>")
    target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:${SC_RELEASE_OPTS}>")

    target_link_options(
            ${PROJECT_NAME}
            PRIVATE
            /WX
            "$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
            "$<$<CONFIG:RELEASE>:/LTCG;/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
    )
endif ()

add_subdirectory(${CommonLibPath} ${CommonLibName} EXCLUDE_FROM_ALL)

find_package(spdlog CONFIG REQUIRED)

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        CommonLibSSE::CommonLibSSE
)

# Find dependencies
find_package(xbyak CONFIG REQUIRED)

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        xbyak::xbyak
)

# Handle deployment
if (AUTO_PLUGIN_DEPLOYMENT)
    if (NOT DEFINED ENV{AUTO_PLUGIN_DEPLOYMENT_DIRS})
        message(WARNING "When using AUTO_PLUGIN_DEPLOYMENT, set env var 'AUTO_PLUGIN_DEPLOYMENT_DIRS'")
    else ()
        string(REPLACE ";" ";" DEPLOY_TARGETS "$ENV{AUTO_PLUGIN_DEPLOYMENT_DIRS}")
        foreach (DEPLOY_TARGET ${DEPLOY_TARGETS})
            if (EXISTS "${DEPLOY_TARGET}")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E echo "Deploying to ${DEPLOY_TARGET}"
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_TARGET}/SKSE/Plugins"
                        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_TARGET}/SKSE/Plugins/"
                        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${DEPLOY_TARGET}/SKSE/Plugins/"
                )
            else ()
                message(WARNING "Deployment target '${DEPLOY_TARGET}' does not exist. Skipping.")
            endif ()
        endforeach ()
    endif ()
endif ()
