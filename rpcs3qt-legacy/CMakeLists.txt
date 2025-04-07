include(qt6.cmake)

add_library(rpcs3_ui STATIC
    about_dialog.cpp
    auto_pause_settings_dialog.cpp
    basic_mouse_settings_dialog.cpp
    breakpoint_handler.cpp
    breakpoint_list.cpp
    call_stack_list.cpp
    camera_settings_dialog.cpp
    cg_disasm_window.cpp
    cheat_manager.cpp
    config_adapter.cpp
    config_checker.cpp
    curl_handle.cpp
    custom_dialog.cpp
    custom_table_widget_item.cpp
    debugger_add_bp_window.cpp
    debugger_frame.cpp
    debugger_list.cpp
    downloader.cpp
    dimensions_dialog.cpp
    _discord_utils.cpp
    emu_settings.cpp
    elf_memory_dumping_dialog.cpp
    emulated_pad_settings_dialog.cpp
    fatal_error_dialog.cpp
    find_dialog.cpp
    flow_layout.cpp
    flow_widget.cpp
    flow_widget_item.cpp
    game_compatibility.cpp
    game_list.cpp
    game_list_base.cpp
    game_list_delegate.cpp
    game_list_frame.cpp
    game_list_grid.cpp
    game_list_grid_item.cpp
    game_list_table.cpp
    gui_application.cpp
    gl_gs_frame.cpp
    gs_frame.cpp
    gui_game_info.cpp
    gui_settings.cpp
    infinity_dialog.cpp
    input_dialog.cpp
    instruction_editor_dialog.cpp
    ipc_settings_dialog.cpp
    kernel_explorer.cpp
    localized.cpp
    localized_emu.cpp
    log_frame.cpp
    log_viewer.cpp
    main_window.cpp
    memory_string_searcher.cpp
    memory_viewer_panel.cpp
    microphone_creator.cpp
    midi_creator.cpp
    movie_item.cpp
    movie_item_base.cpp
    msg_dialog_frame.cpp
    osk_dialog_frame.cpp
    pad_led_settings_dialog.cpp
    pad_motion_settings_dialog.cpp
    pad_settings_dialog.cpp
    patch_creator_dialog.cpp
    patch_manager_dialog.cpp
    permissions.cpp
    persistent_settings.cpp
    pkg_install_dialog.cpp
    progress_dialog.cpp
    progress_indicator.cpp
    ps_move_tracker_dialog.cpp
    qt_camera_handler.cpp
    qt_camera_video_sink.cpp
    qt_music_handler.cpp
    qt_utils.cpp
    qt_video_source.cpp
    raw_mouse_settings_dialog.cpp
    register_editor_dialog.cpp
    recvmessage_dialog_frame.cpp
    render_creator.cpp
    rpcn_settings_dialog.cpp
    rsx_debugger.cpp
    save_data_dialog.cpp
    save_data_info_dialog.cpp
    save_data_list_dialog.cpp
    save_manager_dialog.cpp
    savestate_manager_dialog.cpp
    screenshot_item.cpp
    screenshot_manager_dialog.cpp
    screenshot_preview.cpp
    sendmessage_dialog_frame.cpp
    settings.cpp
    settings_dialog.cpp
    shortcut_utils.cpp
    shortcut_dialog.cpp
    shortcut_handler.cpp
    shortcut_settings.cpp
    skylander_dialog.cpp
    syntax_highlighter.cpp
    system_cmd_dialog.cpp
    table_item_delegate.cpp
    tooltips.cpp
    trophy_manager_dialog.cpp
    trophy_notification_frame.cpp
    trophy_notification_helper.cpp
    update_manager.cpp
    user_account.cpp
    user_manager_dialog.cpp
    uuid.cpp
    vfs_dialog.cpp
    vfs_dialog_path_widget.cpp
    vfs_dialog_tab.cpp
    vfs_dialog_usb_input.cpp
    vfs_dialog_usb_tab.cpp
    vfs_tool_dialog.cpp
    video_label.cpp
    welcome_dialog.cpp

    about_dialog.ui
    camera_settings_dialog.ui
    main_window.ui
    pad_led_settings_dialog.ui
    pad_motion_settings_dialog.ui
    pad_settings_dialog.ui
    patch_creator_dialog.ui
    patch_manager_dialog.ui
    ps_move_tracker_dialog.ui
    settings_dialog.ui
    shortcut_dialog.ui
    welcome_dialog.ui

    resources.qrc
)

if(HAS_MEMORY_BREAKPOINTS)
    target_compile_definitions(rpcs3_ui PRIVATE RPCS3_HAS_MEMORY_BREAKPOINTS)
endif()

if(WIN32)
    target_sources(rpcs3_ui PUBLIC windows.qrc)
    target_compile_definitions(rpcs3_ui PRIVATE UNICODE _UNICODE)
endif()

set_target_properties(rpcs3_ui
    PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON)

