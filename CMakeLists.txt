cmake_minimum_required(VERSION 3.20)
project(rpcsx)

set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_BUILD_RPATH_USE_ORIGIN on)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(nlohmann_json CONFIG)
find_package(SPIRV-Cross 1.6 MODULE)
find_package(SPIRV-Tools-opt MODULE)
find_package(xbyak CONFIG)

if(SPIRV-Tools-opt_FOUND)
    find_package(glslang CONFIG)
    find_package(SPIRV-Headers CONFIG)
endif()

add_subdirectory(3rdparty EXCLUDE_FROM_ALL)

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
    if(CMAKE_CXX_COMPILER_LINKER_ID MATCHES "^(LLD|MOLD)$")
        target_link_options(${target} PUBLIC "LINKER:--image-base=${address}")
    else()
        target_link_options(${target} PUBLIC "LINKER:-Ttext-segment,${address}")
    endif()
endfunction()

add_subdirectory(tools)

add_subdirectory(orbis-kernel)
add_subdirectory(rpcsx-os)
add_subdirectory(rpcsx-gpu)
add_subdirectory(rpcsx-gpu2)
add_subdirectory(hw/amdgpu)
add_subdirectory(rx)

target_compile_definitions(rx PRIVATE
    RX_TAG=0
    RX_TAG_VERSION=0
)
