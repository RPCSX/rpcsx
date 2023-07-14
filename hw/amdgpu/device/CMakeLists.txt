project(libamdgpu-device)
set(PROJECT_PATH amdgpu/device)

set(SRC
    src/device.cpp
)

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
            COMMAND glslangValidator -V --vn "${varname}" -o "${outputpath}" "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
            COMMENT "Generating ${outputname}..."
        )

        set(subtarget ".${target}-subtarget-${outputname}")
        add_custom_target(${subtarget} DEPENDS ${outputpath})
        add_dependencies(${target} ${subtarget})
    endforeach()
endfunction()

add_precompiled_vulkan_spirv(${PROJECT_NAME}-shaders
    src/rect_list.geom.glsl
)

find_package(SPIRV-Tools REQUIRED CONFIG)
find_package(SPIRV-Tools-opt REQUIRED CONFIG)
find_package(spirv_cross_core REQUIRED CONFIG)
find_package(spirv_cross_glsl REQUIRED CONFIG)

add_library(${PROJECT_NAME} STATIC ${INCLUDE} ${SRC})
target_link_libraries(${PROJECT_NAME}
PUBLIC
    spirv
    amdgpu::base
    amdgpu::bridge
    amdgpu::shader
    util
    SPIRV-Tools
    SPIRV-Tools-opt
    spirv-cross-glsl

PRIVATE
    ${PROJECT_NAME}-shaders
)

target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE include/${PROJECT_PATH})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
add_library(amdgpu::device ALIAS ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
