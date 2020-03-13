/*
 * NDIInput.cpp
 */

#include "NDIInput.h"

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

using namespace std::chrono;


namespace {
template<typename T>
void draw_line(T*start, T*end, T*out_end) {
	T t_max = std::numeric_limits<T>::max();
	std::transform(start,end,out_end,[t_max](const T& v){return v^t_max;});
}
template<typename T, class T2>
void draw_licence(T2* frame, size_t lines, size_t line_size) {
	T* s= reinterpret_cast<T*>(frame);
	const size_t size = line_size *sizeof(T2) / sizeof(T);
	for (size_t line = 0; line < lines; ++line) {
		draw_line(s, s + size, s);
		s += size;
	}
}
}

IOTHREAD_GENERATOR(NDIInput)

core::Parameters NDIInput::configure() {
	core::Parameters p = IOThread::configure();
	p["stream"]["Name of the stream to read."]="";
	p["backup"]["Name of the backup stream to read."]="";
	p["format"]["Which format to prefer [fastest/rgb/yuv]."]="fastest";
	p["audio"]["Set to true if audio should be received."]=false;
	p["licence"]["Sets licence file location"]="";
	return p;
}

NDIInput::NDIInput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters)
:core::IOThread(log_,parent,0,1,std::string("NDIInput")),
event::BasicEventProducer(log),event::BasicEventConsumer(log),
stream_(""),backup_(""),format_("fastest"),audio_enabled_(false),audio_pipe_(-1),interval_(3_s),licence_(""),licence_display_(false),licence_required_(false),ptz_supported_(false) {
	IOTHREAD_INIT(parameters)
	// Check licence
	lic_ = &Licence::getInstance();
	lic_->set_licence_file(licence_);
	caps_ = lic_->get_licence();
	log[log::info] << "Got licence \"name\": \"" << caps_.name << "\", \"streams\": " << caps_.streams << ", \"outputs\": " << caps_.outputs << ", \"resolution\": " << caps_.resolution << ", \"date\": \"" << caps_.date << "\"" << ", \"level\": \"" << caps_.level << "\"";
	// Init NDI
	if (!NDIlib_initialize())
		throw exception::InitializationFailed("Failed to initialize NDI input.");
	// Audio pipe is for further multichannel implementation
	audio_pipe_=(audio_enabled_?1:-1);
	resize(0,1+(audio_enabled_?1:0));
}

NDIInput::~NDIInput() {
}

std::vector<core::InputDeviceInfo> NDIInput::enumerate() {
	// Returned devices
	std::vector<core::InputDeviceInfo> devices;
	std::vector<std::string> main_param_order = {"address"};

	// Create a finder
	const NDIlib_find_create_t finder_desc;
	NDIlib_find_instance_t ndi_finder = NDIlib_find_create_v2(&finder_desc);
	if (!ndi_finder)
		throw exception::InitializationFailed("Failed to initialize NDI fidner.");

	// Search for the source on the network
	const NDIlib_source_t* sources = nullptr;
	for (int n = 0; n < 500; n++) { // For now about 500ms seems like a reasonable time to discover all sources.
		NDIlib_find_wait_for_sources(ndi_finder, 0); // Sadly timout not working in current NDI lib.
		uint32_t count;
		sources = NDIlib_find_get_current_sources(ndi_finder, &count);
		for (uint32_t i = 0; i < count; i++) {
			core::InputDeviceInfo device;
			device.main_param_order = main_param_order;
			device.device_name = sources[i].p_ndi_name;
			core::InputDeviceConfig cfg_base;
			cfg_base.params["address"]=sources[i].p_url_address;
			device.configurations.push_back(std::move(cfg_base));
			if (std::find_if(devices.begin(), devices.end(), [&] (const core::InputDeviceInfo &device){
				return (device.device_name == sources[i].p_ndi_name);
			}) == devices.end()) devices.push_back(std::move(device));
		}
		sleep(1_ms);
	}
	return devices;
}

