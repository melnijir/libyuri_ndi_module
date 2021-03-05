# Set name of the module
SET (MODULE ndi_blank)

# Set all source files module uses
SET (SRC BlankGenerator.cpp
		 BlankGenerator.h)



# You shouldn't need to edit anything below this line 
add_library(${MODULE} MODULE ${SRC})
target_link_libraries(${MODULE} ${YURI_LIBRARIES})

YURI_INSTALL_MODULE(${MODULE})