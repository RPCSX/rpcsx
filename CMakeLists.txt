cmake_minimum_required(VERSION 3.20)
project(rpcsx)

set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD 23)

if( ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_compile_definitions(LINUX)
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_compile_definitions(MAC)
elseif( ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_compile_definitions(WIN)
endif()

add_subdirectory(3rdparty/crypto)
add_subdirectory(orbis-kernel)
add_subdirectory(rpcsx-os)
add_subdirectory(rpcsx-gpu)
add_subdirectory(hw/amdgpu)