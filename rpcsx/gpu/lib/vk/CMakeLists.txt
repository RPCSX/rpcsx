add_library(vk STATIC src/vk.cpp)

target_link_libraries(vk PUBLIC Vulkan::Loader Vulkan::Headers rx)
target_include_directories(vk PUBLIC include)

