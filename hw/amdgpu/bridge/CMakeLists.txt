project(libamdgpu-bridge)
set(PROJECT_PATH amdgpu/bridge)

set(INCLUDE
    include/${PROJECT_PATH}/bridge.hpp
)

set(SRC
    src/bridge.cpp
)

add_library(${PROJECT_NAME} STATIC ${INCLUDE} ${SRC})
target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE include/${PROJECT_PATH})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
add_library(amdgpu::bridge ALIAS ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
