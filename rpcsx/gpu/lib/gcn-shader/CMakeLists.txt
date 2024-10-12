file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/dialect/)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/dialect/spv.hpp
    COMMAND $<TARGET_FILE:spv-gen> ${CMAKE_CURRENT_BINARY_DIR}/include/dialect/spv.hpp
    DEPENDS spv-gen
    WORKING_DIRECTORY $<TARGET_PROPERTY:SPIRV-Headers,INTERFACE_INCLUDE_DIRECTORIES>/spirv/unified1
    COMMENT "Generating ${CMAKE_CURRENT_BINARY_DIR}/include/dialect/spv.hpp..."
)

add_custom_target(shader-spv-dialect-gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/include/dialect/spv.hpp)
add_library(shader-spv-dialect INTERFACE)
add_dependencies(shader-spv-dialect shader-spv-dialect-gen)
target_include_directories(shader-spv-dialect INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/)

add_library(gcn-shader STATIC
    src/analyze.cpp
    src/eval.cpp
    src/Evaluator.cpp
    src/gcn.cpp
    src/GcnConverter.cpp
    src/GcnInstruction.cpp
    src/glsl.cpp
    src/ModuleInfo.cpp
    src/opt.cpp
    src/SemanticModuleInfo.cpp
    src/spv.cpp
    src/SpvConverter.cpp
    src/SpvTypeInfo.cpp
    src/transform.cpp
)

target_include_directories(gcn-shader PUBLIC include PRIVATE include/shader)

target_link_libraries(gcn-shader
PUBLIC
    shader-spv-dialect
    rx

PRIVATE
    glslang::glslang
    glslang::SPIRV
    SPIRV-Tools
    SPIRV-Tools-opt
    spirv-cross-glsl
)

add_subdirectory(shaders)
