add_library(gnm_vulkan STATIC src/vulkan.cpp)
add_library(gnm::vulkan ALIAS gnm_vulkan)

target_include_directories(gnm_vulkan PUBLIC include PRIVATE include/gnm)
target_link_libraries(gnm_vulkan PUBLIC vk gnm)
