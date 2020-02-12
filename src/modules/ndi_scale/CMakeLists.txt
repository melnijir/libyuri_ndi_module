# Set name of the module
SET (MODULE scale)

# Set all source files module uses
SET (SRC Scale.cpp
		 Scale.h)


 
# You shouldn't need to edit anything below this line 
add_library(${MODULE} MODULE ${SRC})
target_link_libraries(${MODULE} ${YURI_LIBRARIES})

YURI_INSTALL_MODULE(${MODULE})
