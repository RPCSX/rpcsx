find_package(Vulkan 1.3 REQUIRED)
find_package(glfw3 3.3 REQUIRED)

add_executable(rpcsx-gpu-legacy
  main.cpp
)

target_include_directories(rpcsx-gpu-legacy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rpcsx-gpu-legacy PUBLIC amdgpu::bridge amdgpu::device glfw Vulkan::Vulkan rx)
set_target_properties(rpcsx-gpu-legacy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
target_base_address(rpcsx-gpu-legacy 0x0000060000000000)
install(TARGETS rpcsx-gpu-legacy RUNTIME DESTINATION bin)
