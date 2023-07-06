add_library(standalone-config INTERFACE)
target_include_directories(standalone-config INTERFACE orbis-kernel-config)
add_library(orbis::kernel::config ALIAS standalone-config)

add_executable(rpcsx-os 
  iodev/dce.cpp
  iodev/dipsw.cpp
  iodev/dmem.cpp
  iodev/gc.cpp
  iodev/hid.cpp
  iodev/hmd_3da.cpp
  iodev/hmd_cmd.cpp
  iodev/hmd_mmap.cpp
  iodev/hmd_snsr.cpp
  iodev/null.cpp
  iodev/rng.cpp
  iodev/stderr.cpp
  iodev/stdin.cpp
  iodev/stdout.cpp
  iodev/zero.cpp

  main.cpp
  bridge.cpp
  vm.cpp
  ops.cpp
  linker.cpp
  io-device.cpp
  thread.cpp
  vfs.cpp
)
target_include_directories(rpcsx-os PUBLIC .)
target_link_libraries(rpcsx-os PUBLIC orbis::kernel amdgpu::bridge libcrypto unwind unwind-x86_64)
target_link_options(rpcsx-os PUBLIC "LINKER:-Ttext-segment,0x0000010000000000")
target_compile_options(rpcsx-os PRIVATE "-march=native")

set_target_properties(rpcsx-os PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
install(TARGETS rpcsx-os RUNTIME DESTINATION bin)
