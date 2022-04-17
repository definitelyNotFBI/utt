add_executable(fullpay_client2
    main.cpp
    options.cpp
    protocol.cpp
    config.cpp
    conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(fullpay_client2 PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../../fullpay-tworound
    ../../../BCB
    ../../../../tests
)

target_link_libraries(fullpay_client2 PUBLIC 
    ${fullpay_tworound_libs}
)

