if (ZIP_TO_DIST)
  set(CPACK_GENERATOR "7Z")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/dist")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PACKAGE_CONFIG_SUFFIX "Debug")
  else ()
    set(PACKAGE_CONFIG_SUFFIX "Release")
  endif ()

  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PACKAGE_CONFIG_SUFFIX}-${PROJECT_VERSION}")

  install(FILES "$<TARGET_FILE:${PROJECT_NAME}>" DESTINATION "SKSE/Plugins")
  install(FILES "$<TARGET_PDB_FILE:${PROJECT_NAME}>" DESTINATION "SKSE/Plugins")
  include(CPack)

  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E echo "Packaging for distribution..."
          COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
          COMMAND cpack --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake" -C $<CONFIG>
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif ()

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
