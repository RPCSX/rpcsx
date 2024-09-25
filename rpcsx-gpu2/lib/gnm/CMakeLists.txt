add_library(gnm STATIC src/pm4.cpp src/mmio.cpp)
target_include_directories(gnm PUBLIC include PRIVATE include/gnm)

add_subdirectory(lib)
