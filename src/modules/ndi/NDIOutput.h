/*
 * NDIOuptu.h
  */

#ifndef NDIOUTPUT_H_
#define NDIOUTPUT_H_

#include "yuri/core/thread/IOThread.h"
#include "yuri/event/BasicEventConsumer.h"
#include "yuri/core/thread/InputThread.h"
#include "yuri/event/BasicEventConsumer.h"
#include "yuri/event/BasicEventProducer.h"

#include "Licence.h"

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
private:
	bool step();
	virtual bool set_param(const core::Parameter &param) override;
	virtual bool do_process_event(const std::string& event_name, const event::pBasicEvent& event);

	void stop_stream();

	void emit_events();

	std::string stream_;
	bool audio_enabled_;
	float fps_;

	NDIlib_send_instance_t pNDI_send_;

	duration_t interval_;
	duration_t last_point_;
	Timer timer_;

	Licence* lic_;
	std::string licence_;
};

}

}

#endif /* NDIOUTPUT_H_ */
