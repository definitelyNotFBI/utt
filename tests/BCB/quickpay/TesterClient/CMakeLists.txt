add_executable(quickpay_client
    main.cpp
    options.cpp
    config.cpp
    protocol.cpp
    conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(quickpay_client PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../../quickpay
    ../../../BCB
    ../../../../tests
)

target_link_libraries(quickpay_client PUBLIC 
    ${quickpay_libs}
)

