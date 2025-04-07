cmake_minimum_required(VERSION 3.20)
project(rpcsx)

option(WITH_RPCSX "Enable RPCSX" ON)
option(WITH_RPCS3 "Enable RPCS3" OFF)
option(WITH_RPCS3_QT_UI "Enable RPCS3 UI" OFF)
option(WITHOUT_OPENGL "Disable OpenGL" OFF)
option(WITHOUT_OPENGLEW "Disable OpenGLEW" OFF)

# rpcs3 options
option(USE_NATIVE_INSTRUCTIONS "USE_NATIVE_INSTRUCTIONS makes rpcs3 compile with -march=native, which is useful for local builds, but not good for packages." ON)
option(WITH_LLVM "Enable usage of LLVM library" ON)
option(BUILD_LLVM "Build LLVM from git submodule" OFF)
option(STATIC_LINK_LLVM "Link against LLVM statically. This will get set to ON if you build LLVM from the submodule." OFF)
option(USE_FAUDIO "FAudio audio backend" ON)
option(USE_LIBEVDEV "libevdev-based joystick support" ON)
option(USE_DISCORD_RPC "Discord rich presence integration" OFF)
option(USE_SYSTEM_ZLIB "Prefer system ZLIB instead of the builtin one" ON)
option(USE_VULKAN "Vulkan render backend" ON)
option(USE_PRECOMPILED_HEADERS "Use precompiled headers" OFF)
option(USE_SDL "Enables SDL input handler" OFF)
option(USE_SYSTEM_SDL "Prefer system SDL instead of the builtin one" ON)
option(USE_SYSTEM_FFMPEG "Prefer system ffmpeg instead of the prebuild one" OFF)
option(USE_SYSTEM_OPENAL "Prefer system OpenAL instead of the prebuild one" ON)
option(USE_SYSTEM_CURL "Prefer system Curl instead of the prebuild one" ON)
option(USE_SYSTEM_OPENCV "Prefer system OpenCV instead of the builtin one" ON)
option(HAS_MEMORY_BREAKPOINTS "Add support for memory breakpoints to the interpreter" OFF)
option(USE_LTO "Use LTO for building" ON)


set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_BUILD_RPATH_USE_ORIGIN on)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(3rdparty EXCLUDE_FROM_ALL)

if (WITH_RPCSX)
    find_package(nlohmann_json CONFIG)

    function(add_precompiled_vulkan_spirv target)
        add_library(${target} INTERFACE)
        set(SPIRV_GEN_ROOT_DIR "spirv-gen/include/")
        set(SPIRV_GEN_DIR "${SPIRV_GEN_ROOT_DIR}/shaders")

        cmake_path(ABSOLUTE_PATH SPIRV_GEN_ROOT_DIR BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} NORMALIZE OUTPUT_VARIABLE outputrootdir)
        cmake_path(ABSOLUTE_PATH SPIRV_GEN_DIR BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} NORMALIZE OUTPUT_VARIABLE outputdir)
        file(MAKE_DIRECTORY ${outputrootdir})
        file(MAKE_DIRECTORY ${outputdir})
        target_include_directories(${target} INTERFACE ${outputrootdir})

        foreach(input IN LISTS ARGN)
            cmake_path(GET input FILENAME inputname)
            cmake_path(REPLACE_EXTENSION inputname LAST_ONLY .h OUTPUT_VARIABLE outputname)
            cmake_path(APPEND outputdir ${outputname} OUTPUT_VARIABLE outputpath)
            cmake_path(REMOVE_EXTENSION inputname LAST_ONLY OUTPUT_VARIABLE varname)

            string(REPLACE "." "_" varname ${varname})
            string(PREPEND varname "spirv_")

            add_custom_command(
                OUTPUT ${outputpath}
                COMMAND $<TARGET_FILE:glslang::glslang-standalone> -V --target-env vulkan1.2 --vn "${varname}" -o "${outputpath}" "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}" glslang::glslang-standalone
                COMMENT "Generating ${outputname}..."
            )

            set(subtarget ".${target}-subtarget-${outputname}")
            add_custom_target(${subtarget} DEPENDS ${outputpath})
            add_dependencies(${target} ${subtarget})
        endforeach()
    endfunction()

    function(target_base_address target address)
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE off)

        if(CMAKE_CXX_COMPILER_LINKER_ID MATCHES "^(LLD|MOLD)$")
            target_link_options(${target} PUBLIC "LINKER:--image-base=${address}")
        else()
            target_link_options(${target} PUBLIC "LINKER:-Ttext-segment,${address}")
        endif()
    endfunction()

    add_subdirectory(tools)

    add_subdirectory(orbis-kernel)
    add_subdirectory(rpcsx)
endif()

add_subdirectory(rx)

target_compile_definitions(rx PRIVATE
    RX_TAG=0
    RX_TAG_VERSION=0
)

if (WITH_RPCS3)
    add_subdirectory(rpcs3)
endif()

if (NOT ANDROID AND WITH_RPCS3_QT_UI AND WITH_RPCS3)
    add_subdirectory(rpcs3qt-legacy)
endif()
