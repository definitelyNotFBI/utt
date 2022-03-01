# add_subdirectory(cmf)

include_directories(${UTT_INCLUDES})
add_subdirectory(UTTClient)
add_subdirectory(TesterReplica)
add_subdirectory(TesterCRE)
add_custom_target(copy_blockchain_scripts3 ALL COMMENT "Copying scripts abcd")
add_custom_command(TARGET copy_blockchain_scripts3
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts ${CMAKE_CURRENT_BINARY_DIR}/scripts)