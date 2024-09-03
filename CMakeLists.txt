cmake_minimum_required(VERSION 3.20)
project(rpcsx)

set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(3rdparty EXCLUDE_FROM_ALL)
add_subdirectory(orbis-kernel)
add_subdirectory(rpcsx-os)
add_subdirectory(rpcsx-gpu)
add_subdirectory(hw/amdgpu)
add_subdirectory(rx)

target_compile_definitions(rx PRIVATE
    RX_TAG=0
    RX_TAG_VERSION=0
)
