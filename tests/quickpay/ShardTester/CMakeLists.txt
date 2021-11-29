add_executable(shard_tester
    main.cpp
)

target_link_libraries(shard_tester PUBLIC 
    ${quickpay_libs}
)