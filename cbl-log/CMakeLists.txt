cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (cbl-log)
include("../cmake/common_setup.cmake")

set(LITECORE ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/)
set(LITECORE_DISABLE_ICU ON CACHE BOOL "" FORCE)
set(LITECORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LITECORE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LITECORE_BUILD_STATIC OFF CACHE BOOL "" FORCE)
common_setup()
get_platform_libs(PLATFORM_LIBS)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

set(
    LOGCAT_SRC
    ../cbl-log/cbl-log.cc
    ${LITECORE}LiteCore/Logging/LogDecoder.cc
    ${LITECORE}LiteCore/Support/StringUtil.cc
)

add_subdirectory(${LITECORE} LiteCore)

add_executable(
    cbl-log
    ${LOGCAT_SRC}
)

target_include_directories(
    cbl-log PRIVATE
    ${PROJECT_SOURCE_DIR}/../tool_support
    ${LITECORE}C/include/
    ${LITECORE}C/
    ${PROJECT_SOURCE_DIR}/../vendor/linenoise-mob/
    ${LITECORE}LiteCore/Support/
    ${LITECORE}LiteCore/Logging/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/fleece/Fleece/Support/
    ${LITECORE}vendor/fleece/vendor/date/include/
    ${CMAKE_BINARY_DIR}/generated_headers/
)

if(WIN32)
    target_include_directories(
        cbl-log PRIVATE
        ${LITECORE}MSVC
    )
endif()

target_compile_definitions(
    cbl-log PRIVATE
    -DCMAKE
    -DHAS_UNCAUGHT_EXCEPTIONS
)

find_package(Threads REQUIRED)
target_link_libraries(cbl-log PRIVATE tool_support FleeceObjects Threads::Threads)

add_executable(
    cbl-logtest
    ../tests/TokenizerTest.cc
    ../tests/tests_main.cc
    ${LITECORE}LiteCore/tests/LogEncoderTest.cc
    ${LITECORE}LiteCore/Storage/UnicodeCollator_Stub.cc
    ${LITECORE}vendor/fleece/vendor/catch/catch_amalgamated.cpp
    ${LITECORE}vendor/fleece/vendor/catch/CaseListReporter.cc
)

target_include_directories(
    cbl-logtest PRIVATE
    ${LITECORE}vendor/fleece/vendor/catch
    ${LITECORE}LiteCore/Support/
    ${LITECORE}LiteCore/Storage/
    ${LITECORE}LiteCore/Query/
    ${LITECORE}C/Cpp_include/
    ${LITECORE}vendor/fleece/Fleece/Support/
    ${LITECORE}vendor/fleece/Fleece/Core/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/BLIP-Cpp/src/util
    ${LITECORE}vendor/vector_search
    ${LITECORE}C/include/
    ${LITECORE}C/
    ${LITECORE}LiteCore/tests
    ${LITECORE}tool_support/
    ${LITECORE}vendor/vector_search
    ${PROJECT_SOURCE_DIR}/../tool_support
)

if(APPLE)
    set(CRYPTO_LIB "-framework Security" mbedcrypto)
else()
    set(CRYPTO_LIB mbedcrypto mbedx509)
endif()

target_link_libraries(
    cbl-logtest PRIVATE
    LiteCoreObjects
    FleeceObjects
    BLIPObjects
    LiteCoreREST_Objects
    LiteCoreWebSocket
    tool_support
)

install (
    TARGETS cbl-log cbl-logtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
