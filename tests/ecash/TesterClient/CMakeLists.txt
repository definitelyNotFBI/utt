cmake_minimum_required (VERSION 3.2)
project(ecash_client VERSION 0.1.0.0 LANGUAGES CXX)

set(client_sources
    src/main.cpp
    src/config.cpp 
    src/client.cpp
    src/receiver.cpp
    src/Msg.cpp
)

add_executable(ecash_client ${client_sources})

target_compile_definitions(ecash_client PUBLIC USE_COMM_PLAIN_TCP)

target_link_libraries(ecash_client PUBLIC kvbc corebft threshsign util tcpcomm)

target_include_directories(ecash_client PUBLIC ..)
target_include_directories(ecash_client PUBLIC ../..)
target_include_directories(ecash_client PUBLIC ${libkvbc_SOURCE_DIR}/include)
target_include_directories(ecash_client PUBLIC include)
