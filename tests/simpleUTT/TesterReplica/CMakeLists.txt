cmake_minimum_required (VERSION 3.2)
project(simpleutt_replica VERSION 0.1.0.0 LANGUAGES CXX)

add_executable(simpleutt_replica main.cpp
		 internalCommandsHandler.cpp
		 setup.cpp
		 ../simpleKVBTestsBuilder.cpp
		 ${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
		 WrapCommunication.cpp
		 strategy/ShufflePrePrepareMsgStrategy.cpp
		 strategy/StrategyUtils.cpp
		 strategy/MangledPreProcessResultMsgStrategy.cpp)

if(${USE_COMM_PLAIN_TCP})
	target_compile_definitions(simpleutt_replica PUBLIC USE_COMM_PLAIN_TCP)
endif()

if(${USE_COMM_TLS_TCP})
	target_compile_definitions(simpleutt_replica PUBLIC USE_COMM_TLS_TCP)
endif()

if(BUILD_ROCKSDB_STORAGE)
	target_compile_definitions(simpleutt_replica PUBLIC "USE_ROCKSDB=1")
endif()

target_link_libraries(simpleutt_replica PUBLIC kvbc corebft threshsign util test_config_lib stdc++fs utt_bft)

target_include_directories(simpleutt_replica PUBLIC ..)
target_include_directories(simpleutt_replica PUBLIC ../..)
target_include_directories(simpleutt_replica PUBLIC ${libkvbc_SOURCE_DIR}/include)
target_include_directories(simpleutt_replica PRIVATE ${bftengine_SOURCE_DIR}/src/preprocessor)



