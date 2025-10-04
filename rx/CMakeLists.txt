project(rx)

find_package(Git)

add_library(${PROJECT_NAME} OBJECT
    src/debug.cpp
    src/die.cpp
    src/hexdump.cpp
    src/mem.cpp
    src/Version.cpp
    src/FileLock.cpp
)

target_include_directories(${PROJECT_NAME}
PUBLIC
    include

PRIVATE
    include/${PROJECT_NAME}
)

target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)

if (Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} log --date=format:%Y%m%d --pretty=format:'%cd' -n 1 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE GIT_DATE)

    execute_process(COMMAND ${GIT_EXECUTABLE} log --pretty=format:'%h' -n 1
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_REV)

    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet --exit-code
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE GIT_DIRTY ERROR_QUIET)

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_BRANCH)

    string(SUBSTRING ${GIT_DATE} 1 8 GIT_DATE)
    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIRTY}" GIT_DIRTY)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
else()
    set(GIT_DATE 0)
    set(GIT_REV 0)
    set(GIT_DIRTY 1)
    set(GIT_BRANCH Unknown)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    RX_RAW_VERSION=${GIT_DATE}
    RX_GIT_REV=0x${GIT_REV}
    RX_GIT_DIRTY=${GIT_DIRTY}
    RX_GIT_BRANCH=${GIT_BRANCH}
)


file(WRITE "${CMAKE_SOURCE_DIR}/.rx.version" "${GIT_DATE}-${RX_TAG}-${RX_TAG_VERSION}-${GIT_BRANCH}-${GIT_REV}-${GIT_DIRTY}")
