add_executable(sharding_client
    main.cpp
    protocol.cpp
    options.cpp
    config.cpp
    conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(sharding_client PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ./
    ../
    ../../
)

target_link_libraries(sharding_client PUBLIC 
    ${sharding_libs}
)
