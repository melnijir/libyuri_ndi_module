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

std::map<NDIlib_FourCC_type_e, yuri::format_t> pixel_format_map = {
	{NDIlib_FourCC_type_I420,	yuv420p},
	{NDIlib_FourCC_type_NV12,	yuv420p},
	{NDIlib_FourCC_type_YV12,	yuv420p},
	
	{NDIlib_FourCC_type_UYVY,	uyvy422},
	{NDIlib_FourCC_type_UYVA,	uyvy422},

	{NDIlib_FourCC_type_BGRA,	bgra32},
	{NDIlib_FourCC_type_BGRX,	bgra32},
	{NDIlib_FourCC_type_RGBA,	rgba32},
	{NDIlib_FourCC_type_RGBX,	rgba32},
};

yuri::format_t ndi_format_to_yuri (NDIlib_FourCC_type_e fmt) {
	auto it = pixel_format_map.find(fmt);
	if (it == pixel_format_map.end()) throw yuri::exception::Exception("No Yuri format found.");
	return it->second;
}


NDIlib_FourCC_type_e yuri_format_to_ndi(yuri::format_t fmt) {
	for (auto f: pixel_format_map) {
		if (f.second == fmt) return f.first;
	}
	throw yuri::exception::Exception("No NDI format found.");
}