/*
 * NDIOuptu.h
  */

#ifndef NDIOUTPUT_H_
#define NDIOUTPUT_H_

#include "yuri/core/thread/IOThread.h"
#include "yuri/core/thread/InputThread.h"
#include "yuri/event/BasicEventConsumer.h"
#include "yuri/event/BasicEventProducer.h"
#include "yuri/core/frame/RawVideoFrame.h"
#include "yuri/core/frame/RawAudioFrame.h"

#include <Processing.NDI.Lib.h>

namespace yuri {

namespace ndi {

class NDIOutput:public core::IOThread, public event::BasicEventProducer, public event::BasicEventConsumer {
public:
	NDIOutput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters);
	virtual ~NDIOutput() noexcept;
	virtual void run() override;
	IOTHREAD_GENERATOR_DECLARATION
	static core::Parameters configure();
	void sound_sender();
private:
	bool step();
	virtual bool set_param(const core::Parameter &param) override;
	virtual bool do_process_event(const std::string& event_name, const event::pBasicEvent& event);

	void stop_stream();

	void emit_events();

	std::string stream_;
	bool audio_enabled_;
	bool streaming_enabled_;
	float fps_;
	std::string ndi_path_;

	const NDIlib_v5* NDIlib_;
	NDIlib_send_instance_t pNDI_send_;

	core::pRawAudioFrame aframe_to_send_;
	core::pRawVideoFrame vframe_to_send_;
	core::pVideoFrame vcmpframe_to_send_;
};

}

}

#endif /* NDIOUTPUT_H_ */
