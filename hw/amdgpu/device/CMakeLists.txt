project(libamdgpu-device)
set(PROJECT_PATH amdgpu/device)

set(SRC
    src/device.cpp
)

add_precompiled_vulkan_spirv(${PROJECT_NAME}-shaders
    src/rect_list.geom.glsl
)

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
    $<$<CONFIG:Debug>:spirv-cross-glsl>

PRIVATE
    ${PROJECT_NAME}-shaders
)

target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE include/${PROJECT_PATH})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
add_library(amdgpu::device ALIAS ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