const NDIlib_source_t* NDIInput::get_source(std::string name, int *position) {
	// Run finder
	int tries = 0;
	const NDIlib_source_t* sources = nullptr;
	while (tries++ < 10) {
		NDIlib_find_wait_for_sources(ndi_finder_, 1000);
		uint32_t count = 0;
		sources = NDIlib_find_get_current_sources(ndi_finder_, &count);
		for (uint32_t i = 0; i < count; i++) {
			if (name == sources[i].p_ndi_name) {
				*position = i;
				return sources;
			}
		}
	}
	return sources;
}

void NDIInput::run() {
	// Basic finder
	const NDIlib_find_create_t finder_desc;
	ndi_finder_ = NDIlib_find_create_v2(&finder_desc);
	if (!ndi_finder_)
		throw exception::InitializationFailed("Failed to initialize NDI finder.");

	while (still_running()) {
		// Search for the source on the network
		int stream_id = -1;
		const NDIlib_source_t* sources = nullptr;
		while (still_running() && stream_id == -1) {
			sources = get_source(stream_, &stream_id);
			if (stream_id == -1 && backup_.length() > 0)
				sources = get_source(backup_, &stream_id);
		}

		if (stream_id == -1)
			continue;
		
		log[log::info] << "Found stream \"" << sources[stream_id].p_ndi_name << "\" with id " << stream_id;

		NDIlib_recv_create_v3_t receiver_desc;
		receiver_desc.source_to_connect_to = sources[stream_id];
		receiver_desc.p_ndi_recv_name = "Yuri NDI receiver";
		if (format_ == "yuv") {
			receiver_desc.color_format = NDIlib_recv_color_format_UYVY_RGBA;
		} else if (format_ == "rgb") {
			receiver_desc.color_format = NDIlib_recv_color_format_RGBX_RGBA;
		} else {
			receiver_desc.color_format = NDIlib_recv_color_format_fastest;
		}
		receiver_desc.bandwidth = NDIlib_recv_bandwidth_highest;


		ndi_receiver_ = NDIlib_recv_create_v3(&receiver_desc);
		if (!ndi_receiver_) {
			log[log::fatal] << "Failed to initialize NDI receiver.";
			throw exception::InitializationFailed("Failed to initialize NDI receiver.");
		}

		// We are now going to mark this source as being on program output for tally purposes (but not on preview)
		NDIlib_tally_t tally_state;
		tally_state.on_program = true;
		tally_state.on_preview = true;
		NDIlib_recv_set_tally(ndi_receiver_, &tally_state);

		NDIlib_metadata_frame_t enable_hw_accel;
		enable_hw_accel.p_data = (char*)"<ndi_hwaccel enabled=\"true\"/>";
		NDIlib_recv_send_metadata(ndi_receiver_, &enable_hw_accel);

		// Prepare timer for events
		timer_.reset();

		// Ready to play
		int stream_fail = 0;
		log[log::info] << "Receiving started";
		while (still_running() && stream_fail++ < 5) {
			NDIlib_video_frame_v2_t n_video_frame;
			NDIlib_audio_frame_v2_t n_audio_frame;
			NDIlib_metadata_frame_t metadata_frame;
			NDIlib_audio_frame_interleaved_16s_t n_audio_frame_16bpp_interleaved;
			// Yuri Video
			yuri::format_t y_video_format;
			core::pRawVideoFrame y_video_frame;
			// Yuri Audio
			core::pRawAudioFrame y_audio_frame;
			// Yuri timestamp
			time_point<high_resolution_clock, nanoseconds> y_timestamp;
			// Receive
			switch (NDIlib_recv_capture_v2(ndi_receiver_, &n_video_frame, &n_audio_frame, &metadata_frame, 1000)) {
			// No data
			case NDIlib_frame_type_none:
				log[log::debug] << "No data received.";
				break;
			// Video data
			case NDIlib_frame_type_video:
				log[log::debug] << "Video data received: " << n_video_frame.xres << "x" << n_video_frame.yres;
				stream_fail = 0;
				y_video_format = ndi_format_to_yuri(n_video_frame.FourCC);
				y_video_frame = core::RawVideoFrame::create_empty(y_video_format, {(uint32_t)n_video_frame.xres, (uint32_t)n_video_frame.yres}, true);
				std::copy(n_video_frame.p_data, n_video_frame.p_data + n_video_frame.yres * n_video_frame.line_stride_in_bytes, PLANE_DATA(y_video_frame, 0).begin());
				y_timestamp = time_point<high_resolution_clock, nanoseconds>(nanoseconds(n_video_frame.timestamp*100));
				y_video_frame->set_timestamp(y_timestamp);
				emit_event("timecode", n_video_frame.timecode);
				emit_event("timestamp", n_video_frame.timestamp);
				NDIlib_recv_free_video_v2(ndi_receiver_, &n_video_frame);
				// Insert licence warning
				if (licence_required_ && licence_display_)
					draw_licence<uint8_t>(PLANE_DATA(y_video_frame, 0).begin(), y_video_frame->get_height(), y_video_frame->get_width()*core::raw_format::get_fmt_bpp(y_video_frame->get_format(),0)/8);
				// Push frame out
				push_frame(0,y_video_frame);
				break;
			// Audio data
			case NDIlib_frame_type_audio:
				log[log::debug] << "Audio data received: " << n_audio_frame.no_samples << " samples, " << n_audio_frame.no_channels << " channels.";
				stream_fail = 0;
				if (audio_enabled_) {
					n_audio_frame_16bpp_interleaved.reference_level = 20;	// 20dB of headroom
					n_audio_frame_16bpp_interleaved.p_data = new short[n_audio_frame.no_samples*n_audio_frame.no_channels];
					// Convert it
					NDIlib_util_audio_to_interleaved_16s_v2(&n_audio_frame, &n_audio_frame_16bpp_interleaved);
					// Process data!
					y_audio_frame = core::RawAudioFrame::create_empty(core::raw_audio_format::signed_16bit, n_audio_frame.no_channels, n_audio_frame.sample_rate, n_audio_frame_16bpp_interleaved.p_data, n_audio_frame.no_samples * n_audio_frame.no_channels);
					push_frame(audio_pipe_, y_audio_frame);
					// Free the interleaved audio data
					delete[] n_audio_frame_16bpp_interleaved.p_data;
				}
				NDIlib_recv_free_audio_v2(ndi_receiver_, &n_audio_frame);
				break;
			// Meta data
			case NDIlib_frame_type_metadata:
				log[log::debug] << "Metadata received.";
				stream_fail = 0;
				NDIlib_recv_free_metadata(ndi_receiver_, &metadata_frame);
				break;
			// There is a status change on the receiver (e.g. new web interface)
			case NDIlib_frame_type_status_change:
				log[log::debug] << "Sender connection status changed.";
				if (NDIlib_recv_ptz_is_supported(ndi_receiver_)) {
					log[log::info] << "Sender supports PTZ, enabling events.";
					ptz_supported_ = true;
				} else {
					log[log::info] << "Sender doest not support PTZ, disabling events.";
					ptz_supported_ = false;
				}
				stream_fail = 0;
				break;
			// Everything else
			default:
				log[log::debug] << "Unknown message found.";
				stream_fail = 0;
				break;
			}
			// We proccessed frame, now check if any events are waiting
			const auto dur = timer_.get_duration();
			if ((dur - last_point_) > interval_) {
				emit_events();
				last_point_+=interval_;
				licence_display_ = !licence_display_;
			}
			process_events();
		}

		log[log::info] << "Stopping receiver";

		// Get it out
		NDIlib_recv_destroy(ndi_receiver_);
		NDIlib_destroy();
	}

	NDIlib_find_destroy(ndi_finder_);
}

