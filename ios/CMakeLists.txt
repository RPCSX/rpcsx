cmake_minimum_required(VERSION 3.16.9)
project("rpcsx-ios")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE on)

set(WITH_RPCSX off)
set(WITH_RPCS3 on)
set(USE_SYSTEM_CURL off)
set(USE_DISCORD_RPC off)
set(USE_SYSTEM_OPENCV off)
set(USE_SYSTEM_FFMPEG off)
set(USE_FAUDIO off)
set(USE_SDL2 off)
set(WITH_LLVM off)
set(BUILD_LLVM off)
set(STATIC_LINK_LLVM off)
set(DISABLE_LTO on)
set(USE_LTO off)
set(USE_OPENSL off)
set(ASMJIT_NO_SHM_OPEN on)
set(USE_SYSTEM_ZLIB off)
set(USE_LIBEVDEV off)
set(COMPILE_FFMPEG on)
set(WITHOUT_OPENGL on)
set(WITHOUT_OPENGLEW on)
set(WITHOUT_OPENAL on)
set(USE_SYSTEM_SDL off)


add_subdirectory(${CMAKE_SOURCE_DIR}/.. rpcsx EXCLUDE_FROM_ALL)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC rpcs3/rpcs3)

target_link_libraries(${CMAKE_PROJECT_NAME}
    rpcs3
    rpcsx::fw::ps3
    rx
    ios
)
