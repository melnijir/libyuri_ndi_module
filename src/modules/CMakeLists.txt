add_subdirectory(ndi_blank)
add_subdirectory(ndi_combine)
add_subdirectory(ndi_scale)
add_subdirectory(ndi_net)
find_package(OpenSSL)
IF(OpenSSL_FOUND)
    add_subdirectory(ndi)
ENDIF()

