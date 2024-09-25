add_precompiled_vulkan_spirv(amdgpu_tiler_vulkan_shaders
    shaders/tiler1d.comp.glsl
    shaders/tiler2d.comp.glsl
    shaders/tilerLinear.comp.glsl
    shaders/detiler1d.comp.glsl
    shaders/detiler2d.comp.glsl
    shaders/detilerLinear.comp.glsl
)

add_library(amdgpu_tiler STATIC src/tiler.cpp)
target_include_directories(amdgpu_tiler PUBLIC include)

add_library(amdgpu_tiler_cpu STATIC src/tiler_cpu.cpp)
add_library(amdgpu_tiler_vulkan STATIC src/tiler_vulkan.cpp)

target_link_libraries(amdgpu_tiler PUBLIC gnm)
target_link_libraries(amdgpu_tiler_cpu PUBLIC amdgpu_tiler)
target_link_libraries(amdgpu_tiler_vulkan PUBLIC amdgpu_tiler amdgpu_tiler_vulkan_shaders vk)

add_library(amdgpu::tiler ALIAS amdgpu_tiler)
add_library(amdgpu::tiler::cpu ALIAS amdgpu_tiler_cpu)
add_library(amdgpu::tiler::vulkan ALIAS amdgpu_tiler_vulkan)
