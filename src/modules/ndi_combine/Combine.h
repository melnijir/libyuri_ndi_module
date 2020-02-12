/*!
 * @file 		Combine.h
 * @author 		Zdenek Travnicek
 * @date		7.3.2013
 * @copyright	Institute of Intermedia, CTU in Prague, 2013
 * 				Distributed under modified BSD Licence, details in file doc/LICENSE
 *
 */

#ifndef COMBINE_H_
#define COMBINE_H_

#include "yuri/core/thread/MultiIOFilter.h"
#include <vector>
namespace yuri {
namespace ndi_combine {

class Combine: public core::MultiIOFilter
{
	using base_type = core::MultiIOFilter;
public:
	IOTHREAD_GENERATOR_DECLARATION
	static core::Parameters configure();
	Combine(log::Log &log_, core::pwThreadBase parent, const core::Parameters &parameters);
	virtual ~Combine() noexcept;
private:
	virtual std::vector<core::pFrame> do_single_step(std::vector<core::pFrame> frames) override;
	virtual bool set_param(const core::Parameter& param) override;
	size_t x_,y_;
	timestamp_t next_time_;
	float fps_;

};

} /* namespace ndi_combine */
} /* namespace yuri */
#endif /* DUMMYMODULE_H_ */
