cmake_minimum_required(VERSION 3.0)
PROJECT(yuri2_ndi VERSION 0.1.5 LANGUAGES CXX C)

OPTION (BUILD_SHARED_LIBS "Build shared libraries." ON)

#################################################################
# Include required CMake modules
#################################################################
INCLUDE(FindPkgConfig)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/")
include(GetGitRevisionDescription)
include(yuri_install)

#################################################################
# Find yuri
#################################################################
find_package(YURI REQUIRED)
find_package(NDI REQUIRED)

#################################################################
# Get the version of this build
#################################################################
#git_describe(YURI_GIT_VERSION)


#################################################################
# Add strict warning if building under linux
#################################################################
IF (UNIX)
add_definitions("-Wall -pedantic -Wextra -std=c++11 -DYURI_USE_CXX11")
ENDIF ()

#################################################################
# Setup names for the core library and helpers
#################################################################
SET(LIBNAME yuri_core)

#################################################################
# Set prefix for modules
#################################################################
SET (CMAKE_SHARED_MODULE_PREFIX "yuri2.8_module_")

#################################################################
# Setup paths
#################################################################
include_directories( ${CMAKE_SOURCE_DIR}/src ${NDI_INCLUDE_DIR} ${YURI_INCLUDE_DIR})
link_directories(  ${YURI_LIB_DIR} ${NDI_LIB_DIR})
SET (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET (MODULE_DIR ${EXECUTABLE_OUTPUT_PATH}/modules)
SET (TARGET_MODULE_PATH lib/yuri2/)


#################################################################
# And finally submerge into source directories
#################################################################
add_subdirectory(src)


SET(CPACK_GENERATOR "DEB")
SET(CPACK_PACKAGE_VENDOR "Melnikoff")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kafe")

SET(CPACK_DEBIAN_PACKAGE_DEPENDS "yuri2, ndi, avahi-daemon")
# Explicitely setting verson for comatibility with old cmake
SET(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
INCLUDE(CPack)
