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
    DeviceCtl.cpp
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
    3rdparty::glfw
    amdgpu::tiler::cpu
    amdgpu::tiler::vulkan
    rdna-semantic-spirv
    gnm::vulkan
    gnm
    orbis::kernel
    rpcsx-core
)

add_subdirectory(lib)
