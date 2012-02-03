/* A basic Velocity Analytics function to calculate high and low bid and ask.
 */

#ifndef __GET_HILO_HH__
#define __GET_HILO_HH__

#pragma once

#include <unordered_map>
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
		leg_t () :
			bid_field_idx (-1),
			ask_field_idx (-1)
		{
			clear();
		}

		void clear() {
			last_bid = last_ask = 0.0;
			is_null = true;
		}

		std::string symbol_name;
		std::string bid_field; int bid_field_idx;
		std::string ask_field; int ask_field_idx;
		double last_bid;
		double last_ask;
		bool is_null;
	};

	class hilo_t : boost::noncopyable
	{
	public:
		hilo_t () :
			math_op (MATH_OP_NOOP),
			is_synthetic (false)
		{
			clear();
		}

		void clear() {
			high = low = 0.0;
			is_null = true;
			legs.first.clear(); legs.second.clear();
		}

		std::string name;
		std::pair<leg_t, leg_t> legs;
		int math_op;
		double high;
		double low;
		bool is_null;
		bool is_synthetic;
	};

	void get_hilo (std::vector<std::shared_ptr<hilo_t>>& hilo, __time32_t start, __time32_t end);

} /* namespace hilo */

#endif /* __GET_HILO_HH__ */

/* eof */