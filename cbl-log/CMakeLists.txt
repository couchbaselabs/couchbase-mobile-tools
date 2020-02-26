cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (cbl-log)
include("../cmake/common_setup.cmake")

set(LITECORE ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/)
set(LITECORE_DISABLE_ICU ON CACHE BOOL "" FORCE)
common_setup()
get_platform_libs(PLATFORM_LIBS)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

aux_source_directory("../vendor/linenoise-ng/src" LINENOISE_SRC)
set(
    LOGCAT_SRC
    "../cbl-log/cbl-log.cc"
    "${LITECORE}LiteCore/Support/LogDecoder.cc"
)

add_subdirectory(${LITECORE} LiteCore)

add_executable(
    cbl-log
    ${LOGCAT_SRC}
    ${LINENOISE_SRC}
    ../Tool.cc
    ../ArgumentTokenizer.cc
)

target_include_directories(
    cbl-log PRIVATE
    ${PROJECT_SOURCE_DIR}/..
    ${LITECORE}C/include/
    ${PROJECT_SOURCE_DIR}/../vendor/linenoise-ng/include/
    ${LITECORE}LiteCore/Support/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/fleece/Fleece/Support/ # PlatformCompat.hh
    ${LITECORE}vendor/BLIP-Cpp/src/util
    ${PROJECT_BINARY_DIR}/generated_headers/
)

fix_cpp_runtime()
target_compile_definitions(cbl-log PRIVATE -DCBLTOOL_NO_C_API -DCMAKE)
target_link_libraries(cbl-log Support FleeceStatic BLIPStatic ${PLATFORM_LIBS})
if(UNIX AND NOT APPLE AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(cbl-log PRIVATE "-stdlib=libc++")
endif()

add_executable(
    cbl-logtest
    ../tests/TokenizerTest.cc
    ../ArgumentTokenizer.cc
    ../tests/tests_main.cc
    ${LITECORE}LiteCore/tests/LogEncoderTest.cc
    ${LITECORE}LiteCore/Storage/UnicodeCollator_Stub.cc
)

target_include_directories(
    cbl-logtest PRIVATE
    ${LITECORE}vendor/fleece/vendor/catch
    ${LITECORE}LiteCore/Support/
    ${LITECORE}LiteCore/Storage/
    ${LITECORE}vendor/fleece/Fleece/Support/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/BLIP-Cpp/src/util
    ${LITECORE}C/include/
    ${PROJECT_SOURCE_DIR}/..
)

if(UNIX AND NOT APPLE AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(cbl-logtest PRIVATE "-stdlib=libc++")
endif()

if(APPLE)
    set(CRYPTO_LIB "-framework Security")
else()
    set(CRYPTO_LIB "mbedcrypto")
endif()

target_link_libraries(
    cbl-logtest PRIVATE
    LiteCoreStatic
    FleeceStatic
    Support
    SQLite3_UnicodeSN
    BLIPStatic
    CivetWeb
    ${CRYPTO_LIB}
    ${PLATFORM_LIBS}
)

install (
    TARGETS cbl-log cbl-logtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