# AUTOMOC brings Windows.h to the sources, which have some definitions conflicting with winsock2.h
# define WIN32_LEAN_AND_MEAN resolve the problem
# https://docs.microsoft.com/en-us/windows/win32/winsock/creating-a-basic-winsock-application
# https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers#faster-builds-with-smaller-header-files
target_compile_definitions(rpcs3_ui PRIVATE WIN32_LEAN_AND_MEAN)

target_link_libraries(rpcs3_ui
    PUBLIC
        3rdparty::qt6
        3rdparty::yaml-cpp
        rpcs3)

if(TARGET OpenGL::EGL)
    target_link_libraries(rpcs3_ui PUBLIC OpenGL::EGL)
endif()


if (NOT ANDROID)
    if(WIN32)
        add_executable(rpcs3qt-ui-legacy WIN32)
        target_sources(rpcs3qt-ui-legacy PRIVATE rpcs3.rc)
        target_compile_definitions(rpcs3qt-ui-legacy PRIVATE UNICODE _UNICODE)
    elseif(APPLE)
        add_executable(rpcs3qt-ui-legacy MACOSX_BUNDLE)
        target_sources(rpcs3qt-ui-legacy PRIVATE rpcs3.icns update_helper.sh)
        set_source_files_properties(update_helper.sh PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        set_target_properties(rpcs3qt-ui-legacy
            PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/rpcs3.plist.in")
    else()
        add_executable(rpcs3qt-ui-legacy)
    endif()

    set_target_properties(rpcs3qt-ui-legacy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    target_sources(rpcs3qt-ui-legacy
        PRIVATE
        display_sleep_control.cpp
        headless_application.cpp
        main.cpp
        main_application.cpp

        Input/basic_keyboard_handler.cpp
        Input/basic_mouse_handler.cpp
        Input/gui_pad_thread.cpp
        Input/keyboard_pad_handler.cpp
    )

    set_target_properties(rpcs3qt-ui-legacy
        PROPERTIES
            AUTOMOC ON
            AUTOUIC ON)

    target_link_libraries(rpcs3qt-ui-legacy
        PRIVATE
            rpcs3_emu
            rpcs3_ui
            3rdparty::qt6)

    if(UNIX)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        find_package(Threads REQUIRED)
        target_link_libraries(rpcs3qt-ui-legacy PRIVATE Threads::Threads)
    endif()

    if(WIN32)
        target_link_libraries(rpcs3qt-ui-legacy PRIVATE bcrypt ws2_32 Iphlpapi Winmm Psapi gdi32 setupapi pdh)
    else()
        target_link_libraries(rpcs3qt-ui-legacy PRIVATE ${CMAKE_DL_LIBS})
    endif()

    # Copy icons to executable directory
    if(APPLE)
        if (CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
            set(QT_DEPLOY_FLAGS "-no-strip")
        else()
            set(QT_DEPLOY_FLAGS "")
        endif()
        qt_finalize_target(rpcs3qt-ui-legacy)
        add_custom_command(TARGET rpcs3qt-ui-legacy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rpcs3.icns $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/../Resources/rpcs3.icns
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/Icons $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/../Resources/Icons
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/GuiConfigs $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/../Resources/GuiConfigs
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/git $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/../Resources/git
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "${PROJECT_BINARY_DIR}/bin/rpcs3.app" "${QT_DEPLOY_FLAGS}")
    elseif(UNIX)
        add_custom_command(TARGET rpcs3qt-ui-legacy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/Icons $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/Icons
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/GuiConfigs $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/GuiConfigs
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/git $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/git)
    elseif(WIN32)
        add_custom_command(TARGET rpcs3qt-ui-legacy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:OpenAL::OpenAL> $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/Icons $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/Icons
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/GuiConfigs $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/GuiConfigs
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/rpcs3/bin/git $<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/git
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --no-opengl-sw --no-patchqt
                --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-ffmpeg --no-quick-import
                --plugindir "$<IF:$<CXX_COMPILER_ID:MSVC>,$<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/plugins,$<TARGET_FILE_DIR:rpcs3qt-ui-legacy>/share/qt6/plugins>"
                --verbose 0 "$<TARGET_FILE:rpcs3qt-ui-legacy>")
    endif()

    # Unix installation
    if(UNIX AND NOT APPLE)
        # Install the binary
        install(TARGETS rpcs3qt-ui-legacy RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        # Install the application icon and menu item
        install(FILES rpcs3.svg
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
        install(FILES rpcs3.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps)
        install(FILES rpcs3.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
        install(FILES rpcs3.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
        # Install other files
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/rpcs3/bin/Icons
            DESTINATION ${CMAKE_INSTALL_DATADIR}/rpcs3)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/rpcs3/bin/GuiConfigs
            DESTINATION ${CMAKE_INSTALL_DATADIR}/rpcs3)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/rpcs3/bin/git
            DESTINATION ${CMAKE_INSTALL_DATADIR}/rpcs3)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/rpcs3/bin/test
            DESTINATION ${CMAKE_INSTALL_DATADIR}/rpcs3)
    endif()
endif()
