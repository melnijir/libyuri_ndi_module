/*
 * NDIInput.h
  */

#ifndef NDIINPUT_H_
#define NDIINPUT_H_

#include "yuri/core/thread/IOThread.h"
#include "yuri/event/BasicEventConsumer.h"
#include "yuri/core/thread/InputThread.h"
#include "yuri/event/BasicEventConsumer.h"
#include "yuri/event/BasicEventProducer.h"

#include "Licence.h"

#include <Processing.NDI.Lib.h>

namespace yuri {

namespace ndi {

class NDIInput:public core::IOThread, public event::BasicEventProducer, public event::BasicEventConsumer {
public:
	NDIInput(log::Log &log_,core::pwThreadBase parent, const core::Parameters &parameters);
	virtual ~NDIInput() noexcept;
	virtual void run() override;
	IOTHREAD_GENERATOR_DECLARATION
	static core::Parameters configure();
	static std::vector<core::InputDeviceInfo> enumerate();
private:
	const NDIlib_source_t* get_source(std::string name, int *position);

	virtual bool set_param(const core::Parameter &param) override;
	virtual bool do_process_event(const std::string& event_name, const event::pBasicEvent& event);

	void emit_events();

	std::string stream_;
	std::string backup_;
	std::string format_;
	int nodata_timout_;
	bool audio_enabled_;
	position_t audio_pipe_;

	duration_t interval_;
	duration_t last_point_;
	Timer timer_;

	Licence* lic_;
	std::string licence_;

	NDIlib_find_instance_t ndi_finder_;
};

}

}

#endif /* NDIINPUT_H_ */
