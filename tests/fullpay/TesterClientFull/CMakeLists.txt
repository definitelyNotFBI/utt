add_executable(quickpay_client_full
    main.cpp
    options.cpp
    config.cpp
    protocol.cpp
    conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(quickpay_client_full PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../
    ../../
)

target_link_libraries(quickpay_client_full PUBLIC 
    ${quickpay_libs}
)

