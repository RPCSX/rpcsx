cmake_minimum_required(VERSION 3.25)

project(orbis-kernel-examples)

if("${ORBIS_KERNEL_DIR}" STREQUAL "")
  set(ORBIS_KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
  cmake_path(ABSOLUTE_PATH ORBIS_KERNEL_DIR NORMALIZE)
endif()

set(CMAKE_CXX_STANDARD 23)
add_subdirectory(standalone)
add_subdirectory(${ORBIS_KERNEL_DIR} orbis-kernel EXCLUDE_FROM_ALL )

