find_library(SPIRV-Cross_core_LIBRARY NAMES spirv-cross-core)
find_library(SPIRV-Cross_glsl_LIBRARY NAMES spirv-cross-glsl)

find_path(SPIRV-Cross_INCLUDE_DIR NAMES spirv.hpp PATH_SUFFIXES spirv_cross)
if(SPIRV-Cross_INCLUDE_DIR)
    if(EXISTS "${SPIRV-Cross_INCLUDE_DIR}/spirv.hpp")
        file(STRINGS "${SPIRV-Cross_INCLUDE_DIR}/spirv.hpp" _ver_line
            REGEX "^[\t ]*#define[\t ]+SPV_VERSION[\t ]+0x[0-9]+"
            LIMIT_COUNT 1
        )
        string(REGEX MATCH "0x[0-9]+" _ver "${_ver_line}")
        math(EXPR SPIRV-Cross_MAJOR_VERSION "${_ver} >> 16")
        math(EXPR SPIRV-Cross_MINOR_VERSION "${_ver} >> 8 & 0xFF")
        set(SPIRV-Cross_VERSION
            "${SPIRV-Cross_MAJOR_VERSION}.${SPIRV-Cross_MINOR_VERSION}"
        )
        unset(_ver_line)
        unset(_ver)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SPIRV-Cross
    REQUIRED_VARS
        SPIRV-Cross_INCLUDE_DIR
        SPIRV-Cross_core_LIBRARY
        SPIRV-Cross_glsl_LIBRARY
    VERSION_VAR SPIRV-Cross_VERSION
)

if(SPIRV-Cross_FOUND AND NOT TARGET spirv-cross-core)
    add_library(spirv-cross-core UNKNOWN IMPORTED)
    set_target_properties(spirv-cross-core PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SPIRV-Cross_INCLUDE_DIR}"
        IMPORTED_LOCATION "${SPIRV-Cross_core_LIBRARY}"
    )
endif()

if(SPIRV-Cross_FOUND AND NOT TARGET spirv-cross-glsl)
    add_library(spirv-cross-glsl UNKNOWN IMPORTED)
    set_target_properties(spirv-cross-glsl PROPERTIES
        IMPORTED_LOCATION "${SPIRV-Cross_glsl_LIBRARY}"
    )
    target_link_libraries(spirv-cross-glsl INTERFACE spirv-cross-core)
endif()

mark_as_advanced(
    SPIRV-Cross_INCLUDE_DIR
    SPIRV-Cross_core_LIBRARY
    SPIRV-Cross_glsl_LIBRARY
)
