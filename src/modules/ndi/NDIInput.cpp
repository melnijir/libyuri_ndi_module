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
#include "../../../libs/json.hpp"

#include <cassert>

namespace yuri {
namespace ndi {

using namespace std::chrono;


namespace {

const char*  extra_ips_config_file = "/etc/dicaffeine/dserver.json";

float get_event_float(const event::pBasicEvent& event) {
	switch(event->get_type()) {
	case event::event_type_t::integer_event:
		return static_cast<float>(event::get_value<event::EventInt>(event));
	case event::event_type_t::double_event:
		return static_cast<float>(event::get_value<event::EventDouble>(event));
	default:
		return 0;
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
	p["lowres"]["Set to true if video should be received in low resolution."]=false;
	p["reference_level"]["The audio reference level in dB. [-20dB - 20dB]"]=false;
	p["event_time"]["How often will be events fired."]=1.0;
	p["ndi_path"]["Path where to find the NDI libraries, if empty, env variable NDI_PATH is used."]="";
	return p;
}

NDIInput::NDIInput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters)
:core::IOThread(log_,parent,0,1,std::string("NDIInput")),
event::BasicEventProducer(log),event::BasicEventConsumer(log),
stream_(""),backup_(""),format_("fastest"),ndi_path_(""),audio_enabled_(false),lowres_enabled_(false),
reference_level_(0),audio_pipe_(-1),stream_fail_(0),event_time_(1_s),ptz_supported_(false),
last_pan_val_(0),last_tilt_val_(0),last_pan_speed_(0),last_tilt_speed_(0) {
	IOTHREAD_INIT(parameters)
	// Load NDI library
	NDIlib_ = load_ndi_library(ndi_path_);
	// Init NDI
	if (!NDIlib_->initialize())
		throw exception::InitializationFailed("Failed to initialize NDI input.");
	// Audio pipe is for further multichannel implementation
	audio_pipe_=(audio_enabled_?1:-1);
	resize(0,1+(audio_enabled_?1:0));
	// Check if there are extra ips in the config file
	try	{
		std::ifstream cfg_file(extra_ips_config_file);
		nlohmann::json cfg_json;
		cfg_file >> cfg_json;
		extra_ips_ = cfg_json.value("extra_ips", "");
	} catch(const std::exception& e) {
		log[log::warning] << "This module version was made for Dicaffeine installation but cannot find it's default configuration file.";
	}
}

NDIInput::~NDIInput() {
}

std::vector<core::InputDeviceInfo> NDIInput::enumerate() {
	// Returned devices
	std::vector<core::InputDeviceInfo> devices;
	std::vector<std::string> main_param_order = {"address"};

	// Find library
	auto NDIlib = load_ndi_library();

	// Check if there are extra ips in the environment
	auto env_ndi_extra_ips = std::getenv("NDI_EXTRA_IPS");

	// Create a finder
	NDIlib_find_create_t finder_desc;
	if (env_ndi_extra_ips) finder_desc.p_extra_ips = env_ndi_extra_ips;
	NDIlib_find_instance_t ndi_finder = NDIlib->find_create_v2(&finder_desc);
	if (!ndi_finder)
		throw exception::InitializationFailed("Failed to initialize NDI fidner.");

	// Search for the source on the network
	const NDIlib_source_t* sources = nullptr;
	for (int n = 0; n < 500; n++) { // For now about 500ms seems like a reasonable time to discover all sources.
		NDIlib->find_wait_for_sources(ndi_finder, 0); // Sadly timout not working in current NDI lib.
		uint32_t count;
		sources = NDIlib->find_get_current_sources(ndi_finder, &count);
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
		NDIlib_->find_wait_for_sources(ndi_finder_, 1000);
		uint32_t count = 0;
		sources = NDIlib_->find_get_current_sources(ndi_finder_, &count);
		for (uint32_t i = 0; i < count; i++) {
			if (name == sources[i].p_ndi_name) {
				*position = i;
				return sources;
			}
		}
	}
	return sources;
}

void NDIInput::sound_receiver() {
	// Yuri Audio
	core::pRawAudioFrame y_audio_frame;
	while (audio_running_ && audio_enabled_) {
		NDIlib_audio_frame_v2_t n_audio_frame;
		NDIlib_audio_frame_interleaved_16s_t n_audio_frame_16bpp_interleaved;
		switch (NDIlib_->recv_capture_v2(ndi_receiver_, nullptr, &n_audio_frame, nullptr, ndi_source_max_wait_ms)) {
		// Audio data
		case NDIlib_frame_type_audio:
			log[log::debug] << "Audio data received: " << n_audio_frame.no_samples << " samples, " << n_audio_frame.no_channels << " channels.";
			stream_fail_ = 0;
			n_audio_frame_16bpp_interleaved.reference_level = reference_level_;	// 0dB of headroom
			n_audio_frame_16bpp_interleaved.p_data = new short[n_audio_frame.no_samples*n_audio_frame.no_channels];
			// Convert it
			NDIlib_->util_audio_to_interleaved_16s_v2(&n_audio_frame, &n_audio_frame_16bpp_interleaved);
			// Process data!
			y_audio_frame = core::RawAudioFrame::create_empty(core::raw_audio_format::signed_16bit, n_audio_frame.no_channels, n_audio_frame.sample_rate, n_audio_frame_16bpp_interleaved.p_data, n_audio_frame.no_samples * n_audio_frame.no_channels);
			push_frame(audio_pipe_, y_audio_frame);
			// Free the interleaved audio data
			delete[] n_audio_frame_16bpp_interleaved.p_data;
			NDIlib_->recv_free_audio_v2(ndi_receiver_, &n_audio_frame);
			break;
		// Everything else
		default:
			log[log::debug] << "Unknown message found.";
			stream_fail_ = 0;
			break;
		}
	}
}

void NDIInput::run() {
	// Basic finder
	NDIlib_find_create_t finder_desc;
	if (extra_ips_.length()) {
		log[log::info] << "Found extra IPs \"" << extra_ips_ << "\", adding to finder description.";
		finder_desc.p_extra_ips = extra_ips_.c_str();
	}
	// Start event timer
	event_timer_.reset();

	while (still_running()) {
		// Init NDI finder
		ndi_finder_ = NDIlib_->find_create_v2(&finder_desc);
		if (!ndi_finder_)
			throw exception::InitializationFailed("Failed to initialize NDI finder.");
		// Keep and update stream status
		bool stream_running = false;
		emit_event("stream_off");
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
		receiver_desc.allow_video_fields = false;
		if (format_ == "uyvy") {
			receiver_desc.color_format = NDIlib_recv_color_format_UYVY_RGBA;
		} else if (format_ == "bgra") {
			receiver_desc.color_format = NDIlib_recv_color_format_BGRX_BGRA;
		} else if (format_ == "rgba") {
			receiver_desc.color_format = NDIlib_recv_color_format_RGBX_RGBA;
		} else {
			receiver_desc.color_format = NDIlib_recv_color_format_fastest;
		}
		if (lowres_enabled_) {
			receiver_desc.bandwidth = NDIlib_recv_bandwidth_lowest;
		} else {
			receiver_desc.bandwidth = NDIlib_recv_bandwidth_highest;
		}


		ndi_receiver_ = NDIlib_->recv_create_v3(&receiver_desc);
		if (!ndi_receiver_) {
			log[log::fatal] << "Failed to initialize NDI receiver.";
			throw exception::InitializationFailed("Failed to initialize NDI receiver.");
		}

		// We are now going to mark this source as being on program output for tally purposes (but not on preview)
		NDIlib_tally_t tally_state;
		tally_state.on_program = true;
		tally_state.on_preview = true;
		NDIlib_->recv_set_tally(ndi_receiver_, &tally_state);

		NDIlib_metadata_frame_t enable_hw_accel;
		enable_hw_accel.p_data = (char*)"<ndi_hwaccel enabled=\"true\"/>";
		NDIlib_->recv_send_metadata(ndi_receiver_, &enable_hw_accel);

		// Ready to play
		log[log::info] << "Receiving started";
		// Start audio receiver
		audio_running_ = true;
		std::thread th(&NDIInput::sound_receiver, this);
		// Start video receiver
		while (still_running() && stream_fail_++ < 20) {
			NDIlib_video_frame_v2_t n_video_frame;
			NDIlib_metadata_frame_t metadata_frame;
			NDIlib_recv_queue_t recv_queue;
			// Yuri Video
			yuri::format_t y_video_format;
			core::pRawVideoFrame y_video_frame;
			// Yuri timestamp
			time_point<high_resolution_clock, nanoseconds> y_timestamp;
			int64_t y_timecode;
			// Receive
			switch (NDIlib_->recv_capture_v2(ndi_receiver_, &n_video_frame, nullptr, &metadata_frame, ndi_source_max_wait_ms)) {
			// No data
			case NDIlib_frame_type_none:
				log[log::debug] << "No data received.";
				break;
			// Video data
			case NDIlib_frame_type_video:
				log[log::debug] << "Video data received: " << n_video_frame.xres << "x" << n_video_frame.yres;
				stream_fail_ = 0;
				if (!stream_running) {
					stream_running = true;
					emit_event("stream_on");
				}
				// Check queue - it too large it's time to drop frames
				NDIlib_->recv_get_queue(ndi_receiver_, &recv_queue);
				if (recv_queue.video_frames > ndi_source_max_queue_frames) {
					NDIlib_->recv_free_video_v2(ndi_receiver_, &n_video_frame);
					log[log::info] << "Loosing video frames, queue: " << recv_queue.video_frames;
					break;
				}
				y_video_format = ndi_format_to_yuri(n_video_frame.FourCC);
				y_video_frame = core::RawVideoFrame::create_empty(y_video_format, {(uint32_t)n_video_frame.xres, (uint32_t)n_video_frame.yres}, true);
				std::copy(n_video_frame.p_data, n_video_frame.p_data + n_video_frame.yres * n_video_frame.line_stride_in_bytes, PLANE_DATA(y_video_frame, 0).begin());
				y_timestamp = time_point<high_resolution_clock, nanoseconds>(nanoseconds(n_video_frame.timestamp*100));
				y_timecode = n_video_frame.timecode;
				// Free video frame as early as possible
				NDIlib_->recv_free_video_v2(ndi_receiver_, &n_video_frame);
				y_video_frame->set_timestamp(y_timestamp);
				emit_event("timecode", y_timecode);
				emit_event("timestamp", y_timestamp);
				// Push frame out
				push_frame(0,y_video_frame);
				break;
			// Meta data
			case NDIlib_frame_type_metadata:
				log[log::debug] << "Metadata received.";
				stream_fail_ = 0;
				NDIlib_->recv_free_metadata(ndi_receiver_, &metadata_frame);
				break;
			// There is a status change on the receiver (e.g. new web interface)
			case NDIlib_frame_type_status_change:
				log[log::debug] << "Sender connection status changed.";
				stream_fail_ = 0;
				if (NDIlib_->recv_ptz_is_supported(ndi_receiver_)) {
					log[log::info] << "Sender supports PTZ, enabling events.";
					ptz_supported_ = true;
				} else {
					log[log::info] << "Sender doest not support PTZ, disabling events.";
					ptz_supported_ = false;
				}
				break;
			// Everything else
			default:
				log[log::debug] << "Unknown message found.";
				stream_fail_ = 0;
				break;
			}
			if (event_timer_.get_duration() > event_time_) {
				emit_events();
				event_timer_.reset();
			}
			process_events();
		}
		log[log::info] << "Stopping receiver";

		// Connect audio thread
		audio_running_ = false;
		th.join();

		// Get it out
		NDIlib_->recv_destroy(ndi_receiver_);
		NDIlib_->destroy();
		// Reset fails
		stream_fail_ = 0;
		// Destroy finder
		NDIlib_->find_destroy(ndi_finder_);
	}
}

void NDIInput::emit_events() {
	// Performace info (dropped and received frames)
	NDIlib_recv_performance_t perf_total, perf_dropped;
	NDIlib_->recv_get_performance(ndi_receiver_, &perf_total, &perf_dropped);
	emit_event("audio_received", perf_total.audio_frames);
	emit_event("audio_dropped", perf_dropped.audio_frames);
	emit_event("video_received", perf_total.video_frames);
	emit_event("video_dropped", perf_dropped.video_frames);
}

bool NDIInput::do_process_event(const std::string& event_name, const event::pBasicEvent& event) {
	if (iequals(event_name, "quit")) {
        request_end(core::yuri_exit_interrupted);
        return true;
    } else {
		if (!ptz_supported_)
			return false;
		if (iequals(event_name,"recall_preset")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				NDIlib_->recv_ptz_recall_preset(ndi_receiver_, event::lex_cast_value<int>(val[0]), event::lex_cast_value<float>(val[1]));
			} else {
				auto val = event::get_value<event::EventInt>(event);
				NDIlib_->recv_ptz_recall_preset(ndi_receiver_, val, 1.0);
			}
		} else if (iequals(event_name,"store_preset")) {
			auto val = event::get_value<event::EventInt>(event);
			NDIlib_->recv_ptz_store_preset(ndi_receiver_, val);
		} else if (iequals(event_name,"zoom")) {
			auto val = get_event_float(event);
			NDIlib_->recv_ptz_zoom(ndi_receiver_, val);
		} else if (iequals(event_name,"zoom_speed")) {
			auto val = get_event_float(event);
			if (val < -1 || val > 1) val = 0;
			NDIlib_->recv_ptz_zoom_speed(ndi_receiver_, val);
		} else if (iequals(event_name,"pan_tilt")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				last_pan_val_ = event::lex_cast_value<float>(val[0]);
				last_tilt_val_ = event::lex_cast_value<float>(val[1]);
				NDIlib_->recv_ptz_pan_tilt(ndi_receiver_, last_pan_val_, last_tilt_val_);
			} else {
				log[log::info] << "Got pan_tilt event in wrong format, must be vector of two floats <-1..0..1>.";
			}
		} else if (iequals(event_name,"pan")) {
			last_pan_val_ = get_event_float(event);
			NDIlib_->recv_ptz_pan_tilt(ndi_receiver_, last_pan_val_, last_tilt_val_);
		} else if (iequals(event_name,"tilt")) {
			last_tilt_val_ = get_event_float(event);
			NDIlib_->recv_ptz_pan_tilt(ndi_receiver_, last_pan_val_, last_tilt_val_);
		} else if (iequals(event_name,"pan_tilt_speed")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				last_pan_speed_ = 0-event::lex_cast_value<float>(val[0]);
				last_tilt_speed_ = event::lex_cast_value<float>(val[1]);
				NDIlib_->recv_ptz_pan_tilt_speed(ndi_receiver_, last_pan_speed_, last_tilt_speed_);
			} else {
				log[log::info] << "Got pan_tilt_speed event in wrong format, must be vector of two floats <-1..0..1>.";
			}
		} else if (iequals(event_name,"pan_speed")) {
			last_pan_speed_ = get_event_float(event);
			log[log::info] << "pan_speed: [" << last_pan_speed_ << "," << last_tilt_speed_ << "]";
			NDIlib_->recv_ptz_pan_tilt_speed(ndi_receiver_, last_pan_speed_, last_tilt_speed_);
		} else if (iequals(event_name,"tilt_speed")) {
			last_tilt_speed_ = get_event_float(event);
			log[log::info] << "tilt_speed: [" << last_pan_speed_ << "," << last_tilt_speed_ << "]";
			NDIlib_->recv_ptz_pan_tilt_speed(ndi_receiver_, last_pan_speed_, last_tilt_speed_);
		} else if (iequals(event_name,"auto_focus")) {
			NDIlib_->recv_ptz_auto_focus(ndi_receiver_);
		} else if (iequals(event_name,"focus")) {
			auto val = get_event_float(event);
			NDIlib_->recv_ptz_focus(ndi_receiver_, val);
		} else if (iequals(event_name,"focus_speed")) {
			auto val = get_event_float(event);
			NDIlib_->recv_ptz_focus_speed(ndi_receiver_, val);
		} else if (iequals(event_name,"white_balance_auto")) {
			NDIlib_->recv_ptz_white_balance_auto(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_indoor")) {
			NDIlib_->recv_ptz_white_balance_indoor(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_outdoor")) {
			NDIlib_->recv_ptz_white_balance_outdoor(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_oneshot")) {
			NDIlib_->recv_ptz_white_balance_oneshot(ndi_receiver_);
		} else if (iequals(event_name,"white_balance_manual")) {
			if (event->get_type() == event::event_type_t::vector_event) {
				auto val = event::get_value<event::EventVector>(event);
				if(val.size() < 2) return false;
				NDIlib_->recv_ptz_white_balance_manual(ndi_receiver_, event::lex_cast_value<float>(val[0]), event::lex_cast_value<float>(val[1]));
			} else {
				log[log::info] << "Got white_balance_manual event in wrong format, must be vector of two floats <-1..0..1>.";
			}
		} else if (iequals(event_name,"exposure_auto")) {
			NDIlib_->recv_ptz_exposure_auto(ndi_receiver_);
		} else if (iequals(event_name,"exposure_manual")) {
			auto val = get_event_float(event);
			NDIlib_->recv_ptz_exposure_manual(ndi_receiver_, val);
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
			(ndi_path_, "ndi_path")
			(audio_enabled_, "audio")
			(lowres_enabled_, "lowres")
			(reference_level_, "reference_level"))
		return true;
	return IOThread::set_param(param);
}

}
}
