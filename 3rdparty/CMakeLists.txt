add_subdirectory(crypto)

if(NOT xbyak_FOUND)
    add_subdirectory(xbyak)
endif()

add_subdirectory(SPIRV-Headers)

add_subdirectory(SPIRV-Tools)

option(SPIRV_CROSS_SHARED "" off)
option(SPIRV_CROSS_STATIC "" on)
option(SPIRV_CROSS_ENABLE_GLSL "" on)
option(SPIRV_CROSS_ENABLE_HLSL "" off)
option(SPIRV_CROSS_ENABLE_MSL "" off)
option(SPIRV_CROSS_ENABLE_CPP "" off)
option(SPIRV_CROSS_ENABLE_REFLECT "" off)
option(SPIRV_CROSS_ENABLE_C_API "" off)
option(SPIRV_CROSS_ENABLE_UTIL "" off)
option(SPIRV_CROSS_CLI "" off)
option(SPIRV_CROSS_ENABLE_TESTS "" off)
option(SPIRV_CROSS_SKIP_INSTALL "" on)
add_subdirectory(SPIRV-Cross)

add_subdirectory(glslang)
if(NOT TARGET glslang::glslang-standalone)
    add_executable(glslang::glslang-standalone ALIAS glslang-standalone)
endif()

if(NOT nlohmann_json_FOUND)
    add_subdirectory(json)
endif()

add_subdirectory(LibAtrac9)

set(FFMPEG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/FFmpeg) 
add_custom_command(
    OUTPUT ${FFMPEG_PATH}/config.h
    COMMAND ./configure
    COMMENT "Configuring FFmpeg..."
    WORKING_DIRECTORY ${FFMPEG_PATH}
)
add_custom_target(ffmpeg-configure DEPENDS ${FFMPEG_PATH}/config.h)

add_custom_target(ffmpeg-build)
add_custom_command(
    TARGET ffmpeg-build
    COMMAND $(MAKE) -C ${FFMPEG_PATH}
    COMMENT "Building FFmpeg..."
    DEPENDS ffmpeg-configure
    WORKING_DIRECTORY ${FFMPEG_PATH}
)

add_library(ffmpeg-core INTERFACE)
add_dependencies(ffmpeg-core ffmpeg-build)

function(import_ffmpeg_library name)
    add_library(ffmpeg::${name} STATIC IMPORTED GLOBAL)
    set_property(TARGET ffmpeg::${name} PROPERTY IMPORTED_LOCATION "${FFMPEG_PATH}/lib${name}/lib${name}.a")
    set_property(TARGET ffmpeg::${name} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_PATH}")
endfunction()

import_ffmpeg_library(avcodec)
import_ffmpeg_library(avformat)
import_ffmpeg_library(avfilter)
import_ffmpeg_library(avdevice)
import_ffmpeg_library(avutil)
import_ffmpeg_library(swscale)
import_ffmpeg_library(swresample)
import_ffmpeg_library(postproc)

