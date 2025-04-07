include(DetectArchitecture)

# FFMPEG
if(NOT TARGET 3rdparty_ffmpeg)
	add_library(3rdparty_ffmpeg INTERFACE)

	# Select the version of ffmpeg to use, default is builtin
	if(USE_SYSTEM_FFMPEG)
		message(STATUS "RPCS3: using shared ffmpeg")
		find_package(FFMPEG REQUIRED)

		target_include_directories(3rdparty_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIR})
		target_link_libraries(3rdparty_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})
	else()
		message(STATUS "RPCS3: using builtin ffmpeg")
		add_subdirectory(ffmpeg EXCLUDE_FROM_ALL)
		# ffmpeg-core libraries are extracted to CMAKE_BINARY_DIR
		set(FFMPEG_LIB_DIR "${CMAKE_BINARY_DIR}/3rdparty/ffmpeg/lib")

		if (WIN32)
			target_link_libraries(3rdparty_ffmpeg INTERFACE "Bcrypt.lib")
		endif()

		find_library(FFMPEG_LIB_AVFORMAT avformat PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
		find_library(FFMPEG_LIB_AVCODEC avcodec PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
		find_library(FFMPEG_LIB_AVUTIL avutil PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
		find_library(FFMPEG_LIB_SWSCALE swscale PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)
		find_library(FFMPEG_LIB_SWRESAMPLE swresample PATHS ${FFMPEG_LIB_DIR} NO_DEFAULT_PATH)

		if (FFMPEG_LIB_AVFORMAT MATCHES "FFMPEG_LIB_AVFORMAT-NOTFOUND")
			message(FATAL_ERROR "@#$%! FFMPEG NOT FOUND! ${FFMPEG_LIB_DIR}")
		endif()

		target_link_libraries(3rdparty_ffmpeg
		INTERFACE
			${FFMPEG_LIB_AVFORMAT}
			${FFMPEG_LIB_AVCODEC}
			${FFMPEG_LIB_AVUTIL}
			${FFMPEG_LIB_SWSCALE}
			${FFMPEG_LIB_SWRESAMPLE}
		)
		target_include_directories(3rdparty_ffmpeg INTERFACE "ffmpeg/include")
	endif()
endif()


# DiscordRPC
add_subdirectory(discord-rpc)

add_library(3rdparty::ffmpeg ALIAS 3rdparty_ffmpeg)
add_library(3rdparty::discordRPC ALIAS 3rdparty_discordRPC)
