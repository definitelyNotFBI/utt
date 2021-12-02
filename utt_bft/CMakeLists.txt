cmake_minimum_required (VERSION 3.2)
project(libutt_bft VERSION 0.1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(utt_bft
    
    # Clients
    src/client/Params.cpp
    src/client/IClient.cpp
    
    # Replicas
    src/replica/Params.cpp
    
    # General
    src/ThresholdParamGen.cpp
    src/QuickPayMsg.cpp
)

target_link_libraries(utt_bft PUBLIC 
    corebft 
    util 
    bftcommunication 
    ff
    ${UTT_LIBS}
)
target_include_directories(utt_bft PUBLIC 
    include 
    util 
    ${UTT_INCLUDES}
)

# Allow others in the system to use this library
set_property(GLOBAL PROPERTY 
    UTT_BFT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(tests)