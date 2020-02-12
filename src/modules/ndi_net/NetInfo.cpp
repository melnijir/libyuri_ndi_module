/*!
 * @file 		NetInfo.cpp
 * @author 		Jiri Melnikov <jiri@melnikoff.org>
 * @date 		31.07.2018
 *
 */

#include "NetInfo.h"
#include "yuri/core/Module.h"

#include "../common/utils.h"

namespace yuri {
namespace ndi_net {

using s_event_map = std::map<std::string, yuri::event::pBasicEvent>;
using s_event_pair = std::pair<std::string, yuri::event::pBasicEvent>;

IOTHREAD_GENERATOR(NetInfo)

MODULE_REGISTRATION_BEGIN("ndi_net_info")
		REGISTER_IOTHREAD("ndi_net_info",NetInfo)
MODULE_REGISTRATION_END()

core::Parameters NetInfo::configure()
{
	core::Parameters p = core::IOThread::configure();
	p.set_description("NetInfo");
	p["interval"]["Interval to send notifications (in seconds)"]=1.0;
	p["interface"]["Which interface should be checked (:all for all devices in system)"]=":all";
//	p->set_max_pipes(0,0);
	return p;
}


NetInfo::NetInfo(log::Log &log_, core::pwThreadBase parent, const core::Parameters &parameters):
core::IOThread(log_,parent,0,0,std::string("ndi_net_info")),
event::BasicEventProducer(log),event::BasicEventConsumer(log),
interval_(1_s),if_addr_list_(nullptr),interface_(":all")
{
	set_latency(10_ms);
	IOTHREAD_INIT(parameters);
	if (get_latency() > interval_) {
		set_latency(interval_/4);
	}
}

NetInfo::~NetInfo() noexcept
{
}

namespace {
	void add_event(std::string interface, std::string type, std::string address, std::map<std::string, s_event_map> &events) {
		auto event = std::make_shared<yuri::event::EventString>(address);
		if (events.find(interface) == events.end()) {
			s_event_map event_ip_map;
			event_ip_map.insert(s_event_pair(type, event));
			auto event_new = std::pair<std::string, s_event_map>(interface, event_ip_map);
			events.insert(event_new);
		} else {
			auto event_pair = s_event_pair(type, event);
			events.at(interface).insert(event_pair);
		}
	}
}

void NetInfo::run()
{
	timer_.reset();
	while (still_running()) {
		wait_for_events(get_latency());
		const auto dur = timer_.get_duration();
		if ((dur - last_point_) > interval_) {
			emit_events();
			last_point_+=interval_;
		}
		process_events();
	}
}

void NetInfo::emit_events()
{
    getifaddrs(&if_addr_list_);

	std::map<std::string, s_event_map> events;

    for (struct ifaddrs * if_addr = if_addr_list_; if_addr != nullptr; if_addr = if_addr->ifa_next) {
        if (!if_addr->ifa_addr) {
            continue;
        }
        if (if_addr->ifa_addr->sa_family == AF_INET) {
            // IPv4 Address
            void * tmp_addr_ptr=&((struct sockaddr_in *)if_addr->ifa_addr)->sin_addr;
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmp_addr_ptr, addr_str, INET_ADDRSTRLEN);
			if (interface_ == ":all" || !strcmp(interface_.c_str(), if_addr->ifa_name)) {
				add_event(if_addr->ifa_name, "ipv4", addr_str, events);
			}
        } else if (if_addr->ifa_addr->sa_family == AF_INET6) {
            // IPv6 Address
            void * tmp_addr_ptr=&((struct sockaddr_in6 *)if_addr->ifa_addr)->sin6_addr;
            char addr_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmp_addr_ptr, addr_str, INET6_ADDRSTRLEN);
			if (interface_ == ":all" || !strcmp(interface_.c_str(), if_addr->ifa_name)) {
				add_event(if_addr->ifa_name, "ipv6", addr_str, events);
			}
        } else if (if_addr->ifa_addr->sa_family == AF_PACKET) {
			struct sockaddr_ll * saddr = (struct sockaddr_ll *)(if_addr->ifa_addr);
			std::stringstream sstream;
			for (int i = 0; i < 6; i++)
				sstream << std::setfill('0') << std::setw(2) << std::hex << (uint)saddr->sll_addr[i] << (i < 5 ? ":" : "");
			if (interface_ == ":all" || !strcmp(interface_.c_str(), if_addr->ifa_name)) {
				add_event(if_addr->ifa_name, "mac", sstream.str(), events);
			}
		}
    }
    if (if_addr_list_!=nullptr) freeifaddrs(if_addr_list_);

	for (std::map<std::string, s_event_map>::iterator it = events.begin(); it != events.end(); ++it) {
		auto event = std::make_shared<yuri::event::EventDict>(it->second);
		emit_event(it->first, event);
	}
}

bool NetInfo::set_param(const core::Parameter& param)
{
	if (assign_parameters(param)
			(interval_, "interval", [](const core::Parameter& p){ return 1_s * p.get<double>();})
			(interface_, "interface"))
		return true;
	return core::IOThread::set_param(param);
}

bool NetInfo::do_process_event(const std::string& event_name, const event::pBasicEvent& event)
{
	return true;
}

} /* namespace ndi_net */
} /* namespace yuri */
