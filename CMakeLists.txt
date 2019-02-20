cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)
project (LiteCoreTools)

find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${local_dir}"
        OUTPUT_VARIABLE _build_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if((NOT "$ENV{VERSION}" STREQUAL "") AND (NOT "$ENV{BLD_NUM}" STREQUAL ""))
    set(TOOLS_VERSION "$ENV{VERSION}" CACHE INTERNAL "")
    set(TOOLS_BLD_NUM "$ENV{BLD_NUM}" CACHE INTERNAL "")
endif()

if((NOT "${TOOLS_VERSION}" STREQUAL "") AND (NOT "${TOOLS_BLD_NUM}" STREQUAL ""))
    message("Using provided values:  VERSION: ${TOOLS_VERSION}, BLD_NUM: ${TOOLS_BLD_NUM}")
    string(REPLACE "." ";" VERSION_LIST ${TOOLS_VERSION})
    list(GET VERSION_LIST 0 TOOLS_VERSION_MAJOR)
    list(GET VERSION_LIST 1 TOOLS_VERSION_MINOR)
    list(GET VERSION_LIST 2 TOOLS_VERSION_PATCH)
    if(GIT_FOUND)
        set(TOOLS_VERSION_STRING "${TOOLS_VERSION} (${TOOLS_BLD_NUM}; ${_build_version})")
    else()
        set(TOOLS_VERSION_STRING "${TOOLS_VERSION} (${TOOLS_BLD_NUM}; unknown)")
    endif()
else()
    message("No environment variables found, using git commit only")
    set(TOOLS_VERSION_MAJOR 0)
    set(TOOLS_VERSION_MINOR 0)
    set(TOOLS_VERSION_PATCH 0)
    if(GIT_FOUND)
        set(TOOLS_VERSION_STRING "0.0.0 (commit ${_build_version})")
    else()
        set(TOOLS_VERSION_STRING "0.0.0 (commit unknown)")
    endif()
endif()

configure_file(
    ${PROJECT_SOURCE_DIR}/config.h.in
    ${PROJECT_BINARY_DIR}/generated_headers/config.h
)

if(ANDROID)
    return()
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(LITECORE_DISABLE_ICU CACHE BOOL ON)
set(LITECORE vendor/couchbase-lite-core/)

### CBLITE

aux_source_directory("cblite" CBLITE_SRC)
aux_source_directory("litecp" LITECP_SRC)
aux_source_directory("vendor/linenoise-ng/src" LINENOISE_SRC)
set(LOGCAT_SRC
    "cbl-log/cbl-log.cc"
    "${LITECORE}LiteCore/Support/LogDecoder.cc"
    "${LITECORE}LiteCore/Support/LogEncoder.cc")

add_subdirectory(${LITECORE})
get_directory_property(LITECORE_LIBRARIES_PRIVATE DIRECTORY ${LITECORE} DEFINITION LITECORE_LIBRARIES_PRIVATE)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    # Enable relative RPATHs for installed bits
    set (CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")


    if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} is not supported for building!")
        endif()
        find_library(LIBCXX_LIB c++)
        if (NOT LIBCXX_LIB)
            message(FATAL_ERROR "libc++ not found")
        endif()
        message("Found libc++ at ${LIBCXX_LIB}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

        find_library(LIBCXXABI_LIB c++abi)
        if (NOT LIBCXXABI_LIB)
            message(FATAL_ERROR "libc++abi not found")
        endif()
        message("Found libc++abi at ${LIBCXXABI_LIB}")
        find_path(LIBCXX_INCLUDE c++/v1/string
            HINTS "${CMAKE_BINARY_DIR}/tlm/deps/libcxx.exploded"
            PATH_SUFFIXES include)
        if (NOT LIBCXX_INCLUDE)
            message(FATAL_ERROR "libc++ header files not found")
        endif()
        message("Using libc++ header files in ${LIBCXX_INCLUDE}")
        include_directories("${LIBCXX_INCLUDE}/c++/v1")
        if(NOT EXISTS "/usr/include/xlocale.h")
            include_directories("${LIBCXX_INCLUDE}/c++/v1/support/xlocale") # this fixed path is here to avoid compilation on Ubuntu 17.10 where xlocale.h is searched by some header(s) in libc++ as <xinclude.h> but not found from search path without this modification.  However, only do it if the original xlocale.h does not exist since this will get searched before /usr/include and override a valid file with an empty one.
        endif()
        include_directories("/usr/include/libcxxabi") # this fixed path is here to avoid Clang issue noted at http://lists.alioth.debian.org/pipermail/pkg-llvm-team/2015-September/005208.html
    endif()

    # libc++ is special - clang will introduce an implicit -lc++ when it is used.
    # That means we need to tell the linker the path to the directory containing
    # libc++.so rather than just linking the .so directly. This must be done
    # *before* the target declaration as it affects all subsequent targets.
    get_filename_component (LIBCXX_LIBDIR "${LIBCXX_LIB}" DIRECTORY)
    link_directories (${LIBCXX_LIBDIR})
endif()



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
    ${LITECORE}Replicator/			         # for CivetWebSocket.hh
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/fleece/Fleece/Support/ # PlatformCompat.hh
    ${PROJECT_BINARY_DIR}/generated_headers/
)

if (APPLE)
    set(PLATFORM_LIBS z
		 			"-framework CoreFoundation" "-framework Security")
elseif(MSVC)
    set(PLATFORM_LIBS zlibstatic)
else()
    set(PLATFORM_LIBS pthread z dl ${LIBCXX_LIB} ${LIBCXXABI_LIB})
endif()

target_link_libraries(cblite ${LITECORE_LIBRARIES_PRIVATE} LiteCoreREST_Static ${PLATFORM_LIBS})

### cbl-log

add_executable(cbl-log
               ${LOGCAT_SRC}
               ${LINENOISE_SRC}
               Tool.cc
               ArgumentTokenizer.cc)

target_include_directories(
    cbl-log PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${LITECORE}C/include/
    vendor/linenoise-ng/include/
    ${LITECORE}LiteCore/Support/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/fleece/Fleece/Support/ # PlatformCompat.hh
    ${PROJECT_BINARY_DIR}/generated_headers/
)
target_compile_definitions(cbl-log PRIVATE -DCBLTOOL_NO_C_API)
target_link_libraries(cbl-log FleeceStatic Support BLIPStatic ${PLATFORM_LIBS})

### TESTS

include_directories(${LITECORE}vendor/fleece/vendor/catch)
include_directories(${PROJECT_SOURCE_DIR})
add_executable(cblitetest tests/TokenizerTest.cc
	 					  ArgumentTokenizer.cc)

include_directories(${LITECORE}LiteCore/Support/
    ${LITECORE}LiteCore/Storage/
    ${LITECORE}vendor/fleece/Fleece/Support/
    ${LITECORE}vendor/fleece/API/
    ${LITECORE}vendor/BLIP-Cpp/src/util
    ${LITECORE}C/include/)
add_executable(cbl-logtest tests/TokenizerTest.cc
    ArgumentTokenizer.cc
    ${LITECORE}LiteCore/tests/LogEncoderTest.cc)

if(APPLE)
    set(CRYPTO_LIB "-framework Security")
else()
    set(CRYPTO_LIB "mbedcrypto")
endif()

target_link_libraries(cbl-logtest LiteCoreStatic FleeceStatic Support
    SQLite3_UnicodeSN BLIPStatic CivetWeb ${CRYPTO_LIB} ${PLATFORM_LIBS})

### Install

install (TARGETS cbl-log cbl-logtest
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib)
