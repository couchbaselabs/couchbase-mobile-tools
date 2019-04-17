add_executable(cblitetest 
    ArgumentTokenizer.cc
    tests/tests_main.cc
    tests/TokenizerTest.cc
    tests/N1QLParserTests.cc)
target_link_libraries(cblitetest
    n1ql_to_json
    ${PLATFORM_LIBS})
