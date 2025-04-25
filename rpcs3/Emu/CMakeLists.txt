add_library(rpcs3_emu STATIC
    cache_utils.cpp
    games_config.cpp
    IdManager.cpp
    localized_string.cpp
    savestate_utils.cpp
    scoped_progress_dialog.cpp
    System.cpp
    system_config.cpp
    system_config_types.cpp
    system_progress.cpp
    system_utils.cpp
    vfs_config.cpp
    VFS.cpp
    GDB.cpp
    title.cpp
    perf_meter.cpp
    perf_monitor.cpp
    IPC_config.cpp
    IPC_socket.cpp
)

if(USE_LTO)
    set_target_properties(rpcs3_emu PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if(USE_ASAN)
    set_source_files_properties(../../Utilities/Thread.cpp PROPERTIES COMPILE_DEFINITIONS USE_ASAN)
endif()

if(HAS_MEMORY_BREAKPOINTS)
    target_compile_definitions(rpcs3_emu PRIVATE RPCS3_HAS_MEMORY_BREAKPOINTS)
endif()

target_link_libraries(rpcs3_emu
    PRIVATE
        3rdparty::zlib 3rdparty::yaml-cpp 3rdparty::zstd
    PUBLIC
        3rdparty::libevdev 3rdparty::flatbuffers)


find_package(Threads REQUIRED)
target_link_libraries(rpcs3_emu
    PUBLIC Threads::Threads)

# For stdafx.h
target_include_directories(rpcs3_emu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(rpcs3_emu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty
)

target_include_directories(rpcs3_emu PUBLIC "${CMAKE_SOURCE_DIR}")

target_link_libraries(rpcs3_emu
    PUBLIC
        3rdparty::pugixml)

# Audio
target_sources(rpcs3_emu PRIVATE
    Audio/audio_resampler.cpp
    Audio/audio_utils.cpp
    Audio/AudioDumper.cpp
    Audio/AudioBackend.cpp
    Audio/Cubeb/CubebBackend.cpp
    Audio/Cubeb/cubeb_enumerator.cpp
)

target_compile_definitions(rpcs3_emu PUBLIC WIN32_LEAN_AND_MEAN)


if(USE_FAUDIO)
    find_package(SDL3)
    if(SDL3_FOUND AND SDL3_VERSION VERSION_GREATER_EQUAL 3.2.0)
        target_sources(rpcs3_emu PRIVATE
            Audio/FAudio/FAudioBackend.cpp
            Audio/FAudio/faudio_enumerator.cpp
        )
        target_link_libraries(rpcs3_emu PUBLIC 3rdparty::faudio)
    endif()
endif()

if(WIN32)
    if(NOT MSVC)
        target_link_libraries(rpcs3_emu PRIVATE xaudio2_9)
    endif()
    target_sources(rpcs3_emu PRIVATE
        Audio/XAudio2/XAudio2Backend.cpp
        Audio/XAudio2/xaudio2_enumerator.cpp
    )
    target_compile_definitions(rpcs3_emu PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
endif()

target_link_libraries(rpcs3_emu
    PUBLIC
        3rdparty::openal
        3rdparty::cubeb
        3rdparty::soundtouch
        3rdparty::miniupnpc
        3rdparty::json
)

# Cell
target_sources(rpcs3_emu PRIVATE
    Cell/ErrorCodes.cpp
    Cell/MFC.cpp
    Cell/PPUAnalyser.cpp
    Cell/PPUDisAsm.cpp
    Cell/PPUFunction.cpp
    Cell/PPUInterpreter.cpp
    Cell/PPUModule.cpp
    Cell/PPUThread.cpp
    Cell/PPUTranslator.cpp
    Cell/RawSPUThread.cpp
    Cell/SPUAnalyser.cpp
    Cell/SPUASMJITRecompiler.cpp
    Cell/SPUDisAsm.cpp
    Cell/SPUInterpreter.cpp
    Cell/SPUCommonRecompiler.cpp
    Cell/SPULLVMRecompiler.cpp
    Cell/SPUThread.cpp
    Cell/lv2/lv2.cpp
    Cell/lv2/sys_bdemu.cpp
    Cell/lv2/sys_btsetting.cpp
    Cell/lv2/sys_cond.cpp
    Cell/lv2/sys_console.cpp
    Cell/lv2/sys_crypto_engine.cpp
    Cell/lv2/sys_config.cpp
    Cell/lv2/sys_dbg.cpp
    Cell/lv2/sys_event.cpp
    Cell/lv2/sys_event_flag.cpp
    Cell/lv2/sys_fs.cpp
    Cell/lv2/sys_game.cpp
    Cell/lv2/sys_gamepad.cpp
    Cell/lv2/sys_gpio.cpp
    Cell/lv2/sys_hid.cpp
    Cell/lv2/sys_interrupt.cpp
    Cell/lv2/sys_io.cpp
    Cell/lv2/sys_lwcond.cpp
    Cell/lv2/sys_lwmutex.cpp
    Cell/lv2/sys_memory.cpp
    Cell/lv2/sys_mmapper.cpp
    Cell/lv2/sys_mutex.cpp
    Cell/lv2/sys_net.cpp
    Cell/lv2/sys_net/lv2_socket.cpp
    Cell/lv2/sys_net/lv2_socket_native.cpp
    Cell/lv2/sys_net/lv2_socket_raw.cpp
    Cell/lv2/sys_net/lv2_socket_p2p.cpp
    Cell/lv2/sys_net/lv2_socket_p2ps.cpp
    Cell/lv2/sys_net/network_context.cpp
    Cell/lv2/sys_net/nt_p2p_port.cpp
    Cell/lv2/sys_net/sys_net_helpers.cpp
    Cell/lv2/sys_overlay.cpp
    Cell/lv2/sys_ppu_thread.cpp
    Cell/lv2/sys_process.cpp
    Cell/lv2/sys_prx.cpp
    Cell/lv2/sys_rsx.cpp
    Cell/lv2/sys_rsxaudio.cpp
    Cell/lv2/sys_rwlock.cpp
    Cell/lv2/sys_semaphore.cpp
    Cell/lv2/sys_spu.cpp
    Cell/lv2/sys_sm.cpp
    Cell/lv2/sys_ss.cpp
    Cell/lv2/sys_storage.cpp
    Cell/lv2/sys_time.cpp
    Cell/lv2/sys_timer.cpp
    Cell/lv2/sys_trace.cpp
    Cell/lv2/sys_tty.cpp
    Cell/lv2/sys_uart.cpp
    Cell/lv2/sys_usbd.cpp
    Cell/lv2/sys_vm.cpp
)

if(NOT MSVC)
    set_source_files_properties(Cell/PPUTranslator.cpp PROPERTIES
        COMPILE_FLAGS -fno-rtti
        SKIP_PRECOMPILE_HEADERS ON
    )
endif()

target_link_libraries(rpcs3_emu
    PRIVATE
        3rdparty::stblib 3rdparty::libpng
)


# CPU
target_sources(rpcs3_emu PRIVATE
    CPU/CPUThread.cpp
    CPU/CPUTranslator.cpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
    target_sources(rpcs3_emu PRIVATE
        CPU/Backends/AArch64/AArch64ASM.cpp
        CPU/Backends/AArch64/AArch64Common.cpp
        CPU/Backends/AArch64/AArch64JIT.cpp
        CPU/Backends/AArch64/AArch64Signal.cpp
    )
endif()

target_link_libraries(rpcs3_emu
    PUBLIC 3rdparty::llvm 3rdparty::asmjit)


# Io
target_sources(rpcs3_emu PRIVATE
    Io/Buzz.cpp
    Io/camera_config.cpp
    Io/Dimensions.cpp
    Io/GameTablet.cpp
    Io/GHLtar.cpp
    Io/GunCon3.cpp
    Io/Infinity.cpp
    Io/interception.cpp
    Io/KeyboardHandler.cpp
    Io/midi_config_types.cpp
    Io/mouse_config.cpp
    Io/MouseHandler.cpp
    Io/pad_config.cpp
    Io/pad_config_types.cpp
    Io/pad_types.cpp
    Io/PadHandler.cpp
    Io/rb3drums_config.cpp
    Io/RB3MidiDrums.cpp
    Io/RB3MidiGuitar.cpp
    Io/RB3MidiKeyboard.cpp
    Io/recording_config.cpp
    Io/Skylander.cpp
    Io/TopShotElite.cpp
    Io/TopShotFearmaster.cpp
    Io/Turntable.cpp
    Io/usb_device.cpp
    Io/usb_vfs.cpp
    Io/usio.cpp
)

target_link_libraries(rpcs3_emu PRIVATE
    3rdparty::rtmidi
)

# Np
target_sources(rpcs3_emu PRIVATE
    NP/fb_helpers.cpp
    NP/np_cache.cpp
    NP/np_gui_cache.cpp
    NP/np_contexts.cpp
    NP/np_dnshook.cpp
    NP/np_requests_gui.cpp
    NP/np_handler.cpp
    NP/np_helpers.cpp
    NP/np_notifications.cpp
    NP/np_requests.cpp
    NP/signaling_handler.cpp
    NP/np_structs_extra.cpp
    NP/rpcn_client.cpp
    NP/rpcn_config.cpp
    NP/rpcn_countries.cpp
    NP/upnp_config.cpp
    NP/upnp_handler.cpp
    NP/ip_address.cpp
)

# Memory
target_sources(rpcs3_emu PRIVATE
    Memory/vm.cpp
)

# RSX
target_sources(rpcs3_emu PRIVATE
    RSX/Capture/rsx_capture.cpp
    RSX/Capture/rsx_replay.cpp
    RSX/Common/BufferUtils.cpp
    RSX/Common/surface_store.cpp
    RSX/Common/TextureUtils.cpp
    RSX/Common/texture_cache.cpp
    RSX/Common/texture_cache_types.cpp
    RSX/Core/RSXContext.cpp
    RSX/Core/RSXDisplay.cpp
    RSX/Core/RSXDrawCommands.cpp
    RSX/gcm_enums.cpp
    RSX/gcm_printing.cpp
    RSX/GSRender.cpp
    RSX/Host/MM.cpp
    RSX/Host/RSXDMAWriter.cpp
    RSX/Null/NullGSRender.cpp
    RSX/NV47/FW/draw_call.cpp
    RSX/NV47/FW/reg_context.cpp
    RSX/NV47/HW/common.cpp
    RSX/NV47/HW/nv0039.cpp
    RSX/NV47/HW/nv3089.cpp
    RSX/NV47/HW/nv308a.cpp
    RSX/NV47/HW/nv406e.cpp
    RSX/NV47/HW/nv4097.cpp
    RSX/Overlays/FriendsList/overlay_friends_list_dialog.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu_components.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu_main_menu.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu_message_box.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu_page.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu_settings.cpp
    RSX/Overlays/HomeMenu/overlay_home_menu_savestate.cpp
    RSX/Overlays/Network/overlay_recvmessage_dialog.cpp
    RSX/Overlays/Network/overlay_sendmessage_dialog.cpp
    RSX/Overlays/Trophies/overlay_trophy_list_dialog.cpp
    RSX/Overlays/overlays.cpp
    RSX/Overlays/overlay_animated_icon.cpp
    RSX/Overlays/overlay_animation.cpp
    RSX/Overlays/overlay_compile_notification.cpp
    RSX/Overlays/overlay_controls.cpp
    RSX/Overlays/overlay_cursor.cpp
    RSX/Overlays/overlay_debug_overlay.cpp
    RSX/Overlays/overlay_edit_text.cpp
    RSX/Overlays/overlay_fonts.cpp
    RSX/Overlays/overlay_list_view.cpp
    RSX/Overlays/overlay_manager.cpp
    RSX/Overlays/overlay_media_list_dialog.cpp
    RSX/Overlays/overlay_message.cpp
    RSX/Overlays/overlay_message_dialog.cpp
    RSX/Overlays/overlay_osk.cpp
    RSX/Overlays/overlay_osk_panel.cpp
    RSX/Overlays/overlay_perf_metrics.cpp
    RSX/Overlays/overlay_progress_bar.cpp
    RSX/Overlays/overlay_save_dialog.cpp
    RSX/Overlays/overlay_trophy_notification.cpp
    RSX/Overlays/overlay_user_list_dialog.cpp
    RSX/Overlays/overlay_utils.cpp
    RSX/Overlays/overlay_video.cpp
    RSX/Overlays/Shaders/shader_loading_dialog.cpp
    RSX/Overlays/Shaders/shader_loading_dialog_native.cpp
    RSX/Program/CgBinaryFragmentProgram.cpp
    RSX/Program/CgBinaryVertexProgram.cpp
    RSX/Program/FragmentProgramDecompiler.cpp
    RSX/Program/FragmentProgramRegister.cpp
    RSX/Program/GLSLCommon.cpp
    RSX/Program/ProgramStateCache.cpp
    RSX/Program/program_util.cpp
    RSX/Program/SPIRVCommon.cpp
    RSX/Program/VertexProgramDecompiler.cpp
    RSX/GSFrameBase.cpp
    RSX/RSXDisAsm.cpp
    RSX/RSXFIFO.cpp
    RSX/RSXOffload.cpp
    RSX/RSXTexture.cpp
    RSX/RSXThread.cpp
    RSX/RSXZCULL.cpp
    RSX/rsx_methods.cpp
    RSX/rsx_utils.cpp
    RSX/rsx_vertex_data.cpp
)

if(NOT ANDROID)
    target_sources(rpcs3_emu PRIVATE
        RSX/GL/GLCommonDecompiler.cpp
        RSX/GL/GLCompute.cpp
        RSX/GL/GLDMA.cpp
        RSX/GL/GLDraw.cpp
        RSX/GL/GLFragmentProgram.cpp
        RSX/GL/GLGSRender.cpp
        RSX/GL/GLHelpers.cpp
        RSX/GL/GLOverlays.cpp
        RSX/GL/GLPipelineCompiler.cpp
        RSX/GL/GLPresent.cpp
        RSX/GL/GLRenderTargets.cpp
        RSX/GL/GLResolveHelper.cpp
        RSX/GL/GLShaderInterpreter.cpp
        RSX/GL/GLTexture.cpp
        RSX/GL/GLTextureCache.cpp
        RSX/GL/glutils/blitter.cpp
        RSX/GL/glutils/buffer_object.cpp
        RSX/GL/glutils/capabilities.cpp
        RSX/GL/glutils/common.cpp
        RSX/GL/glutils/fbo.cpp
        RSX/GL/glutils/image.cpp
        RSX/GL/glutils/program.cpp
        RSX/GL/glutils/ring_buffer.cpp
        RSX/GL/glutils/sampler.cpp
        RSX/GL/GLVertexBuffers.cpp
        RSX/GL/GLVertexProgram.cpp
        RSX/GL/OpenGL.cpp
        RSX/GL/upscalers/fsr1/fsr_pass.cpp
    )
endif()

if(TARGET 3rdparty_vulkan)
    target_sources(rpcs3_emu PRIVATE
        RSX/VK/upscalers/fsr1/fsr_pass.cpp
        RSX/VK/vkutils/barriers.cpp
        RSX/VK/vkutils/buffer_object.cpp
        RSX/VK/vkutils/chip_class.cpp
        RSX/VK/vkutils/commands.cpp
        RSX/VK/vkutils/data_heap.cpp
        RSX/VK/vkutils/descriptors.cpp
        RSX/VK/vkutils/image.cpp
        RSX/VK/vkutils/image_helpers.cpp
        RSX/VK/vkutils/instance.cpp
        RSX/VK/vkutils/scratch.cpp
        RSX/VK/vkutils/sync.cpp
        RSX/VK/vkutils/swapchain.cpp
        RSX/VK/vkutils/memory.cpp
        RSX/VK/vkutils/device.cpp
        RSX/VK/vkutils/sampler.cpp
        RSX/VK/vkutils/shared.cpp
        RSX/VK/VKAsyncScheduler.cpp
        RSX/VK/VKCommandStream.cpp
        RSX/VK/VKCommonDecompiler.cpp
        RSX/VK/VKCommonPipelineLayout.cpp
        RSX/VK/VKCompute.cpp
        RSX/VK/VKDMA.cpp
        RSX/VK/VKDraw.cpp
        RSX/VK/VKFormats.cpp
        RSX/VK/VKFragmentProgram.cpp
        RSX/VK/VKFramebuffer.cpp
        RSX/VK/VKGSRender.cpp
        RSX/VK/VKHelpers.cpp
        RSX/VK/VKMemAlloc.cpp
        RSX/VK/VKOverlays.cpp
        RSX/VK/VKPipelineCompiler.cpp
        RSX/VK/VKPresent.cpp
        RSX/VK/VKProgramPipeline.cpp
        RSX/VK/VKQueryPool.cpp
        RSX/VK/VKRenderPass.cpp
        RSX/VK/VKRenderTargets.cpp
        RSX/VK/VKResolveHelper.cpp
        RSX/VK/VKResourceManager.cpp
        RSX/VK/VKShaderInterpreter.cpp
        RSX/VK/VKTexture.cpp
        RSX/VK/VKVertexBuffers.cpp
        RSX/VK/VKVertexProgram.cpp
        RSX/VK/VKTextureCache.cpp
    )
endif()

target_link_libraries(rpcs3_emu
    PUBLIC
        3rdparty::ffmpeg 3rdparty::sdl3
        3rdparty::opengl 3rdparty::stblib
        3rdparty::vulkan 3rdparty::glew
        3rdparty::libusb 3rdparty::wolfssl
        Vulkan::Headers
        rpcsx::fw::ps3::api
        rpcsx::cpu::cell::ppu
        rpcsx::cpu::cell::ppu::semantic

    PRIVATE
        3rdparty::glslang
)

if(APPLE)
    check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
    if (HAVE_CLOCK_GETTIME)
        target_compile_definitions(rpcs3_emu PUBLIC -DHAVE_CLOCK_GETTIME)
    endif()
endif()


if(USE_PRECOMPILED_HEADERS)
    target_precompile_headers(rpcs3_emu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../stdafx.h")
endif()
