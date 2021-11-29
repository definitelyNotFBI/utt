cmake_minimum_required (VERSION 3.2)
project(quickpay VERSION 0.1.0.0 LANGUAGES CXX)

include_directories(${UTT_INCLUDES})

find_package(Boost ${MIN_BOOST_VERSION} COMPONENTS system filesystem REQUIRED)

set(quickpay_libs 
    ${Boost_LIBRARIES}
    util
    threshsign
    corebft
    test_config_lib
    utt_bft
)

add_custom_target(copy_blockchain_scripts_qp ALL 
                        COMMENT "Copying scripts for quickpay")
add_custom_command(TARGET copy_blockchain_scripts_qp
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts ${CMAKE_CURRENT_BINARY_DIR}/scripts)


add_custom_target(copy_tls_script_qp ALL COMMENT 
                                    "Copying TLS script (for quickpay)")
add_custom_command(TARGET copy_tls_script_qp
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/linux/create_tls_certs.sh
     ${CMAKE_CURRENT_BINARY_DIR}/scripts/create_tls_certs.sh)

add_subdirectory(Tx)
add_subdirectory(TesterReplica)
add_subdirectory(ShardTester)
add_subdirectory(TesterClient)