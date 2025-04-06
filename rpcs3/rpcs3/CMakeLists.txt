# Define GNU standard installation directories
include(GNUInstallDirs)

# Generate git-version.h at build time.
include(${CMAKE_CURRENT_SOURCE_DIR}/git-version.cmake)

# Check for a sufficient compiler and set build options
include(ConfigureCompiler)
include(CheckFunctionExists)

set(CMAKE_CXX_STANDARD 20)

set(ADDITIONAL_LIBS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #on some Linux distros shm_unlink and similar functions are in librt only
    set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} "rt")
elseif(NOT WIN32 AND NOT CMAKE_CXX_FLAGS MATCHES "LIBICONV_PLUG")
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

add_library(rpcs3 STATIC
        rpcs3_version.cpp
        module_verifier.cpp
        stb_image.cpp

        dev/iso.cpp

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

target_link_libraries(rpcs3 PUBLIC
    rpcs3_emu
    3rdparty::zlib
    3rdparty::pugixml
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
