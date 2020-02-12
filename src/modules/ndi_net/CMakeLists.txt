# Set name of the module
SET (MODULE ndi_net_info)

# Set all source files module uses
SET (SRC NetInfo.cpp
		 NetInfo.h
		 ../common/utils.cpp
		 ../common/utils.h)


 
add_library(${MODULE} MODULE ${SRC})
target_link_libraries(${MODULE} ${YURI_LIBRARIES})

YURI_INSTALL_MODULE(${MODULE})
