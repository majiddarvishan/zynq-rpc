cmake_minimum_required(VERSION 3.21)

project(zynq_rpc
        VERSION 0.0.1
        LANGUAGES CXX
        )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include CPM
include(cmake/CPM.cmake)

# Fetch ZeroMQ first (required by cppzmq)
# CPMAddPackage(
#     NAME libzmq
#     GITHUB_REPOSITORY zeromq/libzmq
#     GIT_TAG v4.3.5
#     OPTIONS
#         "BUILD_TESTS OFF"
#         "BUILD_SHARED OFF"
#         "BUILD_STATIC ON"
#         "WITH_PERF_TOOL OFF"
#         "ZMQ_BUILD_TESTS OFF"
#         "ENABLE_CPACK OFF"
# )

# get_target_property(_type libzmq TYPE)
# message(STATUS "libzmq target type: ${_type}")

# Fetch cppzmq (C++ bindings)
CPMAddPackage(
    NAME cppzmq
    GITHUB_REPOSITORY zeromq/cppzmq
    GIT_TAG v4.11.0
    OPTIONS
        "CPPZMQ_BUILD_TESTS OFF"
)

# Library target
add_library(zynq_rpc STATIC src/server.cpp src/client.cpp)

# Set target properties using modern CMake approach
target_link_libraries(zynq_rpc
    PRIVATE
        cppzmq
        # libzmq
)

target_include_directories(zynq_rpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Examples
add_executable(server_main examples/server_main.cpp)
target_link_libraries(server_main PRIVATE zynq_rpc)

add_executable(client_main examples/client_main.cpp)
target_link_libraries(client_main PRIVATE zynq_rpc)

add_executable(multi_client_main examples/multi_client_main.cpp)
target_link_libraries(multi_client_main PRIVATE zynq_rpc)

