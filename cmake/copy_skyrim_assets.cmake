function(copy_base_skyrim_assets source_dir destination_dir)
    if (NOT EXISTS "${source_dir}")
        return()
    endif ()

    file(MAKE_DIRECTORY "${destination_dir}")

    file(GLOB asset_entries LIST_DIRECTORIES true "${source_dir}/*")
    foreach (asset_entry IN LISTS asset_entries)
        get_filename_component(asset_name "${asset_entry}" NAME)
        string(TOLOWER "${asset_name}" asset_name_lower)

        if (NOT asset_name_lower STREQUAL "optional")
            file(COPY "${asset_entry}" DESTINATION "${destination_dir}")
        endif ()
    endforeach ()
endfunction()

function(copy_optional_skyrim_assets source_dir destination_dir copied_var)
    set(optional_asset_dir "${source_dir}/optional")
    if (NOT EXISTS "${optional_asset_dir}")
        set("${copied_var}" OFF PARENT_SCOPE)
        return()
    endif ()

    file(MAKE_DIRECTORY "${destination_dir}")

    file(GLOB optional_entries LIST_DIRECTORIES true "${optional_asset_dir}/*")
    if (NOT optional_entries)
        set("${copied_var}" OFF PARENT_SCOPE)
        return()
    endif ()

    foreach (optional_entry IN LISTS optional_entries)
        file(COPY "${optional_entry}" DESTINATION "${destination_dir}")
    endforeach ()

    set("${copied_var}" ON PARENT_SCOPE)
endfunction()
