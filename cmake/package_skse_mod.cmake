set(main_zip "${OUTPUT_DIR}/${MOD_NAME}-${MOD_VERSION}.zip")
set(optional_zip "${OUTPUT_DIR}/${MOD_NAME}-${OPTIONAL_ASSETS_VERSION}-optional.zip")
set(main_stage "${WORK_DIR}/main")
set(optional_stage "${WORK_DIR}/optional")

include("${CMAKE_CURRENT_LIST_DIR}/copy_skyrim_assets.cmake")

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
execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar cf "${main_zip}" --format=zip ${main_entries}
        WORKING_DIRECTORY "${main_stage}"
        RESULT_VARIABLE main_zip_result
)

if (NOT main_zip_result EQUAL 0)
    message(FATAL_ERROR "Failed to create ${main_zip}")
endif ()

message(STATUS "Created ${main_zip}")

if (DEFINED ASSET_SOURCE_DIR AND EXISTS "${ASSET_SOURCE_DIR}/optional")
    copy_optional_skyrim_assets("${ASSET_SOURCE_DIR}" "${optional_stage}" optional_assets_copied)

    if (optional_assets_copied)
        file(GLOB optional_entries RELATIVE "${optional_stage}" "${optional_stage}/*")
        execute_process(
                COMMAND "${CMAKE_COMMAND}" -E tar cf "${optional_zip}" --format=zip ${optional_entries}
                WORKING_DIRECTORY "${optional_stage}"
                RESULT_VARIABLE optional_zip_result
        )

        if (NOT optional_zip_result EQUAL 0)
            message(FATAL_ERROR "Failed to create ${optional_zip}")
        endif ()

        message(STATUS "Created ${optional_zip}")
    else ()
        message(STATUS "Skipping optional archive because ${ASSET_SOURCE_DIR}/optional is empty")
    endif ()
endif ()
