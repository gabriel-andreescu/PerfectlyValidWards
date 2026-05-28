set(main_zip "${OUTPUT_DIR}/${MOD_NAME}-${MOD_VERSION}.zip")
set(optional_zip "${OUTPUT_DIR}/${MOD_NAME}-${OPTIONAL_ASSETS_VERSION}-optional.zip")
set(main_stage "${WORK_DIR}/main")
set(optional_stage "${WORK_DIR}/optional")

include("${CMAKE_CURRENT_LIST_DIR}/copy_skyrim_assets.cmake")

find_program(SEVEN_ZIP_EXECUTABLE NAMES 7z 7za 7zr)

function(write_zip archive_path stage_dir entries)
    execute_process(
            COMMAND "${SEVEN_ZIP_EXECUTABLE}" a -tzip -mx=9 "${archive_path}" ${entries}
            WORKING_DIRECTORY "${stage_dir}"
            RESULT_VARIABLE zip_result
    )

    if (NOT zip_result EQUAL 0)
        message(FATAL_ERROR "Failed to create ${archive_path}")
    endif ()
endfunction()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(REMOVE_RECURSE "${WORK_DIR}")
file(REMOVE "${main_zip}" "${optional_zip}")

file(MAKE_DIRECTORY "${main_stage}/SKSE/Plugins")
file(COPY "${PLUGIN_DLL}" DESTINATION "${main_stage}/SKSE/Plugins")
file(COPY "${PLUGIN_PDB}" DESTINATION "${main_stage}/SKSE/Plugins")

if (DEFINED ASSET_SOURCE_DIR)
    copy_base_skyrim_assets("${ASSET_SOURCE_DIR}" "${main_stage}")
endif ()

file(GLOB main_entries RELATIVE "${main_stage}" "${main_stage}/*")
write_zip("${main_zip}" "${main_stage}" "${main_entries}")
message(STATUS "Created ${main_zip}")

if (DEFINED ASSET_SOURCE_DIR AND EXISTS "${ASSET_SOURCE_DIR}/optional")
    copy_optional_skyrim_assets("${ASSET_SOURCE_DIR}" "${optional_stage}" optional_assets_copied)

    if (optional_assets_copied)
        file(GLOB optional_entries RELATIVE "${optional_stage}" "${optional_stage}/*")
        write_zip("${optional_zip}" "${optional_stage}" "${optional_entries}")
        message(STATUS "Created ${optional_zip}")
    else ()
        message(STATUS "Skipping optional archive because ${ASSET_SOURCE_DIR}/optional is empty")
    endif ()
endif ()
