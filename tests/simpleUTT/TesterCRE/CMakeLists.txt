cmake_minimum_required (VERSION 3.2)
project(simpleutt_cre VERSION 0.1.0.0 LANGUAGES CXX)

set(cre_sources
    main.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

add_executable(simpleutt_cre ${cre_sources})

if(${USE_COMM_PLAIN_TCP})
	target_compile_definitions(simpleutt_cre PUBLIC USE_COMM_PLAIN_TCP)
endif()

if(${USE_COMM_TLS_TCP})
    target_compile_definitions(simpleutt_cre PUBLIC USE_COMM_TLS_TCP)
endif()

target_link_libraries(simpleutt_cre PUBLIC cre kvbc corebft threshsign util cmf_messages bftclient_new test_config_lib)

target_include_directories(simpleutt_cre PUBLIC ..)
target_include_directories(simpleutt_cre PUBLIC ../..)

