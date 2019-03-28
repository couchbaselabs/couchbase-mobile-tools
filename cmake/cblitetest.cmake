add_executable(cblitetest tests/TokenizerTest.cc
                           ArgumentTokenizer.cc)
target_link_libraries(cblitetest ${PLATFORM_LIBS})