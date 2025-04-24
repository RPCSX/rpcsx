add_library(
    rpcsx_cpu_cell_ppu STATIC
    src/Decoder.cpp
)

add_library(rpcsx_cpu_cell_ppu_semantic
STATIC
    semantic/ppu.cpp
)
target_include_directories(rpcsx_cpu_cell_ppu_semantic PUBLIC include PRIVATE include/rx/cpu/cell/ppu)
target_link_libraries(rpcsx_cpu_cell_ppu_semantic PUBLIC rx)

# add_custom_command(
#     OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ppu.ll
#     COMMAND ${CLANG_EXECUTABLE} -O3 -S -emit-llvm semantic/ppu.cpp -o ${CMAKE_CURRENT_BINARY_DIR}/ppu.ll -I include/rx/cpu/cell/ppu/ -I ../../../../rx/include/ -std=c++23 -fno-exceptions
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
# )

# add_custom_target(ppu-semantic DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ppu.ll)

target_include_directories(rpcsx_cpu_cell_ppu
    PUBLIC
        include
    PRIVATE
        include/rx/cpu/cell/ppu
)

target_link_libraries(rpcsx_cpu_cell_ppu PUBLIC rx)
# add_dependencies(rpcsx_cpu_cell_ppu ppu-semantic)
add_library(rpcsx::cpu::cell::ppu ALIAS rpcsx_cpu_cell_ppu)
add_library(rpcsx::cpu::cell::ppu::semantic ALIAS rpcsx_cpu_cell_ppu_semantic)

