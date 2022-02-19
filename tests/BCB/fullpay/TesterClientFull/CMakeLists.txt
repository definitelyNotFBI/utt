add_executable(fullpay_client
    main.cpp
    ../../quickpay/TesterClient/options.cpp
    protocol.cpp
    ../../quickpay/TesterClient/config.cpp
    conn.cpp
    # config.cpp
    # protocol.cpp
    # conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(fullpay_client PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../../fullpay
    ../../../BCB
    ../../../../tests
)

target_link_libraries(fullpay_client PUBLIC 
    ${fullpay_libs}
)

