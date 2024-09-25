add_subdirectory(crypto)

if(NOT xbyak_FOUND)
    add_subdirectory(xbyak)
endif()

add_subdirectory(SPIRV-Headers)

add_subdirectory(SPIRV-Tools)

option(SPIRV_CROSS_SHARED "" on)
option(SPIRV_CROSS_STATIC "" on)
option(SPIRV_CROSS_ENABLE_GLSL "" on)
option(SPIRV_CROSS_ENABLE_HLSL "" off)
option(SPIRV_CROSS_ENABLE_MSL "" off)
option(SPIRV_CROSS_ENABLE_CPP "" off)
option(SPIRV_CROSS_ENABLE_REFLECT "" off)
option(SPIRV_CROSS_ENABLE_C_API "" off)
option(SPIRV_CROSS_ENABLE_UTIL "" off)
option(SPIRV_CROSS_CLI "" off)
option(SPIRV_CROSS_ENABLE_TESTS "" off)
option(SPIRV_CROSS_SKIP_INSTALL "" on)
add_subdirectory(SPIRV-Cross)

install(TARGETS spirv-cross-c-shared LIBRARY DESTINATION bin)
set_target_properties(spirv-cross-c-shared PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(glslang)
if(NOT TARGET glslang::glslang-standalone)
    add_executable(glslang::glslang-standalone ALIAS glslang-standalone)
endif()

if(NOT nlohmann_json_FOUND)
    add_subdirectory(json)
endif()

