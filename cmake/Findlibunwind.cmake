find_package(PkgConfig QUIET)
pkg_search_module(UNWIND QUIET IMPORTED_TARGET libunwind)

find_library(libunwind_x86_64_LIBRARY
    NAMES unwind-x86_64
    HINTS "${UNWIND_LIBRARY_DIRS}"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libunwind
    REQUIRED_VARS UNWIND_LINK_LIBRARIES libunwind_x86_64_LIBRARY
    VERSION_VAR UNWIND_VERSION
)

if(libunwind_FOUND AND NOT TARGET libunwind::unwind-x86_64)
    add_library(libunwind::unwind-x86_64 UNKNOWN IMPORTED)
    set_target_properties(libunwind::unwind-x86_64 PROPERTIES
        IMPORTED_LOCATION "${libunwind_x86_64_LIBRARY}"
    )
    target_link_libraries(libunwind::unwind-x86_64 INTERFACE PkgConfig::UNWIND)
endif()

mark_as_advanced(libunwind_x86_64_LIBRARY)
