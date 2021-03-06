#include "utils.h"

using namespace yuri::core::raw_format;

constexpr char strmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', ':', ';', '<', '=', '>', '?'};

std::string bin_to_str(std::vector<byte> data)
{
  std::string text(data.size() * 2, ' ');
  for (unsigned int i = 0; i < data.size(); ++i) {
    text[2 * i]     = strmap[(data[i] & 0xF0) >> 4];
    text[2 * i + 1] = strmap[data[i] & 0x0F];
  }
  return text;
}

std::vector<byte> str_to_bin(std::string text) {
    std::vector<byte> data(text.length() / 2);
    for (unsigned int i = 0; i < text.length() / 2; ++i) {
        data[i] = ((text[2 * i] - 48) << 4) | (text[2 * i + 1] - 48);
    }
    return data;
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