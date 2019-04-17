add_executable(cblitetest 
    ArgumentTokenizer.cc
    tests/tests_main.cc
    tests/TokenizerTest.cc
    tests/N1QLParserTests.cc)

target_link_libraries(cblitetest
    n1ql_to_json
    ${PLATFORM_LIBS})

if(WIN32)
    set(FilesToCopy ${PROJECT_BINARY_DIR}/n1ql-to-json/\$\(Configuration\)/n1ql_to_json)
    add_custom_command(
        TARGET cblitetest POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -DFilesToCopy="${FilesToCopy}"
        -DDestinationDirectory=${PROJECT_BINARY_DIR}/\$\(Configuration\)
        -P ${PROJECT_SOURCE_DIR}/vendor/couchbase-lite-core/MSVC/copy_artifacts.cmake
    )
endif()