add_executable(quickpay_replica 
    main.cpp
    options.cpp
    config.cpp
    conn.cpp
    protocol.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(quickpay_replica PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../../quickpay
    ../../../BCB
    ../../../../tests
)

target_link_libraries(quickpay_replica PUBLIC 
    ${quickpay_libs}
)
