cmake_minimum_required(VERSION 3.20)
project(rpcsx)

set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD 23)

add_subdirectory(3rdparty/crypto)
add_subdirectory(orbis-kernel)
add_subdirectory(rpcsx-os)
