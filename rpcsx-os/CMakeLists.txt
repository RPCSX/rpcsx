add_library(standalone-config INTERFACE)
target_include_directories(standalone-config INTERFACE orbis-kernel-config)
add_library(orbis::kernel::config ALIAS standalone-config)

add_executable(rpcsx-os
  iodev/ajm.cpp
  iodev/blockpool.cpp
  iodev/bt.cpp
  iodev/camera.cpp
  iodev/cd.cpp
  iodev/console.cpp
  iodev/hdd.cpp
  iodev/dce.cpp
  iodev/dipsw.cpp
  iodev/dmem.cpp
  iodev/gc.cpp
  iodev/hid.cpp
  iodev/hmd_3da.cpp
  iodev/hmd_cmd.cpp
  iodev/hmd_mmap.cpp
  iodev/hmd_snsr.cpp
  iodev/icc_configuration.cpp
  iodev/mbus.cpp
  iodev/metadbg.cpp
  iodev/notification.cpp
  iodev/npdrm.cpp
  iodev/null.cpp
  iodev/rng.cpp
  iodev/sbl_srv.cpp
  iodev/shm.cpp
  iodev/urandom.cpp
  iodev/xpt.cpp
  iodev/zero.cpp
  iodev/aout.cpp
  iodev/av_control.cpp
  iodev/hdmi.cpp
  iodev/mbus_av.cpp
  iodev/scanin.cpp
  iodev/s3da.cpp
  iodev/gbase.cpp
  iodev/devstat.cpp
  iodev/devact.cpp
  iodev/devctl.cpp
  iodev/uvd.cpp
  iodev/vce.cpp

  main.cpp
  backtrace.cpp
  bridge.cpp
  vm.cpp
  ops.cpp
  linker.cpp
  io-device.cpp
  thread.cpp
  vfs.cpp
)

target_include_directories(rpcsx-os PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rpcsx-os PUBLIC orbis::kernel amdgpu::bridge rx libcrypto unwind unwind-x86_64 xbyak)
target_link_options(rpcsx-os PUBLIC "LINKER:-Ttext-segment,0x0000010000000000")
target_compile_options(rpcsx-os PRIVATE "-march=native")

set_target_properties(rpcsx-os PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
install(TARGETS rpcsx-os RUNTIME DESTINATION bin)
