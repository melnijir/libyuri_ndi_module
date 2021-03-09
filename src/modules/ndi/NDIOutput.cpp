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
#include <cstring>

namespace yuri {
namespace ndi {

IOTHREAD_GENERATOR(NDIOutput)

core::Parameters NDIOutput::configure() {
	core::Parameters p = IOThread::configure();
	p["stream"]["Name of the stream to send."]="Dicaffeine";
	p["audio"]["Set to true if audio should be send."]=false;
	p["licence"]["Sets licence file location"]="";
	p["fps"]["Sets fps indicator sent in the stream"]="";
	return p;
}

NDIOutput::NDIOutput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters)
:core::IOThread(log_,parent,1,0,std::string("NDIOutput")),
event::BasicEventProducer(log),event::BasicEventConsumer(log),
stream_("Dicaffeine"),audio_enabled_(false),fps_(60),max_time_(30_minutes),licence_("") {
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

    static const char* p_connection_str =	"<ndi_product long_name=\"NDI Yuri module\" "
											"             short_name=\"NDI module\" "
											"             manufacturer=\"Unknown.\" "
											"             version=\"1.000.000\" "
											"             session=\"default\" "
											"             model_name=\"YM\" "
											"             serial=\"ABCDEFG\"/>";
    NDIlib_metadata_frame_t NDI_connection_type;
    NDI_connection_type.length          = (int)::strlen(p_connection_str);
    NDI_connection_type.timecode        = NDIlib_send_timecode_synthesize;
    NDI_connection_type.p_data          = (char*)p_connection_str;	// should be const in header file
	NDIlib_send_add_connection_metadata(pNDI_send_, &NDI_connection_type);
	licence_timer_.reset();
	std::thread th(&NDIOutput::sound_sender, this);
	IOThread::run();
	stop_stream();
	th.join();
}

void NDIOutput::sound_sender() {
	while (running() && audio_enabled_) {
		aframe_to_send_ = std::dynamic_pointer_cast<core::RawAudioFrame>(pop_frame(1));
		if (streaming_enabled_ && aframe_to_send_) {
			NDIlib_audio_frame_interleaved_16s_t NDI_audio_frame;
			NDI_audio_frame.no_channels = aframe_to_send_->get_channel_count();
			NDI_audio_frame.no_samples = aframe_to_send_->get_sample_count();
			NDI_audio_frame.sample_rate = aframe_to_send_->get_sampling_frequency();
			NDI_audio_frame.p_data = (short*)aframe_to_send_->data();
			NDIlib_util_send_send_audio_interleaved_16s(pNDI_send_, &NDI_audio_frame);
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
	if (caps_.level == "unlicensed" && licence_timer_.get_duration() > max_time_) {
		request_end(core::yuri_exit_interrupted);
		return true;
	}
	NDIlib_tally_t NDI_tally;
	NDIlib_send_get_tally(pNDI_send_, &NDI_tally, 0);
	NDI_video_frame.p_data = PLANE_RAW_DATA(vframe_to_send_,0);
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
