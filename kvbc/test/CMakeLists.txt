find_package(GTest REQUIRED)
find_package(rapidcheck)

if(Boost_FOUND)
    add_executable(blockchain_view_test blockchain_view_test.cpp )
    add_test(blockchain_view_test blockchain_view_test)
    target_include_directories(blockchain_view_test PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(blockchain_view_test PUBLIC
        GTest::Main
        util
        kvbc
    )
endif()

# add_test(NAME kvbc_filter_test COMMAND kvbc_filter_test)
# add_executable(kvbc_filter_test
#        kvbc_app_filter/kvbc_filter_test.cpp)
#
# target_link_libraries(kvbc_filter_test
#         ${Boost_LIBRARIES}
#         GTest::Main
#         GTest::GTest
#         ${yaml-cpp_LIBRARIES}
#         kvbc
#         concord-kvbc-proto
#         concordbft_reconfiguration
#         logging)

add_executable(order_test order_test.cpp )
add_test(order_test order_test)
target_link_libraries(order_test GTest::Main kvbc util)
target_compile_options(order_test PUBLIC -Wno-sign-compare)

add_executable(kvbc_dbadapter_test kvbc_dbadapter_test.cpp )
add_test(kvbc_dbadapter_test kvbc_dbadapter_test)
target_link_libraries(kvbc_dbadapter_test GTest::Main kvbc util)


add_executable(sparse_merkle_storage_db_adapter_unit_test
    sparse_merkle_storage/db_adapter_unit_test.cpp )
add_test(sparse_merkle_storage_db_adapter_unit_test sparse_merkle_storage_db_adapter_unit_test)
target_link_libraries(sparse_merkle_storage_db_adapter_unit_test PUBLIC
    GTest::Main
    GTest::GTest
    util
    corebft
    kvbc
    stdc++fs
)

add_executable(sparse_merkle_storage_serialization_unit_test
    sparse_merkle_storage/serialization_unit_test.cpp )
add_test(sparse_merkle_storage_serialization_unit_test sparse_merkle_storage_serialization_unit_test)
target_link_libraries(sparse_merkle_storage_serialization_unit_test PUBLIC
    GTest::Main
    GTest::GTest
    util
    kvbc
)

if(rapidcheck_FOUND)
add_executable(sparse_merkle_storage_db_adapter_property_test
    sparse_merkle_storage/db_adapter_property_test.cpp )
add_test(sparse_merkle_storage_db_adapter_property_test sparse_merkle_storage_db_adapter_property_test)
# Decrease both the input size and test count from the default value of 100 to 50 in order to speed up this test.
set_property(TEST sparse_merkle_storage_db_adapter_property_test PROPERTY ENVIRONMENT RC_PARAMS=max_size=50\ max_success=50)
target_link_libraries(sparse_merkle_storage_db_adapter_property_test PUBLIC
    GTest::Main
    GTest::GTest
    rapidcheck
    util
    corebft
    kvbc
    stdc++fs
)

add_executable(sparse_merkle_internal_node_property_test sparse_merkle/internal_node_property_tests.cpp
)
add_test(sparse_merkle_internal_node_property_test sparse_merkle_internal_node_property_test)
target_link_libraries(sparse_merkle_internal_node_property_test PUBLIC
    GTest::Main
    GTest::GTest
    rapidcheck
    util
    kvbc
    OpenSSL::Crypto
)
endif ()

add_executable(sparse_merkle_base_types_test sparse_merkle/base_types_test.cpp
    )
add_test(sparse_merkle_base_types_test sparse_merkle_base_types_test)
target_link_libraries(sparse_merkle_base_types_test PUBLIC
    GTest::Main
    GTest::GTest
    corebft
    kvbc
)

find_package(OpenSSL REQUIRED)
add_executable(sparse_merkle_internal_node_test sparse_merkle/internal_node_test.cpp
    )
add_test(sparse_merkle_internal_node_test sparse_merkle_internal_node_test)
target_link_libraries(sparse_merkle_internal_node_test PUBLIC
    GTest::Main
    GTest::GTest
    corebft
    kvbc
    ${OPENSSL_LIBRARIES}
)

find_package(OpenSSL REQUIRED)
add_executable(sparse_merkle_tree_test sparse_merkle/tree_test.cpp
    )
add_test(sparse_merkle_tree_test sparse_merkle_tree_test)
target_link_libraries(sparse_merkle_tree_test PUBLIC
    GTest::Main
    GTest::GTest
    kvbc
    corebft
    ${OPENSSL_LIBRARIES}
)

if (BUILD_ROCKSDB_STORAGE)
add_executable(multiIO_test multiIO_test.cpp )
add_test(multiIO_test multiIO_test)

target_link_libraries(multiIO_test PUBLIC
    GTest::Main
    GTest::GTest
    util
    kvbc
    corebft
)

add_executable(kv_blockchain_db_editor_test sparse_merkle_storage/kv_blockchain_db_editor_test.cpp )
add_test(kv_blockchain_db_editor_test kv_blockchain_db_editor_test)

target_include_directories(kv_blockchain_db_editor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include/kvbc_app_filter/ 
																${CMAKE_CURRENT_SOURCE_DIR}/../../bftengine/src)
target_link_libraries(kv_blockchain_db_editor_test PUBLIC
    GTest::Main
    GTest::GTest
    lib_db_editor
    util
    kvbc
    stdc++fs
    execution_data_cmf
)
endif(BUILD_ROCKSDB_STORAGE)

if(USE_S3_OBJECT_STORE)
add_executable(s3_storage_factory_test s3_storage_factory_test.cpp )
add_test(s3_storage_factory_test s3_storage_factory_test)
target_include_directories(s3_storage_factory_test PUBLIC
    kvbc
)
target_link_libraries(s3_storage_factory_test PUBLIC
    GTest::Main
    kvbc
)
endif(USE_S3_OBJECT_STORE)

if (BUILD_ROCKSDB_STORAGE)
    add_executable(categorized_kv_blockchain_unit_test
        categorization/kv_blockchain_test.cpp )
    add_test(categorized_kv_blockchain_unit_test categorized_kv_blockchain_unit_test)
    target_link_libraries(categorized_kv_blockchain_unit_test PUBLIC
        GTest::Main
        GTest::GTest
        util
        corebft
        kvbc
        stdc++fs
    )

    add_executable(categorized_blocks_unit_test
        categorization/blocks_test.cpp )
    add_test(categorized_blocks_unit_test categorized_blocks_unit_test)
    target_link_libraries(categorized_blocks_unit_test PUBLIC
        GTest::Main
        GTest::GTest
        util
        corebft
        kvbc
        stdc++fs
    )

    add_executable(categorized_blockchain_unit_test
        categorization/blockchain_test.cpp )
    add_test(categorized_blockchain_unit_test categorized_blockchain_unit_test)
    target_link_libraries(categorized_blockchain_unit_test PUBLIC
        GTest::Main
        GTest::GTest
        util
        corebft
        kvbc
        stdc++fs
    )
    add_executable(immutable_kv_category_unit_test categorization/immutable_kv_category_unit_test.cpp )
    add_test(immutable_kv_category_unit_test immutable_kv_category_unit_test)
    target_link_libraries(immutable_kv_category_unit_test PUBLIC
        GTest::Main
        GTest::GTest
        util
        corebft
        kvbc
        stdc++fs
    )

    add_executable(block_merkle_category_unit_test categorization/block_merkle_category_unit_test.cpp )
    add_test(block_merkle_category_unit_test block_merkle_category_unit_test)
    target_link_libraries(block_merkle_category_unit_test PUBLIC
        GTest::Main
        GTest::GTest
        util
        kvbc
        stdc++fs
    )

    add_executable(versioned_kv_category_unit_test categorization/versioned_kv_category_unit_test.cpp )
    add_test(versioned_kv_category_unit_test versioned_kv_category_unit_test)
    target_link_libraries(versioned_kv_category_unit_test PUBLIC
        GTest::Main
        GTest::GTest
        util
        corebft
        kvbc
        stdc++fs
    )
    add_executable(pruning_test pruning_test.cpp)
    add_test(pruning_test pruning_test)
    target_link_libraries(pruning_test
            GTest::GTest
            corebft
            kvbc
            stdc++fs)

    add_executable(pruning_reserved_pages_client_test pruning_reserved_pages_client_test.cpp)
    add_test(pruning_reserved_pages_client_test pruning_reserved_pages_client_test)
    target_link_libraries(pruning_reserved_pages_client_test PUBLIC
            GTest::Main
            GTest::GTest
            util
            corebft
            kvbc
    )

    add_executable(replica_state_sync_test replica_state_sync_test.cpp)
    add_test(replica_state_sync_test replica_state_sync_test)
    target_link_libraries(replica_state_sync_test PUBLIC
            GTest::Main
            GTest::GTest
            util
            corebft
            kvbc
            stdc++fs
    )
endif (BUILD_ROCKSDB_STORAGE)
