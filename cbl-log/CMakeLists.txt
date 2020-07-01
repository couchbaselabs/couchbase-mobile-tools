cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (cbl-log)
include("../cmake/common_setup.cmake")

set(LITECORE ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/)
set(LITECORE_DISABLE_ICU ON CACHE BOOL "" FORCE)
set(LITECORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
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
    "${LITECORE}LiteCore/Support/LogEncoder.cc"
    "${LITECORE}LiteCore/Support/StringUtil.cc"
    "${LITECORE}LiteCore/Support/LibC++Debug.cc"
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

if(WIN32)
    target_include_directories(
        cbl-log PRIVATE
        ${LITECORE}MSVC
    )
endif()

target_compile_definitions(cbl-log PRIVATE -DCBLTOOL_NO_C_API -DCMAKE)
target_link_libraries(cbl-log FleeceStatic BLIPStatic)

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
    ${LITECORE}LiteCore/Query/
    ${LITECORE}vendor/fleece/Fleece/Support/
    ${LITECORE}vendor/fleece/Fleece/Core/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/BLIP-Cpp/src/util
    ${LITECORE}C/include/
    ${PROJECT_SOURCE_DIR}/..
)

if(APPLE)
    set(CRYPTO_LIB "-framework Security" mbedcrypto)
else()
    set(CRYPTO_LIB mbedcrypto mbedx509)
endif()

target_link_libraries(
    cbl-logtest PRIVATE
    LiteCoreStatic
    FleeceStatic
)

install (
    TARGETS cbl-log cbl-logtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
