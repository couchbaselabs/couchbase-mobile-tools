cmake_minimum_required (VERSION 3.5)
cmake_policy(VERSION 3.5)
project (cblite)
include("../cmake/common_setup.cmake")

set(LITECORE ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/)
set(LITECORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LITECORE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LITECORE_BUILD_STATIC OFF CACHE BOOL "" FORCE)

option(BUILD_ENTERPRISE "Set whether or not to build enterprise edition" OFF)

common_setup()
get_platform_libs(PLATFORM_LIBS)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

add_subdirectory(${LITECORE} LiteCore)
get_directory_property(LITECORE_LIBRARIES_PRIVATE DIRECTORY ${LITECORE} DEFINITION LITECORE_LIBRARIES_PRIVATE)

fix_cpp_runtime()


# #### CBLITE


aux_source_directory("../cblite" CBLITE_SRC)

add_executable( cblite
    ${CBLITE_SRC}
    llm/Bedrock.cc
    llm/Gemini.cc
    llm/LLMProvider.cc
    llm/OpenAI.cc
    ../litecp/DBEndpoint.cc
    ../litecp/DirEndpoint.cc
    ../litecp/Endpoint.cc
    ../litecp/JSONEndpoint.cc
    ../litecp/RemoteEndpoint.cc
)

target_include_directories( cblite PRIVATE
    ${PROJECT_SOURCE_DIR}/../litecp
    ${PROJECT_SOURCE_DIR}/llm
    ${PROJECT_SOURCE_DIR}/
    ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/Networking
    ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/Networking/HTTP
    ${PROJECT_SOURCE_DIR}/../vendor/couchbase-lite-core/REST
)

target_compile_definitions(cblite PRIVATE -DCMAKE)
if(BUILD_ENTERPRISE)
    target_compile_definitions(cblite PRIVATE -DCOUCHBASE_ENTERPRISE)
endif()

target_link_libraries( cblite PRIVATE
    tool_support
    LiteCoreObjects
    LiteCoreREST_Static
    LiteCoreWebSocket
    ${LITECORE_LIBRARIES_PRIVATE}
)


#### TESTS


add_executable( cblitetest
    ../tests/tests_main.cc
    ../tests/TokenizerTest.cc
    ${LITECORE}vendor/fleece/vendor/catch/catch_amalgamated.cpp
    ${LITECORE}vendor/fleece/vendor/catch/CaseListReporter.cc
)

target_include_directories( cblitetest PRIVATE
    ${LITECORE}vendor/fleece/vendor/catch
)

target_link_libraries( cblitetest PRIVATE
    tool_support
    ${LITECORE_LIBRARIES_PRIVATE}
)

target_compile_definitions( cblitetest PRIVATE
    -DNO_TEMP_DIR
    -DNO_WAIT_UNTIL
)

install (
    TARGETS cblite cblitetest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
