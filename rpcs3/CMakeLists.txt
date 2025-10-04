add_subdirectory(3rdparty)

if (DISABLE_LTO)
    if (CMAKE_C_FLAGS)
        string(REGEX REPLACE "-flto[^ ]*" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
    endif()
    if (CMAKE_CXX_FLAGS)
        string(REGEX REPLACE "-flto[^ ]*" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    endif()
endif()

string(FIND "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}" "-flto" FOUND_LTO)
if (NOT FOUND_LTO EQUAL -1)
    message(FATAL_ERROR "RPCS3 doesn't support building with LTO, use -DDISABLE_LTO=TRUE to force-disable it")
endif()

if(NOT WIN32)
    add_compile_options(-pthread)
endif()

# TODO: do real installation, including copying directory structure
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PROJECT_BINARY_DIR}/bin")

# Define GNU standard installation directories
include(GNUInstallDirs)

# Generate git-version.h at build time.
include(${CMAKE_CURRENT_SOURCE_DIR}/git-version.cmake)

# Check for a sufficient compiler and set build options
include(ConfigureCompiler)
include(CheckFunctionExists)

set(CMAKE_CXX_STANDARD 23)

set(ADDITIONAL_LIBS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #on some Linux distros shm_unlink and similar functions are in librt only
    set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} "rt")
elseif(NOT WIN32 AND NOT CMAKE_CXX_FLAGS MATCHES "LIBICONV_PLUG" AND NOT ANDROID)
    #it seems like glibc includes the iconv functions we use but other libc
    #implementations like the one on OSX don't seem implement them
    set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} "iconv")
endif()

# subdirectories
add_subdirectory(Emu)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/rpcs3")
    # Optionally enable X11 for window management
    find_package(X11)
    if(X11_FOUND)
        target_compile_definitions(rpcs3_emu PUBLIC -DHAVE_X11)
    endif()
    find_package(Wayland)
    if(WAYLAND_FOUND)
        target_compile_definitions(rpcs3_emu PUBLIC -DHAVE_WAYLAND)
    endif()
endif()


gen_git_version(${CMAKE_CURRENT_SOURCE_DIR})

add_library(rpcs3_core STATIC
    util/atomic.cpp
    util/console.cpp
    util/emu_utils.cpp
    util/media_utils.cpp
    util/video_provider.cpp
    util/logs.cpp
    util/yaml.cpp
    util/vm_native.cpp
    util/dyn_lib.cpp
    util/sysinfo.cpp
    util/cpu_stats.cpp
    util/serialization_ext.cpp
    util/bin_patch.cpp
    util/cheat_info.cpp
    util/cond.cpp
    util/Config.cpp
    util/date_time.cpp
    util/File.cpp
    util/JITASM.cpp
    util/JITLLVM.cpp
    util/LUrlParser.cpp
    util/mutex.cpp
    util/rXml.cpp
    util/sema.cpp
    util/simple_ringbuf.cpp
    util/stack_trace.cpp
    util/StrFmt.cpp
    util/Thread.cpp
    util/version.cpp
)

target_include_directories(rpcs3_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rpcs3_core PUBLIC rx)

target_link_libraries(rpcs3_core PUBLIC
    rpcsx::cpu::cell::ppu # FIXME: remove
    rpcsx::fw::ps3::api # FIXME: remove
    cellos-kernel # FIXME: remove
    nlohmann_json::nlohmann_json
    3rdparty::stblib
    3rdparty::ffmpeg
    3rdparty::asmjit
    3rdparty::zlib
    3rdparty::pugixml
    3rdparty::yaml-cpp
)

if(APPLE)
    target_sources(rpcs3_core PRIVATE
        util/darwin/sysinfo_darwin.mm
    )
endif()

if(USE_PRECOMPILED_HEADERS)
    target_precompile_headers(rpcs3_core PUBLIC stdafx.h)
endif()

add_library(rpcs3 STATIC
    rpcs3_version.cpp
    module_verifier.cpp
    stb_image.cpp

    dev/iso.cpp

    Crypto/aes.cpp
    Crypto/aesni.cpp
    Crypto/decrypt_binaries.cpp
    Crypto/ec.cpp
    Crypto/key_vault.cpp
    Crypto/lz.cpp
    Crypto/md5.cpp
    Crypto/sha1.cpp
    Crypto/sha256.cpp
    Crypto/unedat.cpp
    Crypto/unpkg.cpp
    Crypto/unself.cpp
    Crypto/unzip.cpp
    Crypto/utils.cpp

    Loader/disc.cpp
    Loader/ELF.cpp
    Loader/mself.cpp
    Loader/PSF.cpp
    Loader/PUP.cpp
    Loader/TAR.cpp
    Loader/TROPUSR.cpp
    Loader/TRP.cpp

    Input/ds3_pad_handler.cpp
    Input/ds4_pad_handler.cpp
    Input/dualsense_pad_handler.cpp
    Input/evdev_joystick_handler.cpp
    Input/evdev_gun_handler.cpp
    Input/hid_pad_handler.cpp
    Input/mm_joystick_handler.cpp
    Input/pad_thread.cpp
    Input/product_info.cpp
    Input/ps_move_calibration.cpp
    Input/ps_move_config.cpp
    Input/ps_move_handler.cpp
    Input/ps_move_tracker.cpp
    Input/raw_mouse_config.cpp
    Input/raw_mouse_handler.cpp
    Input/sdl_pad_handler.cpp
    Input/skateboard_pad_handler.cpp
    Input/xinput_pad_handler.cpp
    Input/virtual_pad_handler.cpp
)

set_source_files_properties("util/JITLLVM.cpp" "util/JITASM.cpp" PROPERTIES
    COMPILE_FLAGS "$<IF:$<CXX_COMPILER_ID:MSVC>,/GR-,-fno-rtti>"
    SKIP_PRECOMPILE_HEADERS ON
)

set_source_files_properties("util/yaml.cpp" PROPERTIES
    COMPILE_FLAGS "$<IF:$<CXX_COMPILER_ID:MSVC>,/EHsc,-fexceptions>"
    SKIP_PRECOMPILE_HEADERS ON
)

target_link_libraries(rpcs3 PUBLIC
    rpcs3_emu
    rpcs3_core
    3rdparty::discordRPC
    3rdparty::hidapi
    3rdparty::libusb
    3rdparty::libpng
    3rdparty::7zip
    3rdparty::wolfssl
    3rdparty::libcurl
    3rdparty::opencv
    3rdparty::fusion
    3rdparty::rtmidi
    3rdparty::zstd
    ${ADDITIONAL_LIBS}
)

if(USE_PRECOMPILED_HEADERS)
    target_precompile_headers(rpcs3 PUBLIC stdafx.h)
endif()

# Unix display manager
if(X11_FOUND)
    target_link_libraries(rpcs3 PRIVATE X11::X11)
elseif(USE_VULKAN AND UNIX AND NOT WAYLAND_FOUND AND NOT APPLE AND NOT ANDROID)
    # Wayland has been checked in 3rdparty/CMakeLists.txt already.
    message(FATAL_ERROR "RPCS3 requires either X11 or Wayland (or both) for Vulkan.")
endif()
