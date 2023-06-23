cmake_minimum_required(VERSION 3.0)
project(libcrypto)
set(PROJECT_PATH crypto)

set(INCLUDE
    include/${PROJECT_PATH}/sha1.h
)

set(SRC
    src/sha1.c
)

add_library(${PROJECT_NAME} STATIC ${INCLUDE} ${SRC})
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE include/${PROJECT_PATH})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
