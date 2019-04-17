add_subdirectory(n1ql-to-json)

add_executable(cblite
                ${CBLITE_SRC}
                ${LITECP_SRC}
                ${LINENOISE_SRC}
                Tool.cc
                ArgumentTokenizer.cc)
 
 target_include_directories(
     cblite PRIVATE
     ${PROJECT_SOURCE_DIR}
     litecp
     vendor/linenoise-ng/include/
     ${LITECORE}C/
     ${LITECORE}C/include/
     ${LITECORE}LiteCore/Support/
     ${LITECORE}Replicator/                   # for CivetWebSocket.hh
     ${LITECORE}vendor/fleece/API/
     ${LITECORE}vendor/fleece/Fleece/Support/ # PlatformCompat.hh
     ${PROJECT_BINARY_DIR}/generated_headers/
 )
 
 target_compile_definitions(cblite PRIVATE -DCMAKE)
 target_link_libraries(cblite
    ${LITECORE_LIBRARIES_PRIVATE}
    LiteCoreREST_Static
    n1ql_to_json_static
    ${PLATFORM_LIBS})
 if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(cblite ${ICU4C_COMMON} ${ICU4C_I18N})
 endif()
