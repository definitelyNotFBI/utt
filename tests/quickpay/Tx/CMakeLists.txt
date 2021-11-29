add_executable(tx_generator
    main.cpp
    setup.cpp
)

target_link_libraries(tx_generator PUBLIC 
    ${quickpay_libs}
)