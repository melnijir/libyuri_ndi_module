/*
 * NDIOutput.cpp
 */

#include "NDIOutput.h"

#include "yuri/core/Module.h"
#include "yuri/core/frame/RawVideoFrame.h"
#include "yuri/core/frame/raw_frame_types.h"
#include "yuri/core/frame/raw_frame_params.h"
#include "yuri/core/frame/RawAudioFrame.h"
#include "yuri/core/frame/raw_audio_frame_types.h"
#include "yuri/core/frame/raw_audio_frame_params.h"

#include "yuri/core/utils.h"

#include "../common/utils.h"

#include <thread>
#include <cassert>
#include <cmath>
#include <vector>

namespace yuri {
namespace ndi {

namespace {

void draw_licence_rgba(core::pRawVideoFrame& frame, float percents) {
	auto& plane = PLANE_DATA(frame,0);
	const auto res = plane.get_resolution();
	auto iter = plane.begin();
	uint8_t t_max = std::numeric_limits<uint8_t>::max();
	for (dimension_t line = 0; line < (res.height / 10); ++line) {
		for (dimension_t col = 0; col < res.width; ++col) {
			if ((float)col/res.width <= percents) {
				*iter = *iter^t_max;
				iter++;
				*iter = *iter^t_max;
				iter++;
				*iter = *iter^t_max;
				iter++;
			} else {
				iter+=4;
			}
		}
	}
}

}

IOTHREAD_GENERATOR(NDIOutput)

core::Parameters NDIOutput::configure() {
	core::Parameters p = IOThread::configure();
	p["stream"]["Name of the stream to send."]="LiNDI";
	p["audio"]["Set to true if audio should be send."]=false;
	p["licence"]["Sets licence file location"]="";
	p["fps"]["Sets fps indicator sent in the stream"]="";
	return p;
}

NDIOutput::NDIOutput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters)
:core::IOThread(log_,parent,1,0,std::string("NDIOutput")),
event::BasicEventProducer(log),event::BasicEventConsumer(log),
stream_("LiNDI"),audio_enabled_(false),fps_(60),max_time_(15_minutes),licence_("") {
	IOTHREAD_INIT(parameters)
	set_latency(1_ms);
	if (audio_enabled_) resize(2,0);
	// Check licence
	lic_ = &Licence::getInstance();
	lic_->set_licence_file(licence_);
	caps_ = lic_->get_licence();
	log[log::info] << "Got licence \"name\": \"" << caps_.name << "\", \"streams\": " << caps_.streams << ", \"outputs\": " << caps_.outputs << ", \"resolution\": " << caps_.resolution << ", \"date\": \"" << caps_.date << "\"" << ", \"level\": \"" << caps_.level << "\"";
	// Init NDI
	if (!NDIlib_initialize())
		throw exception::InitializationFailed("Failed to initialize NDI output.");
}

NDIOutput::~NDIOutput() {
}

void NDIOutput::run() {
	NDIlib_send_create_t NDI_send_create_desc;
	NDI_send_create_desc.p_ndi_name = stream_.c_str();
	if (audio_enabled_) {
		NDI_send_create_desc.clock_audio = true;
	}
	pNDI_send_ = NDIlib_send_create(&NDI_send_create_desc);
	if (!pNDI_send_)
		throw exception::InitializationFailed("Failed to initialize NDI sender.");
	NDIlib_metadata_frame_t NDI_connection_type;
	NDI_connection_type.p_data = "<ndi_product long_name=\"Dicaffeine streamer.\" "
								 "             short_name=\"Dicaffeine\" "
								 "             manufacturer=\"Jiri Melnikov\" "
								 "             version=\"1.000.000\" "
								 "             session=\"default\" "
								 "             model_name=\"RaspiBase\" "
								 "             serial=\"AAAAAA\"/>";
	NDIlib_send_add_connection_metadata(pNDI_send_, &NDI_connection_type);
	timer_.reset();
	std::thread th(&NDIOutput::sound_sender, this);
	IOThread::run();
	stop_stream();
	th.join();
}

void NDIOutput::sound_sender() {
	while (running() && audio_enabled_) {
		aframe_to_send_ = std::dynamic_pointer_cast<core::RawAudioFrame>(pop_frame(1));
		if (streaming_enabled_ && aframe_to_send_) {
			NDIlib_audio_frame_v3_t NDI_audio_frame;
			NDI_audio_frame.no_channels = aframe_to_send_->get_channel_count();
			NDI_audio_frame.no_samples = aframe_to_send_->get_sample_count();
			NDI_audio_frame.sample_rate = aframe_to_send_->get_sampling_frequency();
			NDI_audio_frame.FourCC = NDIlib_FourCC_audio_type_FLTP;
			NDI_audio_frame.channel_stride_in_bytes = 4;
			NDI_audio_frame.p_data = aframe_to_send_->data();
			NDIlib_send_send_audio_v3(pNDI_send_, &NDI_audio_frame);
			aframe_to_send_ = nullptr;
		} else {
			ThreadBase::sleep(1_ms);
		}
	}
	
}

bool NDIOutput::step() {
	if (!NDIlib_send_get_no_connections(pNDI_send_, 10000)) {
		vframe_to_send_ = std::dynamic_pointer_cast<core::RawVideoFrame>(pop_frame(0));
		streaming_enabled_ = false;
		return true;
	} else {
		streaming_enabled_ = true;
	}
	vframe_to_send_ = std::dynamic_pointer_cast<core::RawVideoFrame>(pop_frame(0));
	if (!vframe_to_send_)
		return true;
	NDIlib_video_frame_v2_t NDI_video_frame;
	NDI_video_frame.xres = vframe_to_send_->get_width();
	NDI_video_frame.yres = vframe_to_send_->get_height();
	NDI_video_frame.FourCC = yuri_format_to_ndi(vframe_to_send_->get_format());
	NDI_video_frame.line_stride_in_bytes = 0; // autodetect
	if (fps_ == std::ceil(fps_)) {
		NDI_video_frame.frame_rate_N = 1000*fps_;
		NDI_video_frame.frame_rate_D = 1000;
	} else {
		int fps = std::ceil(fps_);
		NDI_video_frame.frame_rate_N = 1000*fps;
		NDI_video_frame.frame_rate_D = 1001;
	}
	if (caps_.level == "unlicensed") {
		if (timer_.get_duration() > max_time_) {
			request_end(core::yuri_exit_interrupted);
			return true;
		}
		draw_licence_rgba(vframe_to_send_, (float)timer_.get_duration().value/max_time_.value);
	}
	NDIlib_tally_t NDI_tally;
	NDIlib_send_get_tally(pNDI_send_, &NDI_tally, 0);
	NDI_video_frame.p_data = PLANE_RAW_DATA(vframe_to_send_,0);
	NDIlib_send_send_video_async_v2(pNDI_send_, &NDI_video_frame);
	return true;
}

void NDIOutput::stop_stream() {
	NDIlib_send_destroy(pNDI_send_);
	NDIlib_destroy();
}

void NDIOutput::emit_events() {
	// Nothing here now
}

bool NDIOutput::do_process_event(const std::string& event_name, const event::pBasicEvent& event) {
	if (iequals(event_name, "quit")) {
        request_end(core::yuri_exit_interrupted);
        return true;
    }
	log[log::info] << "Got unknown event \"" << event_name << "\", timestamp: " << event->get_timestamp();
	return false;
}

bool NDIOutput::set_param(const core::Parameter &param) {
	if (assign_parameters(param)
			(stream_, "stream")
			(audio_enabled_, "audio")
			(licence_, "licence")
			(fps_, "fps")
			)
		return true;
	return IOThread::set_param(param);
}

}
}
