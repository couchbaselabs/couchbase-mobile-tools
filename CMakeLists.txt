cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (LiteCoreTools)

if(ANDROID)
    return()
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(TOP vendor/couchbase-lite-core/)

include_directories(${PROJECT_SOURCE_DIR}
                    ${PROJECT_SOURCE_DIR}/litecp
                    ${TOP}C
                    ${TOP}C/include/
                    ${TOP}LiteCore/Support/
                    ${TOP}Replicator/
                    ${TOP}vendor/fleece/Fleece/
                    ${TOP}vendor/linenoise-ng/include/)

### CBLITE

aux_source_directory("cblite" CBLITE_SRC)
aux_source_directory("litecp" LITECP_SRC)
aux_source_directory("vendor/linenoise-ng/src" LINENOISE_SRC)

add_subdirectory(${TOP})
get_directory_property(LITECORE_LIBRARIES_PRIVATE DIRECTORY ${TOP} DEFINITION LITECORE_LIBRARIES_PRIVATE)
add_executable(cblite
               ${CBLITE_SRC}
               ${LITECP_SRC}
               ${LINENOISE_SRC}
               Tool.cc
               ArgumentTokenizer.cc)

target_link_libraries(cblite ${LITECORE_LIBRARIES_PRIVATE} LiteCoreREST_Static)

if (APPLE)
    target_link_libraries(cblite
      z
      "-framework CoreFoundation"
      "-framework Security")
elseif(MSVC)
    target_link_libraries(cblite zlibstatic)
else()
    target_link_libraries(cblite pthread z dl
                          ${ICU4C_COMMON} ${ICU4C_I18N} ${LIBCXX_LIB} ${LIBCXXABI_LIB})
endif()

include_directories(${TOP}vendor/fleece/vendor/catch)
add_executable(cblitetest tests/TokenizerTest.cc ArgumentTokenizer.cc)
