/*!
 * @file 		NetInfo.h
 * @author 		Jiri Melnikov <jiri@melnikoff.org>
 * @date 		31.07.2018
 *
 */

#ifndef NETINFO_H_
#define NETINFO_H_

#include "yuri/core/thread/IOThread.h"
#include "yuri/event/BasicEventConsumer.h"
#include "yuri/event/BasicEventProducer.h"

#include <stdio.h>      
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <poll.h>

namespace yuri {
namespace ndi_net {

class NetInfo: public core::IOThread, public event::BasicEventProducer, public event::BasicEventConsumer
{
public:
	IOTHREAD_GENERATOR_DECLARATION
	static core::Parameters configure();
	NetInfo(log::Log &log_, core::pwThreadBase parent, const core::Parameters &parameters);
	virtual ~NetInfo() noexcept;
private:
	virtual void run();

	virtual bool set_param(const core::Parameter& param);
	virtual bool do_process_event(const std::string& event_name, const event::pBasicEvent& event);

	void emit_events();

	duration_t interval_;
	duration_t last_point_;
	Timer timer_;

	struct ifaddrs * if_addr_list_;

	std::string interface_;
};

} /* namespace ndi_net */
} /* namespace yuri */
#endif /* NETINFO_H_ */
