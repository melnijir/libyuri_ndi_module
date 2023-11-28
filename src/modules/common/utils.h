#ifndef _NDI_UTILS_H_
#define _NDI_UTILS_H_

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <exception>

#include "yuri/exception/Exception.h"
#include "yuri/core/utils/new_types.h"
#include "yuri/core/frame/raw_frame_types.h"

#include <Processing.NDI.Lib.h>

typedef unsigned char byte;

const NDIlib_v5* load_ndi_library(std::string ndi_path = "");
yuri::format_t ndi_format_to_yuri (NDIlib_FourCC_type_e fmt);
NDIlib_FourCC_type_e yuri_format_to_ndi(yuri::format_t fmt);

#endif