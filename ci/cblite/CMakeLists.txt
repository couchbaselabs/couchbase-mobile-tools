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

# Configure tlm the way it wants to be configured
SET (CB_DOWNLOAD_DEPS 1)
SET (CB_DOWNLOAD_DEPS_DEFAULT_MANIFEST
  "${PROJECT_SOURCE_DIR}/cbbuild/scripts/jenkins/lite-core/manifest.cmake")
LIST (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/tlm/cmake/Modules")
ADD_SUBDIRECTORY (tlm/deps)

ADD_SUBDIRECTORY (couchbase-mobile-tools/cblite)
