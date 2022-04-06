add_executable(sharding_replica
    main.cpp
    options.cpp
    config.cpp
    conn.cpp
    protocol.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(sharding_replica PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../
    ../../
    ../../../
)

target_link_libraries(sharding_replica PUBLIC 
    ${sharding_libs}
)

