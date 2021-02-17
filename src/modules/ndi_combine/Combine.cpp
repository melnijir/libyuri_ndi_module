/*!
 * @file 		Combine.cpp
 * @author 		Zdenek Travnicek
 * @date 		11.2.2013
 * @date		16.2.2013
 * @copyright	Institute of Intermedia, CTU in Prague, 2013
 * 				Distributed under modified BSD Licence, details in file doc/LICENSE
 *
 */

#include "Combine.h"
#include "yuri/core/Module.h"
#include "yuri/core/frame/RawVideoFrame.h"
#include "yuri/core/frame/raw_frame_types.h"
#include "yuri/core/frame/raw_frame_params.h"
namespace yuri {
namespace ndi_combine {

IOTHREAD_GENERATOR(Combine)

MODULE_REGISTRATION_BEGIN("ndi_combine")
	REGISTER_IOTHREAD("ndi_combine",Combine)
MODULE_REGISTRATION_END()

using namespace yuri::log;

core::Parameters Combine::configure()
{
	core::Parameters p = base_type::configure();
	p.set_description("NDI Combine");
	p["x"]["Width of the grid"]=2;
	p["y"]["Height of the grid"]=2;
	p["fps"]["Maximal framerate"]=30;
	return p;
}


Combine::Combine(log::Log &log_, core::pwThreadBase parent, const core::Parameters &parameters):
base_type(log_,parent,1,1,std::string("ndi_combine")),x_(2),y_(2),fps_(30)
{
	IOTHREAD_INIT(parameters)
	if (x_<1 || y_<1) throw exception::InitializationFailed("Wrong size of the grid");
	resize(x_ * y_,1);
	next_time_ = timestamp_t{};
}

Combine::~Combine() noexcept
{
}

std::vector<core::pFrame> Combine::do_single_step(std::vector<core::pFrame> framesx)
{
	const auto curtime = timestamp_t{};
	if (next_time_ > curtime) {
		sleep(std::min(get_latency(), next_time_ - curtime));
		return {};
	}

	const format_t format = framesx[0]->get_format();
	const auto& fi = core::raw_format::get_format_info(format);
	const size_t frames_no = framesx.size();

	if (fi.planes.size() != 1) {
		log[log::warning] << "Planar formats not supported";
		return {};
	}

	std::vector<core::pRawVideoFrame> frames;
	for (auto& x: framesx) {
		auto f = std::dynamic_pointer_cast<core::RawVideoFrame>(x);
		if (!f) {
			log[log::warning] << "Received non-raw frame.";
			return {};
		}
		frames.push_back(f);
	}

	size_t bpp = fi.planes[0].bit_depth.first/fi.planes[0].bit_depth.second;
	// const resolution_t resolution = frames[0]->get_resolution();
	const size_t width  = frames[0]->get_width();
	const size_t height = frames[0]->get_height();
	for (size_t i=1;i<frames_no;++i) {
		if (frames[i]->get_format() != format) {
			log[log::warning] << "Wrong format for frame in pipe " << i;
			//frames[i].reset();
			return {};
		}
	}
	core::pRawVideoFrame output = core::RawVideoFrame::create_empty(format,{width*x_, height*y_}, true);
	uint8_t* out = PLANE_RAW_DATA(output,0);
	size_t sub_line_width=bpp*width/8;
	size_t line_width = sub_line_width*x_;
	size_t idx = 0;
	for (size_t idx_y=0;idx_y<y_;++idx_y) {
		for (size_t idx_x=0;idx_x<x_;++idx_x) {
			const uint8_t* raw_src = PLANE_RAW_DATA(frames[idx],0);
			size_t line_skip = 0;
			size_t sub_line_width_curr = bpp*frames[idx]->get_width()/8;
			if (sub_line_width_curr > sub_line_width) {
				line_skip = sub_line_width_curr-sub_line_width;
				sub_line_width_curr = sub_line_width;
			}
			size_t height_curr = height < frames[idx]->get_height() ? height : frames[idx]->get_height();
			for (size_t line=0;line<height_curr;++line) {
				std::copy(raw_src+line*sub_line_width_curr+line*line_skip,
						raw_src+(line+1)*sub_line_width_curr+line*line_skip,
						out+(idx_y*height+line)*line_width+idx_x*sub_line_width);
			}
			idx++;
		}
	}
	next_time_ = next_time_ + (1_s / fps_);
	return {output};
}

bool Combine::set_param(const core::Parameter& param)
{
	if (assign_parameters(param)
			(x_, "x")
			(y_, "y")
			(fps_, "fps"))
		return true;
	return base_type::set_param(param);
}

} /* namespace ndi_combine */
} /* namespace yuri */
