project(libamdgpu-shader)
set(PROJECT_PATH amdgpu/shader)

set(SRC
    src/cf.cpp
    src/scf.cpp
    src/CfBuilder.cpp
    src/Converter.cpp
    src/ConverterContext.cpp
    src/Fragment.cpp
    src/Function.cpp
    src/Instruction.cpp
    src/RegisterState.cpp
    src/TypeId.cpp
)

add_library(${PROJECT_NAME} STATIC ${INCLUDE} ${SRC})
target_link_libraries(${PROJECT_NAME} PUBLIC spirv amdgpu::base spirv-cross-core)
target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE include/${PROJECT_PATH})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
add_library(amdgpu::shader ALIAS ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
