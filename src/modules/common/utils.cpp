#include "utils.h"

#include <stdlib.h>
#include <dlfcn.h>

using namespace yuri::core::raw_format;

const NDIlib_v5* load_ndi_library(std::string ndi_path) {
	// Check if we know the path
	if (!ndi_path.length()) {
		auto env_ndi_path = std::getenv("NDI_PATH");
		ndi_path = env_ndi_path ? env_ndi_path : "";
	}
	// Load NDI library
	void* hNDIlib = dlopen(ndi_path.c_str(), RTLD_LOCAL | RTLD_LAZY);
	const NDIlib_v5* (*NDIlib_v5_load)(void) = nullptr;
	if (hNDIlib)
		*((void**)&NDIlib_v5_load) = dlsym(hNDIlib, "NDIlib_v5_load");
	if (!NDIlib_v5_load) {
		if (hNDIlib)
			dlclose(hNDIlib);
		throw yuri::exception::Exception("Could not load NDI library version 5 from location: \""+ndi_path+"\", please download the correct library version.");
	}
	return NDIlib_v5_load();
}

std::map<NDIlib_FourCC_type_e, yuri::format_t> ndi_to_yuri_pixmap = {
	{NDIlib_FourCC_type_I420,	yuv420p}, // Only this one is correct yuv420p, others wont work fine
	{NDIlib_FourCC_type_NV12,	yuv420p},
	{NDIlib_FourCC_type_YV12,	yuv420p},
	
	{NDIlib_FourCC_type_UYVY,	uyvy422},
	{NDIlib_FourCC_type_UYVA,	uyvy422}, // That is very strange, but this is a somehow planar format with uyvy422 and alpha separated

	{NDIlib_FourCC_type_BGRA,	bgra32},
	{NDIlib_FourCC_type_BGRX,	bgra32},
	{NDIlib_FourCC_type_RGBA,	rgba32},
	{NDIlib_FourCC_type_RGBX,	rgba32},
};

std::map<yuri::format_t, NDIlib_FourCC_type_e> yuri_to_ndi_pixmap = {
	{yuv420p, NDIlib_FourCC_type_I420},
	{uyvy422, NDIlib_FourCC_type_UYVY},
	{bgra32,  NDIlib_FourCC_type_BGRA},
	{rgba32,  NDIlib_FourCC_type_RGBA},
};

yuri::format_t ndi_format_to_yuri (NDIlib_FourCC_type_e fmt) {
	auto it = ndi_to_yuri_pixmap.find(fmt);
	if (it == ndi_to_yuri_pixmap.end()) throw yuri::exception::Exception("No Yuri format found.");
	return it->second;
}


NDIlib_FourCC_type_e yuri_format_to_ndi(yuri::format_t fmt) {
	auto it = yuri_to_ndi_pixmap.find(fmt);
	if (it == yuri_to_ndi_pixmap.end()) throw yuri::exception::Exception("No NDI format found.");
	return it->second;
}