set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)

add_subdirectory(crypto)

if(NOT TARGET xbyak::xbyak)
  add_subdirectory(xbyak)
endif()
