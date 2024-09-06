find_package(PkgConfig QUIET)
pkg_search_module(SOX QUIET IMPORTED_TARGET sox)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(sox
    REQUIRED_VARS SOX_LINK_LIBRARIES
    VERSION_VAR SOX_VERSION
)

if(sox_FOUND AND NOT TARGET sox::sox)
    add_library(sox::sox ALIAS PkgConfig::SOX)
endif()
