cmake_minimum_required(VERSION 3.2)
project(concord-bft VERSION 0.1.0.0 LANGUAGES CXX)

#
# C++ options
#   TODO: change to set_target_properties?
#   https://crascit.com/2015/03/28/enabling-cxx11-in-cmake/
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(SLEEP_FOR_DBG FALSE)

set(MIN_BOOST_VERSION 1.64)

# Default to debug builds
# Release builds can be enabled by running cmake with -DCMAKE_BUILD_TYPE=Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Enable debug or release builds" FORCE)
endif()

# Default USE_LOG4CPP to FALSE
if (NOT DEFINED USE_LOG4CPP)
  option(USE_LOG4CPP "Enable LOG4CPP" FALSE)
endif()

# Default KEEP_APOLLO_LOGS to TRUE
option(KEEP_APOLLO_LOGS "Retains logs from replicas in separate folder for each test in build/tests/apollo/logs " TRUE)

# Default TXN_SIGNING_ENABLED to FALSE
option(TXN_SIGNING_ENABLED "Enable External concord client transcattion signing" TRUE)

# Default BUILD_COMM_TCP_PLAIN to FALSE
option(BUILD_COMM_TCP_PLAIN "Enable TCP communication" FALSE)

# Default LEAKCHECK to FALSE
option(LEAKCHECK "Enable Address and Leak Sanitizers" FALSE)

# Default THREADCHECK to FALSE
option(THREADCHECK "Enable Thread Sanitizer" FALSE)

# Default BUILD_COMM_TCP_TLS to FALSE
option(BUILD_COMM_TCP_TLS "Enable TCP TLS communication" FALSE)

# Rocksdb is required for storage now. Consider removing this flag.
option(BUILD_ROCKSDB_STORAGE "Enable building of RocksDB storage library" TRUE)

# Build S3 Object Store support.
option(USE_S3_OBJECT_STORE "Enable S3 Object Store" FALSE)

# Build Slowdown framework
option(BUILD_SLOWDOWN "Build Slowdown framework" FALSE)

# Build Time service with fake clock
option(USE_FAKE_CLOCK_IN_TIME_SERVICE "BUILD TimeService Using Fake Clock" FALSE)

set(COMM_MODULES 0)
if(BUILD_COMM_TCP_PLAIN)
    math(EXPR COMM_MODULES "${COMM_MODULES}+1")
endif()

if(BUILD_COMM_TCP_TLS)
    math(EXPR COMM_MODULES "${COMM_MODULES}+1")
endif()

# UDP module is not part of the CMake configuration, it will be used by default
# if neither of plain TCP or TLS will be chosen
if(${COMM_MODULES} GREATER 1)
    message(FATAL_ERROR "Only one comm module can be chosen")
endif()

if(SLEEP_FOR_DBG)
    add_definitions(-DSLEEP_DBG)
endif()

option(USE_OPENTRACING "Enable OpenTracing" ON)
option(USE_GRPC "Enable GRPC and Protobuf" OFF)
#
# Compiler options
#
#

string(APPEND CMAKE_CXX_FLAGS " -Wall")
string(APPEND CMAKE_CXX_FLAGS " -Wbuiltin-macro-redefined")
string(APPEND CMAKE_CXX_FLAGS " -pedantic")
string(APPEND CMAKE_CXX_FLAGS " -Werror")
string(APPEND CMAKE_CXX_FLAGS " -fno-omit-frame-pointer")
if(LEAKCHECK AND THREADCHECK)
    message(FATAL_ERROR "Cannot have both Thread and Address/Leak Sanitizers Enabled")
endif()
if(LEAKCHECK)
    string(APPEND CMAKE_CXX_FLAGS " -fsanitize=leak -fsanitize=address")
    message("-- Address and Leak Sanitizers Enabled")
endif()
if(THREADCHECK)
    string(APPEND CMAKE_CXX_FLAGS " -fsanitize=thread")
    message("-- Thread Sanitizer Enabled")
endif()

# TODO: Figure out right way to deal with -fstrict-overflow / -Wstrict-overflow related errors
# string(APPEND CXX_FLAGS " -fno-strict-overflow")
# Prevents some buffer overflows: https://access.redhat.com/blogs/766093/posts/1976213
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -D_FORTIFY_SOURCE=2")

string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fstack-protector-all")


if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")

    string(APPEND CMAKE_CXX_FLAGS " -ferror-limit=3")

    # Our RELIC library used in threshsign is in the habit of picking generic
    # macro names like HASH and ALIGNED, which conflicts with our own code or
    # other libraries. Even worse, compilers don't show 'macro redefined' warnings
    # for system header files such as our installed RELIC library. So we do this:
    # TODO: [TK] move to the threshsign module
    string(APPEND CMAKE_CXX_FLAGS " --no-system-header-prefix relic")

    string(APPEND CMAKE_CXX_FLAGS " -Wmacro-redefined")
    string(APPEND CMAKE_CXX_FLAGS " -Wsign-compare")

    # Export a compile database for use by semantic analysis tools
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    string(APPEND CMAKE_CXX_FLAGS " -fmax-errors=3")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(CTest)
#
# Subdirectories
#
if(USE_GRPC)
	add_subdirectory(thin-replica-server)
endif()
# add_subdirectory(libutt/libutt)

# Deps will be installed during image setup
# Just build libutt here
# TODO Currently building outside, and running dmake afterwards
link_directories(libutt/build/libutt/src)

set(
  UTT_INCLUDES
  ${CMAKE_SOURCE_DIR}/libutt/libutt/include
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(
  UTT_LIBS
  "utt"
  "stdc++fs"
  "gomp"
  "ntl"
  "ff"
  "pthread"
  "gmp"
  "gmpxx"
  "zm"
  "xutils"
  "xassert"
  "Threads::Threads"
)

set(
  CURVE
  "BN128"
  CACHE
  STRING
  "Default curve: one of ALT_BN128, BN128, EDWARDS, MNT4, MNT6"
)

option(
  BINARY_OUTPUT
  "In serialization of elliptic curve points, output raw binary data (instead of decimal), which is smaller and faster."
  OFF
)

add_definitions(
  -DCURVE_${CURVE}
)

option(
  USE_MULTITHREADING
  "Enable parallelized execution of DKG protocols using OpenMP"
  OFF
)

if(${CURVE} STREQUAL "BN128")
  add_definitions(
    -DBN_SUPPORT_SNARK=1
  )
endif()

# Adding libutt specifics before adding subdirectories so that all the subdirectories can take advantage of libutt
#
# Subdirectories
#
add_subdirectory(logging)
add_subdirectory(util)
add_subdirectory(secretsmanager)
add_subdirectory(threshsign)
add_subdirectory(communication)
add_subdirectory(kvbc)
add_subdirectory(performance)
add_subdirectory(bftengine)
add_subdirectory(storage)
add_subdirectory(scripts)
add_subdirectory(diagnostics)
add_subdirectory(bftclient)
add_subdirectory(reconfiguration)
add_subdirectory(client)
add_subdirectory(ccron)
add_subdirectory(utt_bft)
add_subdirectory(tools)


#
# Setup testing
#

option(OMIT_TEST_OUTPUT "Forwards output stdout and stdin to /dev/null" OFF)

if(BUILD_TESTING)
    add_subdirectory(bftengine/tests)
    add_subdirectory(tests)
    add_subdirectory(messages)
endif()
