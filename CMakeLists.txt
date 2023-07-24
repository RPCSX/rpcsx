cmake_minimum_required(VERSION 3.20)
project(rpcsx)

set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(xbyak 6.70 CONFIG)

add_subdirectory(3rdparty)
add_subdirectory(orbis-kernel)
add_subdirectory(rpcsx-os)
add_subdirectory(rpcsx-gpu)
add_subdirectory(hw/amdgpu)
