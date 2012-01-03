/* A basic Velocity Analytics function to calculate high and low bid and ask.
 */

#include "get_hilo.hh"

#include <cstdint>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>
#include <FlexRecReader.h>

#include "chromium/logging.hh"

/* FlexRecord Quote identifier. */
static const uint32_t kQuoteId = 40002;

/*  IN: hilo populated with symbol names.
 * OUT: hilo populated with high-low values from start to end.
 */
void
hilo::get_hilo (
	std::vector<std::shared_ptr<hilo::hilo_t>>& query,
	__time32_t	from,		/* legacy from before 2003, yay. */
	__time32_t	till
	)
{
	FlexRecReader fr;
	double bid_price, ask_price;

	DLOG(INFO) << "get_hilo(from=" << from << " till=" << till << ")";

	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t>& shared_it)
	{
		hilo_t& it = *shared_it.get();
		std::set<FlexRecBinding> binding_set;
		FlexRecBinding first_binding (kQuoteId), second_binding (kQuoteId);
		bool is_synthetic = false;

/* source instruments */
		std::set<std::string> symbol_set;
		symbol_set.insert (it.legs.first.symbol_name);
		first_binding.Bind (it.legs.first.bid_field.c_str(), &bid_price);
		first_binding.Bind (it.legs.first.ask_field.c_str(), &ask_price);
		binding_set.insert (first_binding);

		if (MATH_OP_NOOP != it.math_op) {
			symbol_set.insert (it.legs.second.symbol_name);
			second_binding.Bind (it.legs.second.bid_field.c_str(), &bid_price);
			second_binding.Bind (it.legs.second.ask_field.c_str(), &ask_price);
			binding_set.insert (second_binding);
			is_synthetic = true;
		}

		DLOG(INFO) << "iterating with name=" << it.name;

		auto math_func = [&it](double a, double b) -> double {
			if (MATH_OP_TIMES == it.math_op)
				return (double)(a * b);
			else
				return (double)(a / b);
		};

		fr.Open (symbol_set, binding_set, from, till, 0 /* forward */, 0 /* no limit */);
		if (is_synthetic)
		{
			double first_leg_bid_price, first_leg_ask_price,
				second_leg_bid_price, second_leg_ask_price;

			if (!fr.Next())
			{
/* nul stream */
				it.low = it.high = 0.0;
			}
			else
			{
/* find first value for both legs */
				if (fr.GetCurrentSymbolName() == it.legs.first.symbol_name)
				{
					do {
						first_leg_bid_price = bid_price;
						first_leg_ask_price = ask_price;
					} while (fr.Next() && fr.GetCurrentSymbolName() == it.legs.first.symbol_name);
/* follow */
					if (fr.GetCurrentSymbolName() == it.legs.second.symbol_name) {
						it.low  = math_func (first_leg_bid_price, second_leg_bid_price = bid_price);
						it.high = math_func (first_leg_ask_price, second_leg_ask_price = ask_price);
					} else {
/* single leg stream */
						it.low = it.high = 0.0;
					}
				}
				else
				{
					do {
						second_leg_bid_price = bid_price;
						second_leg_ask_price = ask_price;
					} while (fr.Next() && fr.GetCurrentSymbolName() == it.legs.second.symbol_name);
/* follow */
					if (fr.GetCurrentSymbolName() == it.legs.first.symbol_name) {
						it.low  = math_func (first_leg_bid_price = bid_price, second_leg_bid_price);
						it.high = math_func (first_leg_ask_price = ask_price, second_leg_ask_price);
					} else {
/* single leg stream */
						it.low = it.high = 0.0;
					}
				}
			}

/* till end */
			while (fr.Next())
			{
				if (fr.GetCurrentSymbolName() == it.legs.first.symbol_name) {
					first_leg_bid_price = bid_price;
					first_leg_ask_price = ask_price;
				} else {
					second_leg_bid_price = bid_price;
					second_leg_ask_price = ask_price;
				}
			
				const double synthetic_bid_price = math_func (first_leg_bid_price, second_leg_bid_price),
					synthetic_ask_price = math_func (first_leg_ask_price, second_leg_ask_price);
				if (synthetic_bid_price < it.low) {
					it.low  = synthetic_bid_price;
					DLOG(INFO) << "New low=" << it.low;
				}
				if (synthetic_ask_price > it.high) {
					it.high = synthetic_ask_price;
					DLOG(INFO) << "New high=" << it.high;
				}
			}
		}
		else
/* non-synthetic */
		{
			if (fr.Next()) {
				it.low  = bid_price;
				it.high = ask_price;
			} else {
/* nul stream */
				it.low = it.high = 0.0;
			}
				
			while (fr.Next())
			{
				if (bid_price < it.low) {
					it.low  = bid_price;
					DLOG(INFO) << "New low=" << bid_price;
				}
				if (ask_price > it.high) {
					it.high = ask_price;
					DLOG(INFO) << "New high=" << ask_price;
				}
			}
		}
		fr.Close();

		DLOG(INFO) << "iteration complete, low=" << it.low << " high=" << it.high;
	});

	DLOG(INFO) << "get_hilo() finished.";
}

/* eof */