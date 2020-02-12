# Set name of the module
SET (MODULE ndi)

# Set all source files module uses
SET (SRC NDIInput.cpp
		 NDIInput.h
		 NDIOutput.cpp
		 NDIOutput.h
		 Licence.cpp
		 Licence.h
		 ../common/utils.cpp
		 ../common/utils.h
		 register.cpp)

# You shouldn't need to edit anything below this line
include_directories(${NDI_INCLUDE_DIRS}) 
add_library(${MODULE} MODULE ${SRC})
target_link_libraries(${MODULE} ${YURI_LIBRARIES} ${NDI_LIBRARIES} ${OPENSSL_LIBRARIES})

YURI_INSTALL_MODULE(${MODULE})