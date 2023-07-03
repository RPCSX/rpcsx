
add_library(standalone-config INTERFACE)
target_include_directories(standalone-config INTERFACE orbis-kernel-config)
add_library(orbis::kernel::config ALIAS standalone-config)

add_executable(standalone src/main.cpp)
target_include_directories(standalone PUBLIC include)

target_link_libraries(standalone PUBLIC orbis::kernel)
