
add_executable(rpcsx-gpu
  main.cpp
)

target_include_directories(rpcsx-gpu PUBLIC .)
target_link_libraries(rpcsx-gpu PUBLIC amdgpu::bridge amdgpu::device glfw vulkan)
set_target_properties(rpcsx-gpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
install(TARGETS rpcsx-gpu RUNTIME DESTINATION bin)
