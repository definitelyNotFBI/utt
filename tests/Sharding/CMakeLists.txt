set(sharding_libs 
    ${Boost_LIBRARIES}
    util
    threshsign
    corebft
    test_config_lib
    utt_bft
)

# Copy scripts
add_custom_target(copy_blockchain_scripts_shard ALL 
                        COMMENT "Copying scripts for sharding")
add_custom_command(TARGET copy_blockchain_scripts_shard
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts ${CMAKE_CURRENT_BINARY_DIR}/scripts)
add_custom_target(copy_tls_script_shard ALL COMMENT 
                                    "Copying TLS script (for sharding)")
add_custom_command(TARGET copy_tls_script_shard
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/linux/create_tls_certs.sh
     ${CMAKE_CURRENT_BINARY_DIR}/scripts/create_tls_certs.sh)

add_subdirectory(Replica)
add_subdirectory(Client)