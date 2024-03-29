CMAKE_MINIMUM_REQUIRED (VERSION 3.1)
CMAKE_POLICY (VERSION 3.1)

# Tell CMake to use headers / frameworks from SDK inside XCode instead of
# the ones found on the system (for weak linking).  Ignored on non-Apple
SET(CMAKE_OSX_SYSROOT macosx)

# Top-level CMakeLists for Couchbase Lite Core
PROJECT (couchbase-lite-cli-build)

# Provide reasonable default for CMAKE_INSTALL_PREFIX
IF (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE STRING
      "The install location" FORCE)
  LIST(APPEND CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}")
ENDIF (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Download additional cbdeps packages
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  INCLUDE (couchbase-mobile-tools/vendor/couchbase-lite-core/jenkins/CBDeps.cmake)

  # Install cbdeps packages using cbdep tool
  CBDEP_INSTALL (PACKAGE icu4c VERSION 71.1-2)
  FILE (COPY "${CBDEP_icu4c_DIR}/lib" DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()

ADD_SUBDIRECTORY (couchbase-mobile-tools/cblite)
