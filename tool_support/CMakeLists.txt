include(${CMAKE_CURRENT_LIST_DIR}/../cmake/common_setup.cmake)
common_setup()

add_library( tool_support STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../tool_support/Tool.cc
    ${CMAKE_CURRENT_LIST_DIR}/../tool_support/LiteCoreTool.cc
    ${CMAKE_CURRENT_LIST_DIR}/../tool_support/ArgumentTokenizer.cc
    ${CMAKE_CURRENT_LIST_DIR}/../vendor/linenoise-mob/linenoise.c
    ${CMAKE_CURRENT_LIST_DIR}/../vendor/linenoise-mob/utf8.c
)

if(NOT DEFINED LITECORE)
    set(LITECORE ${CMAKE_CURRENT_LIST_DIR}/../vendor/couchbase-lite-core)
endif()

target_include_directories( tool_support PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/
    ${CMAKE_CURRENT_LIST_DIR}/../vendor/linenoise-mob/
    ${CMAKE_BINARY_DIR}/generated_headers/

    ${LITECORE}/C/
    ${LITECORE}/C/include/
    ${LITECORE}/C/Cpp_include/
    ${LITECORE}/LiteCore/Support/
    ${LITECORE}/Networking/
    ${LITECORE}/Networking/HTTP
    ${LITECORE}/Networking/WebSockets
    ${LITECORE}/Replicator
    ${LITECORE}/REST/
    ${LITECORE}/vendor/fleece/API/
    ${LITECORE}/vendor/fleece/Fleece/Support/ # PlatformCompat.hh
)

target_compile_definitions(tool_support PRIVATE -DCMAKE)
if(BUILD_ENTERPRISE)
    target_compile_definitions(tool_support PRIVATE -DCOUCHBASE_ENTERPRISE)
endif()

target_link_libraries( tool_support PUBLIC
    ${LITECORE_LIBRARIES_PRIVATE}
)