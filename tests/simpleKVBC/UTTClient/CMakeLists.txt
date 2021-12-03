# simple client
set(utt_client_source_files
    client.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
    simple_test_client.hpp
    ecash_client.cpp
    adapter_config.hpp
    adapter_config.cpp
    state.cpp
    ../simpleKVBTestsBuilder.cpp
)

add_executable(utt_client ${utt_client_source_files} )

target_include_directories(utt_client
                           PUBLIC
                           ./
                           ${CMAKE_SOURCE_DIR}/util/include
                           ../..
                           ..
                           ../../config
                           ${concord_bft_tools_SOURCE_DIR}
                           ${bftengine_SOURCE_DIR}/src
                           ${bftengine_SOURCE_DIR}/include
                           ${CMAKE_SOURCE_DIR}/bftclient/include
                           ${CMAKE_SOURCE_DIR}/bftclient/src
                           ${UTT_INCLUDES}
                           ${bftclient_new_INCLUDE_DIRECTORIES}
                           ${concordbft_storage_INCLUDE_DIRECTORIES}
)

target_include_directories(utt_client PUBLIC ${libkvbc_SOURCE_DIR}/include)


target_link_libraries(utt_client 
    PUBLIC 
    corebft 
    kvbc
    threshsign
    util
    test_config_lib 
    bftclient_new
    concordbft_storage
    utt_bft
)

if(${USE_COMM_PLAIN_TCP})
	target_compile_definitions(utt_client PUBLIC USE_COMM_PLAIN_TCP)
endif()

if(${USE_COMM_TLS_TCP})
    target_compile_definitions(utt_client PUBLIC USE_COMM_TLS_TCP)
endif()

set_target_properties(utt_client PROPERTIES OUTPUT_NAME client)
