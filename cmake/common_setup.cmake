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
        ${CMAKE_CURRENT_LIST_DIR}/../config.h.in
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
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            add_link_options(-fuse-ld=lld)
        endif()
    endif()
endfunction(common_setup)

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
