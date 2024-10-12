find_package(glfw3 3.3 REQUIRED)

add_precompiled_vulkan_spirv(rpcsx-gpu-shaders
    shaders/fill_red.frag.glsl
    shaders/flip_std.frag.glsl
    shaders/flip_alt.frag.glsl
    shaders/flip.vert.glsl
    shaders/rect_list.geom.glsl
)

add_library(rpcsx-gpu
STATIC
    Cache.cpp
    Device.cpp
    FlipPipeline.cpp
    Pipe.cpp
    Registers.cpp
    Renderer.cpp
)

target_link_libraries(rpcsx-gpu
PUBLIC
    rpcsx-gpu-shaders
    rx
    gcn-shader
    glfw
    amdgpu::tiler::cpu
    amdgpu::tiler::vulkan
    rdna-semantic-spirv
    gnm::vulkan
    gnm
    orbis::kernel
    rpcsx-core
)

add_subdirectory(lib)
