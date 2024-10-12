add_library(rpcsx-core
STATIC
    src/Config.cpp
    src/watchdog.cpp
)

target_include_directories(rpcsx-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(rpcsx-core PUBLIC orbis::kernel rx rpcsx-gpu)
