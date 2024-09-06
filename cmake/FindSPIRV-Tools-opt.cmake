include(CheckCXXSymbolExists)
include(FindPackageHandleStandardArgs)

set(CMAKE_REQUIRED_LIBRARIES SPIRV-Tools-opt)
check_cxx_symbol_exists(spvtools::CreateBitCastCombinePass
    "spirv-tools/optimizer.hpp"
    HAVE_CREATE_BIT_CAST_COMBINE_PASS
)

if(HAVE_CREATE_BIT_CAST_COMBINE_PASS)
    find_package(SPIRV-Tools-opt QUIET CONFIG)
    find_package_handle_standard_args(SPIRV-Tools-opt CONFIG_MODE)
else()
    find_package_handle_standard_args(SPIRV-Tools-opt
        REQUIRED_VARS HAVE_CREATE_BIT_CAST_COMBINE_PASS
    )
endif()
