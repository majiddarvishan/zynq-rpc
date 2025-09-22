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

# Fetch cppzmq (C++ bindings)
CPMAddPackage(
    NAME cppzmq
    GITHUB_REPOSITORY zeromq/cppzmq
    GIT_TAG v4.11.0
)

# Fetch ZeroMQ (required library)
CPMAddPackage(
    NAME zeromq
    GITHUB_REPOSITORY zeromq/libzmq
    GIT_TAG v4.3.5
)

# Include directories automatically added by CPM
include_directories(${cppzmq_SOURCE_DIR})

# Library target
add_library(zynq_rpc STATIC src/server.cpp src/client.cpp)
target_link_libraries(zynq_rpc PRIVATE cppzmq::cppzmq zeromq::zeromq)

# Examples
add_executable(server_main examples/server_main.cpp)
target_link_libraries(server_main PRIVATE zmq_rpc)

add_executable(client_main examples/client_main.cpp)
target_link_libraries(client_main PRIVATE zmq_rpc)

add_executable(multi_client_main examples/multi_client_main.cpp)
target_link_libraries(multi_client_main PRIVATE zmq_rpc)
