function(common_setup)
    set(_build_version "unknown")
    set(_litecore_build_version "unknown")
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY "${local_dir}"
            OUTPUT_VARIABLE _build_version
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY "${LITECORE}"
            OUTPUT_VARIABLE _litecore_build_version
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
        set(TOOLS_VERSION_STRING "${TOOLS_VERSION} (${TOOLS_BLD_NUM}; ${_build_version}) with LiteCore ${_litecore_build_version}")
    else()
        message("No environment variables found, using git commit only")
        set(TOOLS_VERSION_MAJOR 0)
        set(TOOLS_VERSION_MINOR 0)
        set(TOOLS_VERSION_PATCH 0)
        set(TOOLS_VERSION_STRING "0.0.0 (${_build_version}) with LiteCore ${_litecore_build_version}")
    endif()

    configure_file(
        ${PROJECT_SOURCE_DIR}/../config.h.in
        ${CMAKE_BINARY_DIR}/generated_headers/config.h
    )

    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
    set(CMAKE_C_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_C_STANDARD 11 PARENT_SCOPE)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Optimizer bug...causes infinite loops on basic for loops
        add_definitions(-fno-aggressive-loop-optimizations)
    endif()

    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
        # Enable relative RPATHs for installed bits
        set (CMAKE_INSTALL_RPATH "\$ORIGIN/../lib" PARENT_SCOPE)
    endif()
endfunction(common_setup)

function(fix_cpp_runtime)
    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
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
endfunction()

function(get_platform_libs OUTPUT_VAR)
    if (APPLE)
        set(${OUTPUT_VAR}
            z
            "-framework CoreFoundation"
            "-framework Foundation"
            "-framework Security"
            "-framework SystemConfiguration"
            PARENT_SCOPE)
    elseif(MSVC)
        set(${OUTPUT_VAR} zlibstatic  PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} pthread z dl ${LIBCXX_LIB} ${LIBCXXABI_LIB}  PARENT_SCOPE)
    endif()
endfunction(get_platform_libs OUTPUT_VAR)
