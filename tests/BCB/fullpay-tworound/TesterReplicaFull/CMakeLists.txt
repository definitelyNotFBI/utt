add_executable(fullpay_replica2
    main.cpp
    options.cpp
    config.cpp
    conn.cpp
    protocol.cpp
    ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
)

target_include_directories(fullpay_replica2 PUBLIC 
    ${Boost_INCLUDE_DIRS}
    ../
    ../../
    ../../../
)

target_link_libraries(fullpay_replica2 PUBLIC 
    ${fullpay_tworound_libs}
)
