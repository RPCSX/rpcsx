if(NOT avutil IN_LIST ffmpeg_FIND_COMPONENTS)
    list(APPEND ffmpeg_FIND_COMPONENTS avutil)
endif()

find_package(PkgConfig QUIET)
foreach(c IN LISTS ffmpeg_FIND_COMPONENTS)
    pkg_search_module(ffmpeg_${c} QUIET IMPORTED_TARGET lib${c})
endforeach()

find_file(ffmpeg_VERSION_FILE libavutil/ffversion.h HINTS "${ffmpeg_avutil_INCLUDEDIR}")
if(ffmpeg_VERSION_FILE)
    file(STRINGS "${ffmpeg_VERSION_FILE}" ffmpeg_VERSION_LINE REGEX "FFMPEG_VERSION")
    string(REGEX MATCH "[0-9.]+" ffmpeg_VERSION "${ffmpeg_VERSION_LINE}")
    unset(ffmpeg_VERSION_LINE)
    unset(ffmpeg_VERSION_FILE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ffmpeg
    REQUIRED_VARS ffmpeg_avutil_LINK_LIBRARIES
    VERSION_VAR ffmpeg_VERSION
    HANDLE_COMPONENTS
)

foreach(c IN LISTS ffmpeg_FIND_COMPONENTS)
    if(ffmpeg_FOUND AND ffmpeg_${c}_FOUND AND NOT TARGET ffmpeg::${c})
        add_library(ffmpeg::${c} ALIAS PkgConfig::ffmpeg_${c})
    endif()
endforeach()
