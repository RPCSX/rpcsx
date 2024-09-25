find_package(Vulkan 1.3 REQUIRED)

add_library(vk STATIC src/vk.cpp)

target_link_libraries(vk PUBLIC Vulkan::Vulkan rx)
target_include_directories(vk PUBLIC include)

