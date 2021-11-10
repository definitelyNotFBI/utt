cmake_minimum_required (VERSION 3.2)
project(quickpay VERSION 0.1.0.0 LANGUAGES CXX)

add_executable(quickpay_replica 
    TesterReplica/main.cpp
    TesterReplica/options.cpp
    TesterReplica/config.cpp
    TesterReplica/conn.cpp
    TesterReplica/protocol.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

add_executable(quickpay_client
    TesterClient/main.cpp
    TesterClient/options.cpp
    TesterClient/config.cpp
    TesterClient/protocol.cpp
    TesterClient/conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

find_package(Boost ${MIN_BOOST_VERSION} COMPONENTS system filesystem REQUIRED)

target_include_directories(quickpay_replica PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../
)

target_include_directories(quickpay_client PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../
)

set(quickpay_libs 
    ${Boost_LIBRARIES}
    util
    threshsign
    corebft
    test_config_lib
    utt_bft
)

target_link_libraries(quickpay_replica PUBLIC 
    ${quickpay_libs}
)

target_link_libraries(quickpay_client PUBLIC 
    ${quickpay_libs}
)

add_custom_target(copy_blockchain_scripts3 ALL COMMENT "Copying scripts for quickpay")
add_custom_command(TARGET copy_blockchain_scripts3
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/scripts ${CMAKE_CURRENT_BINARY_DIR}/scripts)