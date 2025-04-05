cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
project(iso-tool)

add_executable(iso-tool
    iso-tool.cpp
    ../../Utilities/StrFmt.cpp
    ../../Utilities/File.cpp
    ../dev/iso.cpp
    ../util/atomic.cpp
    ../../Utilities/mutex.cpp
    # ../util/dyn_lib.cpp
    # ../util/logs.cpp
)

target_include_directories(iso-tool PUBLIC ../../ .. ../../3rdparty/json/include)
target_compile_definitions(iso-tool PUBLIC WIN32_LEAN_AND_MEAN _UNICODE UNICODE)
