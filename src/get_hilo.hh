/* A basic Velocity Analytics function to calculate high and low bid and ask.
 */

#ifndef __GET_HILO_HH__
#define __GET_HILO_HH__

#pragma once

#include <string>
#include <vector>

/* Boost noncopyable base class. */
#include <boost/utility.hpp>

namespace hilo
{
	enum {
		MATH_OP_NOOP = 0,
		MATH_OP_TIMES,
		MATH_OP_DIVIDE
	};

	class leg_t : boost::noncopyable
	{
	public:
		std::string symbol_name;
		std::string bid_field;
		std::string ask_field;
	};

	class hilo_t : boost::noncopyable
	{
	public:
		hilo_t () :
			math_op (MATH_OP_NOOP),
			high (0.0),
			low (0.0),
			is_null (true)
		{
		}

		std::string name;
		std::pair<leg_t, leg_t> legs;
		int math_op;
		double high;
		double low;
		bool is_null;
	};

	void get_hilo (std::vector<std::shared_ptr<hilo_t>>& hilo, __time32_t start, __time32_t end);

} /* namespace hilo */

#endif /* __GET_HILO_HH__ */

/* eof */