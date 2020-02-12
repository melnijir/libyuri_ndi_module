# Macro to install yuri modules

macro(YURI_INSTALL_MODULE MODULE)
	install(TARGETS ${MODULE} LIBRARY DESTINATION ${TARGET_MODULE_PATH})
	IF (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 8 AND CMAKE_PATCH_VERSION LESS 4)
		IF(UNIX)
			SET(MODULE_NAME "${CMAKE_SHARED_MODULE_PREFIX}${MODULE}${CMAKE_SHARED_MODULE_SUFFIX}")
			SET(MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}")
			SET(MODULE_OUT_PATH "${MODULE_DIR}/${MODULE_NAME}")
			#MESSAGE("${MODULE_PATH} -> ${MODULE_OUT_PATH}")
			add_custom_command(TARGET ${MODULE} POST_BUILD
	    		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		    	"${MODULE_PATH}" "${MODULE_OUT_PATH}"
	    	)
		ELSE()
			MESSAGE("Current version of CMAKE is too old and I don't know, how to install module '${MODULE}' at this platform. Please mode it manually to bin/modules directory")
		ENDIF()
	ELSE()
		add_custom_command(TARGET ${MODULE} POST_BUILD
	    	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	    	"$<TARGET_FILE_DIR:${MODULE}>/$<TARGET_FILE_NAME:${MODULE}>"
	    	"${MODULE_DIR}/$<TARGET_FILE_NAME:${MODULE}>"
	    )	
	ENDIF()
endmacro(YURI_INSTALL_MODULE MODULE)