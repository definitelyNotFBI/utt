set(utt_bft_test_sources
    TestSerialization.cpp
    TestMintFlow.cpp
    TestPayFlow.cpp
)

foreach(appSrc ${utt_bft_test_sources})
    get_filename_component(appName ${appSrc} NAME_WE)

    add_executable(${appName} ${appSrc})
    target_link_libraries(${appName} PRIVATE ${UTT_LIBS} utt_bft)

    add_test(NAME ${appName} COMMAND ${appName})
endforeach()