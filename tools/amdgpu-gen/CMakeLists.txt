
if (NOT TARGET pugixml::pugixml)
    find_package(pugixml)
endif()

if (TARGET pugixml::pugixml)
    add_executable(amdgpu-gen amdgpu-gen.cpp)
    target_link_libraries(amdgpu-gen PUBLIC pugixml::pugixml nlohmann_json::nlohmann_json)
endif()
