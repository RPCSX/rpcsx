add_library(cellos-kernel STATIC
    src/lv2.cpp
    src/sys_bdemu.cpp
    src/sys_btsetting.cpp
    src/sys_cond.cpp
    src/sys_console.cpp
    src/sys_crypto_engine.cpp
    src/sys_config.cpp
    src/sys_dbg.cpp
    src/sys_event.cpp
    src/sys_event_flag.cpp
    src/sys_fs.cpp
    src/sys_game.cpp
    src/sys_gamepad.cpp
    src/sys_gpio.cpp
    src/sys_hid.cpp
    src/sys_interrupt.cpp
    src/sys_io.cpp
    src/sys_lwcond.cpp
    src/sys_lwmutex.cpp
    src/sys_memory.cpp
    src/sys_mmapper.cpp
    src/sys_mutex.cpp
    src/sys_net.cpp
    src/sys_net/lv2_socket.cpp
    src/sys_net/lv2_socket_native.cpp
    src/sys_net/lv2_socket_raw.cpp
    src/sys_net/lv2_socket_p2p.cpp
    src/sys_net/lv2_socket_p2ps.cpp
    src/sys_net/network_context.cpp
    src/sys_net/nt_p2p_port.cpp
    src/sys_net/sys_net_helpers.cpp
    src/sys_overlay.cpp
    src/sys_ppu_thread.cpp
    src/sys_process.cpp
    src/sys_prx.cpp
    src/sys_rsx.cpp
    src/sys_rsxaudio.cpp
    src/sys_rwlock.cpp
    src/sys_semaphore.cpp
    src/sys_spu.cpp
    src/sys_sm.cpp
    src/sys_ss.cpp
    src/sys_storage.cpp
    src/sys_time.cpp
    src/sys_timer.cpp
    src/sys_trace.cpp
    src/sys_tty.cpp
    src/sys_uart.cpp
    src/sys_usbd.cpp
    src/sys_vm.cpp
)

target_include_directories(cellos-kernel
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include

PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cellos
)

target_link_libraries(cellos-kernel PUBLIC
    rpcs3_core # FIXME: remove
    3rdparty::soundtouch # FIXME: remove
    3rdparty::flatbuffers # FIXME: remove
    3rdparty::wolfssl  # FIXME: remove
    3rdparty::miniupnpc # FIXME: remove
    3rdparty::libusb  # FIXME: remove
    3rdparty::rtmidi  # FIXME: remove
)
