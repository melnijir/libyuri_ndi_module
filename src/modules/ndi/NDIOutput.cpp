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

#include <cassert>

namespace yuri {
namespace ndi {

IOTHREAD_GENERATOR(NDIOutput)

core::Parameters NDIOutput::configure() {
	core::Parameters p = IOThread::configure();
	p["stream"]["Name of the stream to send."]="LiNDI";
	p["audio"]["Set to true if audio should be send."]=false;
	p["licence"]["Sets licence file location"]="";
	return p;
}

NDIOutput::NDIOutput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters)
:core::IOThread(log_,parent,1,0,std::string("NDIOutput")),
event::BasicEventProducer(log),event::BasicEventConsumer(log),
stream_("LiNDI"),audio_enabled_(false),interval_(1_s),licence_("") {
	IOTHREAD_INIT(parameters)
	if (audio_enabled_) resize(2,0);
	// Check licence
	lic_ = &Licence::getInstance();
	lic_->set_licence_file(licence_);
	LicenceCaps caps = lic_->get_licence();
	if (caps.level != "distributor")
		throw exception::InitializationFailed("Licence not suitable for NDI sender.");
	log[log::info] << "Got licence \"name\": \"" << caps.name << "\", \"streams\": " << caps.streams << ", \"outputs\": " << caps.outputs << ", \"resolution\": " << caps.resolution << ", \"date\": \"" << caps.date << "\"" << ", \"level\": \"" << caps.level << "\"";
	// Init NDI
	if (!NDIlib_initialize())
		throw exception::InitializationFailed("Failed to initialize NDI output.");
}

NDIOutput::~NDIOutput() {
}

void NDIOutput::run() {
	NDIlib_send_create_t NDI_send_create_desc;
	NDI_send_create_desc.p_ndi_name = stream_.c_str();
	pNDI_send_ = NDIlib_send_create(&NDI_send_create_desc);
	if (!pNDI_send_)
		throw exception::InitializationFailed("Failed to initialize NDI sender.");
	IOThread::run();
	stop_stream();
}

bool NDIOutput::step() {
	auto frame = std::dynamic_pointer_cast<core::RawVideoFrame>(pop_frame(0));
	if (!frame)
		return true;
	NDIlib_video_frame_v2_t NDI_video_frame;
	NDI_video_frame.xres = frame->get_width();
	NDI_video_frame.yres = frame->get_height();
	NDI_video_frame.FourCC = yuri_format_to_ndi(frame->get_format());
	NDI_video_frame.line_stride_in_bytes = 0; // autodetect
	NDI_video_frame.frame_rate_N = 60000;
	NDI_video_frame.frame_rate_D = 1000;
	NDI_video_frame.p_data = PLANE_RAW_DATA(frame,0);
	NDIlib_send_send_video_v2(pNDI_send_, &NDI_video_frame);
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
	log[log::info] << "Got unknown event \"" << event_name << "\" in time: " << event->get_timestamp();
	return true;
}

bool NDIOutput::set_param(const core::Parameter &param) {
	if (assign_parameters(param)
			(stream_, "stream")
			(audio_enabled_, "audio")
			(licence_, "licence")
			)
		return true;
	return IOThread::set_param(param);
}

}
}
