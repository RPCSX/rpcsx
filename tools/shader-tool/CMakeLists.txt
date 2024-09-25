add_executable(shader-tool shader-tool.cpp)
target_link_libraries(shader-tool PUBLIC gcn-shader rx)

add_executable(gcn-shader-tool shader-tool.cpp)
target_link_libraries(gcn-shader-tool PUBLIC gcn-shader rx rdna-semantic-spirv)
target_compile_definitions(gcn-shader-tool PUBLIC GCN)
install(TARGETS gcn-shader-tool RUNTIME DESTINATION bin)
set_target_properties(gcn-shader-tool PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
