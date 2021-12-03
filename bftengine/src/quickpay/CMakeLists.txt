find_package(cryptopp REQUIRED)

set(quickpay_source_files
    QuickPay.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/messages/ClientRequestMsg.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/MsgsCommunicator.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/SigManager.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/Crypto.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/messages/ClientRequestMsg.cpp
    ${bftengine_SOURCE_DIR}/src/bftengine/messages/MessageBase.cpp
    messages/ClientQuickPayRequestMsg.cpp
)

add_library(quickpay STATIC ${quickpay_source_files})

target_include_directories(quickpay PUBLIC ${CRYPTOPP_INCLUDE_DIRS})
target_include_directories(quickpay PUBLIC ${OPENSSL_INCLUDE_DIR})
target_include_directories(quickpay PUBLIC ${bftengine_SOURCE_DIR}/src/bftengine)
target_include_directories(quickpay PUBLIC ${bftengine_SOURCE_DIR}/include)
target_include_directories(quickpay PUBLIC ${bftengine_SOURCE_DIR}/include/bftengine)

get_property(perf_include GLOBAL PROPERTY PERF_MANAGER_INCLUDE_DIR)
get_property(util_include GLOBAL PROPERTY UTIL_INCLUDE_DIR)
get_property(kvbc_include GLOBAL PROPERTY KVBC_INCLUDE_DIR)
target_include_directories(quickpay PUBLIC ${perf_include} ${util_include} ${kvbc_include})

target_link_libraries(quickpay PUBLIC diagnostics)

target_link_libraries(quickpay PUBLIC util)
target_link_libraries(quickpay PUBLIC threshsign)
target_link_libraries(quickpay PUBLIC ${OPENSSL_LIBRARIES})
target_link_libraries(quickpay PUBLIC bftcommunication)
