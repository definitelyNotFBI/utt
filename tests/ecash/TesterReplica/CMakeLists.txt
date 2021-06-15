cmake_minimum_required (VERSION 3.2)
project(ecash_replica VERSION 0.1.0.0 LANGUAGES CXX)

add_executable(ecash_replica 
	src/main.cpp
	src/ReplicaImpl.cpp
	${concord_bft_tools_SOURCE_DIR}/KeyfileIOUtils.cpp
	)

if(BUILD_ROCKSDB_STORAGE)
	target_compile_definitions(ecash_replica PUBLIC "USE_ROCKSDB=1")
endif()

target_link_libraries(ecash_replica PUBLIC tcpcomm corebft threshsign util test_config_lib)

target_include_directories(ecash_replica PUBLIC include)
target_include_directories(ecash_replica 
	PUBLIC 
	../../config
	${bftengine_SOURCE_DIR}/tests/simpleStorage
)	

target_link_libraries(ecash_replica PRIVATE simple_file_storage_lib)