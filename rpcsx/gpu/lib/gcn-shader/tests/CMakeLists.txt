add_executable(gcn-instruction-decode-test gcn-instruction-decode.cpp)
target_link_libraries(gcn-instruction-decode-test PRIVATE gcn-shader)

add_test(NAME gcn-instruction-decode COMMAND gcn-instruction-decode-test)
