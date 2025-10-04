add_library(standalone-config INTERFACE)
target_include_directories(standalone-config INTERFACE orbis-kernel-config)
add_library(orbis::kernel::config ALIAS standalone-config)

add_subdirectory(cpu)

if(LINUX AND WITH_PS4)
  find_package(libunwind REQUIRED)
  find_package(sox REQUIRED)
  find_package(ALSA REQUIRED)

  add_subdirectory(gpu)
  add_subdirectory(core)

  add_executable(rpcsx
    audio/AudioDevice.cpp
    audio/AlsaDevice.cpp

    iodev/a53io.cpp
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
    iodev/hmd2_cmd.cpp
    iodev/hmd2_imu.cpp
    iodev/hmd2_gen_data.cpp
    iodev/hmd2_gaze.cpp
    iodev/icc_configuration.cpp
    iodev/mbus.cpp
    iodev/metadbg.cpp
    iodev/notification.cpp
    iodev/npdrm.cpp
    iodev/nsid_ctl.cpp
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
    iodev/evlg.cpp
    iodev/srtc.cpp
    iodev/sshot.cpp
    iodev/lvdctl.cpp
    iodev/icc_power.cpp
    iodev/cayman_reg.cpp

    main.cpp
    AudioOut.cpp
    backtrace.cpp
    vm.cpp
    ops.cpp
    linker.cpp
    io-device.cpp
    thread.cpp
    vfs.cpp
    ipmi.cpp
  )

  target_base_address(rpcsx 0x0000070000000000)
  target_compile_options(rpcsx PRIVATE "-mfsgsbase")
  set_target_properties(rpcsx PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  install(TARGETS rpcsx RUNTIME DESTINATION bin)

  target_include_directories(rpcsx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(rpcsx
  PUBLIC
    ffmpeg::avcodec
    ffmpeg::swresample
    ffmpeg::avutil
    Atrac9
    rpcsx-gpu
    orbis::kernel
    rx
    libcrypto
    libunwind::unwind-x86_64
    xbyak::xbyak
    sox::sox
    ALSA::ALSA
    rpcsx-core
  )
endif()

