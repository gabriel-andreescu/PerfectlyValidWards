cmake_minimum_required(VERSION 3.31)

set(VCPKG_DISABLE_METRICS ON)
# optional
set(VCPKG_INSTALL_OPTIONS "--no-print-usage")

project(
        TemplatePluginNG
        VERSION 1.0.0
        LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Build options
option(AUTO_PLUGIN_DEPLOYMENT "Copy the build output to env:AUTO_PLUGIN_DEPLOYMENT_DIRS." OFF)
option(ZIP_TO_DIST "Zip the build output to their own 7z file in .\\dist." ON)
option(WAIT_FOR_DEBUGGER "Wait for debugger to attach in debug mode" OFF)

message(STATUS "")
message(STATUS "==== Plugin Build Configuration ====")
message(STATUS "  Auto plugin deployment: ${AUTO_PLUGIN_DEPLOYMENT}")
message(STATUS "  Zip to dist:            ${ZIP_TO_DIST}")
message(STATUS "  Wait for debugger:      ${WAIT_FOR_DEBUGGER}")
message(STATUS "====================================")

if (WAIT_FOR_DEBUGGER)
    add_compile_definitions(WAIT_FOR_DEBUGGER)
endif ()

# Add CMake features
include(XSEPlugin)

# Find dependencies
find_package(xbyak CONFIG REQUIRED)

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        xbyak::xbyak
)

# Handle packaging
include(Packaging)
