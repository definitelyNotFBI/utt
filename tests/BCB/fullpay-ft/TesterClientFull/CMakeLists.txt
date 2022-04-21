add_executable(fullpay_ft_client
    main.cpp
    options.cpp
    protocol.cpp
    config.cpp
    conn.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(fullpay_ft_client PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../
    ../../fullpay-ft
    ../../../BCB
    ../../../../tests
)

target_link_libraries(fullpay_ft_client PUBLIC 
    ${fullpay_ft_libs}
)

