find_package(Vulkan 1.3 REQUIRED)
find_package(glfw3 3.3 REQUIRED)

add_executable(rpcsx-gpu
  main.cpp
)

target_include_directories(rpcsx-gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rpcsx-gpu PUBLIC amdgpu::bridge amdgpu::device glfw Vulkan::Vulkan rx)
set_target_properties(rpcsx-gpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
install(TARGETS rpcsx-gpu RUNTIME DESTINATION bin)
