cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS off)

add_subdirectory(bridge)
add_subdirectory(device)
add_subdirectory(shader)
add_subdirectory(lib/libspirv)

project(amdgpu)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

add_library(amdgpu::base ALIAS ${PROJECT_NAME})