void NDIInput::emit_events() {
	// Nothing here now
}

bool NDIInput::do_process_event(const std::string& event_name, const event::pBasicEvent& event) {
	if (iequals(event_name, "quit")) {
        request_end(core::yuri_exit_interrupted);
        return true;
    } else {
		if (!ptz_supported_)
			return false;
		licence_required_ = true;
		licence_display_ = true;
		if (iequals(event_name,"recall_preset")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				NDIlib_recv_ptz_recall_preset(ndi_receiver_, event::lex_cast_value<int>(val[0]), event::lex_cast_value<float>(val[1]));
			} else {
				auto val = event::get_value<event::EventInt>(event);
				NDIlib_recv_ptz_recall_preset(ndi_receiver_, val, 1.0);
			}
		} else if (iequals(event_name,"store_preset")) {
			auto val = event::get_value<event::EventInt>(event);
			NDIlib_recv_ptz_store_preset(ndi_receiver_, val);
		} else if (iequals(event_name,"zoom")) {
			auto val = event::get_value<event::EventDouble>(event);
			NDIlib_recv_ptz_zoom(ndi_receiver_, val);
		} else if (iequals(event_name,"zoom_speed")) {
			auto val = event::get_value<event::EventDouble>(event);
			NDIlib_recv_ptz_zoom_speed(ndi_receiver_, val);
		} else if (iequals(event_name,"pan_tilt")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				NDIlib_recv_ptz_pan_tilt(ndi_receiver_, event::lex_cast_value<float>(val[0]), event::lex_cast_value<float>(val[1]));
			} else {
				log[log::info] << "Got pan_tilt event in wrong format, must be vector of two floats <-1..0..1>.";
			}
		} else if (iequals(event_name,"pan_tilt_speed")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				NDIlib_recv_ptz_pan_tilt_speed(ndi_receiver_, event::lex_cast_value<float>(val[0]), event::lex_cast_value<float>(val[1]));
			} else {
				log[log::info] << "Got pan_tilt_speed event in wrong format, must be vector of two floats <-1..0..1>.";
			}
		} else if (iequals(event_name,"auto_focus")) {
			NDIlib_recv_ptz_auto_focus(ndi_receiver_);
		} else if (iequals(event_name,"focus")) {
			auto val = event::get_value<event::EventDouble>(event);
			NDIlib_recv_ptz_focus(ndi_receiver_, val);
		} else if (iequals(event_name,"focus_speed")) {
			auto val = event::get_value<event::EventDouble>(event);
			NDIlib_recv_ptz_focus_speed(ndi_receiver_, val);
		} else if (iequals(event_name,"white_balance_auto")) {
			NDIlib_recv_ptz_white_balance_auto(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_indoor")) {
			NDIlib_recv_ptz_white_balance_indoor(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_outdoor")) {
			NDIlib_recv_ptz_white_balance_outdoor(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_oneshot")) {
			NDIlib_recv_ptz_white_balance_oneshot(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_manual")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				NDIlib_recv_ptz_white_balance_manual(ndi_receiver_, event::lex_cast_value<float>(val[0]), event::lex_cast_value<float>(val[1]));
			} else {
				log[log::info] << "Got white_balance_manual event in wrong format, must be vector of two floats <-1..0..1>.";
			}
		} else if (iequals(event_name,"exposure_auto")) {
			NDIlib_recv_ptz_exposure_auto(ndi_receiver_);
		} else if (iequals(event_name,"exposure_manual")) {
			auto val = event::get_value<event::EventDouble>(event);
			NDIlib_recv_ptz_exposure_manual(ndi_receiver_, val);
		} else {
			log[log::info] << "Got unknown event \"" << event_name << "\", timestamp: " << event->get_timestamp();
		}
	}
	return false;
}

bool NDIInput::set_param(const core::Parameter &param) {
	if (assign_parameters(param)
			(stream_, "stream")
			(backup_, "backup")
			(format_, "format")
			(audio_enabled_, "audio")
			(licence_, "licence")
			)
		return true;
	return IOThread::set_param(param);
}

}
}
