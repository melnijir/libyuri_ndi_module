# - Find the yuri API includes and library
#=============================================================================
# Copyright 2013 Zdenek Travnicek, Institute of Intermedia (www.iim.cz)
#
# based on FindPNG.cmake from cmake distribution 
# Copyright 2002-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)
#  To override automatic detection, please specify environmental variables:
#  YURI_INCLUDE  	pointing to directory with yuri includes
#  YURI_LIBDIR 		pointing to directory with yuri libraries


SET(YURI_INC $ENV{YURI_INCLUDE})
SET(YURI_LIBDIR $ENV{YURI_LIBDIR})

find_path(YURI_YURI_INCLUDE_DIR yuri/core/Module.h PATHS
  ${YURI_INC}
  /usr/local/include/
  /usr/include    
 )

  
find_library(YURI_YURI_LIBRARY yuri2.8_core PATH 
	${YURI_LIBDIR}
 	/usr/lib/
 	/usr/local/lib)
 
IF (YURI_YURI_LIBRARY AND YURI_YURI_INCLUDE_DIR)
	SET(YURI_INCLUDE_DIRS ${YURI_YURI_INCLUDE_DIR} )
	SET(YURI_INCLUDE_DIR ${YURI_YURI_INCLUDE_DIR} ) # for backward compatiblity
	SET(YURI_LIBRARY ${YURI_YURI_LIBRARY})
	SET(YURI_LIBRARIES ${YURI_YURI_LIBRARY})
	#if (YURI_YURI_INCLUDE_DIR AND EXISTS "${YURI_YURI_INCLUDE_DIR}/YURIAPIVersion.h")
	#	file(STRINGS "${YURI_YURI_INCLUDE_DIR}/YURIAPIVersion.h" YURI_version_str REGEX "^#define[ \t]+BLACKMAGIC_YURI_API_VERSION_STRING[ \t]+\".+\"")
	#	string(REGEX REPLACE "^#define[ \t]+BLACKMAGIC_YURI_API_VERSION_STRING[ \t]+\"([^\"]+)\".*" "\\1" YURI_VERSION_STRING "${YURI_version_str}")
	#	unset(YURI_version_str)
	#	#MESSAGE(${YURI_VERSION_STRING})
	#endif ()
ENDIF()      

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YURI  
                                  REQUIRED_VARS YURI_LIBRARY YURI_INCLUDE_DIR)
                                  #VERSION_VAR YURI_VERSION_STRING)


mark_as_advanced(YURI_YURI_INCLUDE_DIR YURI_YURI_LIBRARY )
