# simple client
set(simpleEcash_client_source_files
    client.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
    simple_test_client.hpp
    ecash_client.cpp
    adapter_config.hpp
    adapter_config.cpp
    state.cpp
)
find_package(GTest REQUIRED)

add_executable(simpleEcash_client ${simpleEcash_client_source_files} )

target_include_directories(simpleEcash_client
                           PRIVATE
                           ./
                           ${CMAKE_SOURCE_DIR}/util/include
                           ../config
                           ${concord_bft_tools_SOURCE_DIR}
                           ${bftengine_SOURCE_DIR}/src
                           ${bftengine_SOURCE_DIR}/include
                           ${CMAKE_SOURCE_DIR}/bftclient/include
                           ${CMAKE_SOURCE_DIR}/bftclient/src
                           ${UTT_INCLUDES}
                           ${bftclient_new_INCLUDE_DIRECTORIES}
                           ${concordbft_storage_INCLUDE_DIRECTORIES}
)

target_link_libraries(simpleEcash_client 
    PUBLIC 
    corebft 
    test_config_lib 
    bftclient_new
    concordbft_storage
    ${UTT_LIBS}
)
set_target_properties(simpleEcash_client PROPERTIES OUTPUT_NAME client)

# simple server
set(simpleEcash_replica_source_files
    replica.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
    simple_test_replica.hpp
    simple_test_replica_behavior.hpp
    state.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/Crypto.hpp
)

add_executable(simpleEcash_server 
               
               ${simpleEcash_replica_source_files}
)

target_include_directories(simpleEcash_server
                           PRIVATE
                           ../config
                           ../simpleEcash
                           ${bftengine_SOURCE_DIR}/tests/simpleStorage
                           ${concord_bft_tools_SOURCE_DIR}
                           ${bftengine_SOURCE_DIR}/src
                           ${CMAKE_SOURCE_DIR}/bftclient/src
                           ${CMAKE_SOURCE_DIR}/bftclient/include
                           ${bftengine_SOURCE_DIR}/include
                           ${bftclient_new_INCLUDE_DIRECTORIES}
                           ${UTT_INCLUDES}
                           ${concordbft_storage_INCLUDE_DIRECTORIES}
)

target_link_libraries(simpleEcash_server 
    PUBLIC 
    corebft 
    simple_file_storage_lib 
    test_config_lib
    concordbft_storage
    ${UTT_LIBS}
)
set_target_properties(simpleEcash_server PROPERTIES OUTPUT_NAME server)

if(${USE_COMM_PLAIN_TCP})
    target_compile_definitions(simpleEcash_client PUBLIC USE_COMM_PLAIN_TCP)
    target_compile_definitions(simpleEcash_server PUBLIC USE_COMM_PLAIN_TCP)
endif()

if(${USE_COMM_TLS_TCP})
    target_compile_definitions(simpleEcash_client PUBLIC USE_COMM_TLS_TCP)
    target_compile_definitions(simpleEcash_server PUBLIC USE_COMM_TLS_TCP)
endif()

add_custom_target(generate_test_config2
        		  COMMAND GenerateConcordKeys -f 1 -n 4 -o private_replica_ 
        		  COMMENT "Generating config files for simpleEcash" 
        		)
add_custom_target(generate_utt_config
                  COMMAND GenerateUTTKeys -t 2 -n 4
                  COMMENT "Generating UTT config files for simpleEcash" 
)

add_dependencies(simpleEcash_server generate_test_config2 generate_utt_config)

add_custom_target(copy_simple_test_scripts2 ALL COMMENT "Copying scripts of simpleEcash")
add_custom_command(TARGET copy_simple_test_scripts2
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:simpleEcash_client>/scripts
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:simpleEcash_client>/scripts)

# Fill in CMAKE variables in the test script
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/testReplicasAndClient.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/testReplicasAndClient.sh"
)

# Fill in CMAKE variables in the python wrapper
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/simpleEcash.py.in"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/simpleEcash.py"
)

# Fill in CMAKE variables in the python histogram for perf test
configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/histogram.py.in"
        "${CMAKE_CURRENT_BINARY_DIR}/scripts/histogram.py"
)

add_custom_target(copy_tls_script2 ALL COMMENT "Copying TLS script")
add_custom_command(TARGET copy_tls_script2
        COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/scripts/linux/create_tls_certs.sh
     $<TARGET_FILE_DIR:simpleEcash_client>/scripts/create_tls_certs.sh)
