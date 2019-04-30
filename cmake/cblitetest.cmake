add_executable(cblitetest 
    ArgumentTokenizer.cc
    tests/tests_main.cc
    tests/TokenizerTest.cc)

target_link_libraries(cblitetest
    ${PLATFORM_LIBS})
