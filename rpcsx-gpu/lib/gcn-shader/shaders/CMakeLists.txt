set(OUTPUT_FILENAME rdna-semantic-spirv.hpp)
set(INCLUDE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)
set(OUTPUT_DIRECTORY ${INCLUDE_DIRECTORY}/shaders)
set(OUTPUT_FILE ${OUTPUT_DIRECTORY}/${OUTPUT_FILENAME})
set(INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/rdna.glsl)
file(MAKE_DIRECTORY ${OUTPUT_DIRECTORY})

add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND $<TARGET_FILE:shader-tool> --output-type spirv-header --output-var-name g_rdna_semantic_spirv -i ${INPUT_FILE} -o ${OUTPUT_FILE}
    DEPENDS shader-tool ${INPUT_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating ${OUTPUT_FILE}..."
)

add_custom_target(rdna-semantic-spirv-gen DEPENDS ${OUTPUT_FILE})
add_library(rdna-semantic-spirv INTERFACE)
add_dependencies(rdna-semantic-spirv rdna-semantic-spirv-gen)
target_include_directories(rdna-semantic-spirv INTERFACE ${INCLUDE_DIRECTORY})
