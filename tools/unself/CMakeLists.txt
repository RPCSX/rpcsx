add_executable(unself unself.cpp)

set_target_properties(unself PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
install(TARGETS unself RUNTIME DESTINATION bin)
