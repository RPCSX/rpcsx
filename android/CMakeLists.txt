cmake_minimum_required(VERSION 3.16.9)
project("rpcsx-android")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE on)

set(FFMPEG_VERSION 5.1)
set(LLVM_VERSION 20.1.2)

option(USE_ARCH "Specify arch to build" "")

if (USE_ARCH)
    add_compile_options(-march=${USE_ARCH})
endif()

if (TEST_OVERRIDE_CPU)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(ARCH_FLAGS "-mcpu=cortex-a53")
    else()
        set(ARCH_FLAGS "-mno-avx")
    endif()


    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ARCH_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ARCH_FLAGS}")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(RPCS3_DOWNLOAD_ARCH "arm64-v8a")
else()
    set(RPCS3_DOWNLOAD_ARCH "x86-64")
endif()

if(NOT EXISTS ${CMAKE_BINARY_DIR}/ffmpeg-${FFMPEG_VERSION}.tar.gz)
    message(STATUS "Downloading ffmpeg-${FFMPEG_VERSION}")
    file(DOWNLOAD
        https://github.com/RPCS3-Android/ffmpeg-android/releases/download/${FFMPEG_VERSION}/ffmpeg-${RPCS3_DOWNLOAD_ARCH}-Android.tar.gz
        ${CMAKE_BINARY_DIR}/ffmpeg-${FFMPEG_VERSION}.tar.gz
        SHOW_PROGRESS
    )
endif()

set(FFMPEG_PATH "${CMAKE_BINARY_DIR}/ffmpeg-${FFMPEG_VERSION}")

make_directory(${FFMPEG_PATH})
add_custom_command(
    OUTPUT ${FFMPEG_PATH}/src
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/ffmpeg-${FFMPEG_VERSION}.tar.gz
    DEPENDS ${CMAKE_BINARY_DIR}/ffmpeg-${FFMPEG_VERSION}.tar.gz
    WORKING_DIRECTORY ${FFMPEG_PATH}
    COMMENT "Unpacking ${CMAKE_BINARY_DIR}/ffmpeg-${FFMPEG_VERSION}.tar.gz"
)

add_custom_target(ffmpeg-unpack DEPENDS ${FFMPEG_PATH}/src)

function(import_ffmpeg_library name)
    add_custom_command(
        OUTPUT "${FFMPEG_PATH}/lib${name}/lib${name}.a"
        DEPENDS ffmpeg-unpack
        WORKING_DIRECTORY ${FFMPEG_PATH}
    )

    add_custom_target(ffmpeg-unpack-${name} DEPENDS "${FFMPEG_PATH}/lib${name}/lib${name}.a")
    add_library(ffmpeg::${name} STATIC IMPORTED GLOBAL)
    set_property(TARGET ffmpeg::${name} PROPERTY IMPORTED_LOCATION "${FFMPEG_PATH}/lib${name}/lib${name}.a")
    set_property(TARGET ffmpeg::${name} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_PATH}" "${FFMPEG_PATH}/src")
    add_dependencies(ffmpeg::${name} ffmpeg-unpack-${name})
endfunction()

import_ffmpeg_library(avcodec)
import_ffmpeg_library(avformat)
import_ffmpeg_library(avfilter)
import_ffmpeg_library(avdevice)
import_ffmpeg_library(avutil)
import_ffmpeg_library(swscale)
import_ffmpeg_library(swresample)

add_library(3rdparty_ffmpeg INTERFACE)
target_link_libraries(3rdparty_ffmpeg INTERFACE
    ffmpeg::avformat
    ffmpeg::avcodec
    ffmpeg::avutil
    ffmpeg::swscale
    ffmpeg::swresample
)

add_dependencies(3rdparty_ffmpeg ffmpeg-unpack)

set(WITH_RPCSX off)
set(WITH_RPCS3 on)
set(USE_SYSTEM_LIBUSB off)
set(USE_SYSTEM_CURL off)
set(USE_DISCORD_RPC off)
set(USE_SYSTEM_OPENCV off)
set(USE_SYSTEM_FFMPEG off)
set(USE_FAUDIO off)
set(USE_SDL2 off)
set(BUILD_LLVM on)
set(STATIC_LINK_LLVM on)
set(DISABLE_LTO off)
set(USE_LTO on)
set(USE_OPENSL off)
set(ASMJIT_NO_SHM_OPEN on)
set(USE_SYSTEM_ZLIB on)
set(USE_LIBEVDEV off)
set(COMPILE_FFMPEG off)

add_subdirectory(${CMAKE_SOURCE_DIR}/.. rpcsx EXCLUDE_FROM_ALL)

add_library(${CMAKE_PROJECT_NAME} SHARED
    src/rpcsx-android.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC rpcs3/rpcs3)

target_link_libraries(${CMAKE_PROJECT_NAME}
    rpcs3
    rpcsx::fw::ps3
    rx
    android
    log
)
