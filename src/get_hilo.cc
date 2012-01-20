/* A basic Velocity Analytics function to calculate high and low bid and ask.
 */

#include "get_hilo.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>
#include <FlexRecReader.h>

#include "chromium/logging.hh"

/* FlexRecord Quote identifier. */
static const uint32_t kQuoteId = 40002;

/*  IN: hilo populated with symbol names.
 * OUT: hilo populated with high-low values from start to end.
 */
#if 0
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

/* lambda to function pointer is incomplete in MSVC2010, punt to the compiler to clean up. */
		auto math_func = [&it](double a, double b) -> double {
			if (MATH_OP_TIMES == it.math_op)
				return (double)(a * b);
			else if (b == 0.0)
				return b;
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
						DLOG(INFO) << "Start low=" << it.low << " high=" << it.high;
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
						DLOG(INFO) << "Start low=" << it.low << " high=" << it.high;
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
#else

namespace hilo {
	class symbol_t : boost::noncopyable
	{
	public:
		symbol_t() :
			last_bid_price (0.0),
			last_ask_price (0.0),
			is_null (true)
		{
		}

		std::shared_ptr<hilo_t> non_synthetic;
/* synthetic members */
		double last_bid_price, last_ask_price;
		bool is_null;
		std::list<std::pair<std::shared_ptr<symbol_t>, std::shared_ptr<hilo_t>>> as_first_leg_list, as_second_leg_list;
	};

#if 0
	class cache_t : boost::noncopyable
	{
	public:
		cache_t (symbol_t& symbol) :
			non_synthetic (symbol.non_synthetic),
			last_bid_price (symbol.last_bid_price),
			last_ask_price (symbol.last_ask_price),
			is_null (symbol.is_null)
		{
		}

		std::weak_ptr<hilo_t> non_synthetic;
/* synthetic members */
		double last_bid_price, last_ask_price;
		bool is_null;
		std::list<std::pair<std::shared_ptr<cache_t>, std::weak_ptr<hilo_t>>> as_first_leg_list, as_second_leg_list;
	};
#endif

} /* namespace hilo */

void
hilo::get_hilo (
	std::vector<std::shared_ptr<hilo::hilo_t>>& query,
	__time32_t	from,		/* legacy from before 2003, yay. */
	__time32_t	till
	)
{
	DLOG(INFO) << "get_hilo(from=" << from << " till=" << till << ")";

	std::unordered_map<std::string, std::shared_ptr<symbol_t>> symbol_map;
	std::set<std::string> symbol_set;
	std::set<FlexRecBinding> binding_set;

/* convert multiple queries into a single query expression */
	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t> query_it)
	{
		auto symbol_it = symbol_map.find (query_it->legs.first.symbol_name);
		if (symbol_map.end() == symbol_it) {
			std::shared_ptr<symbol_t> new_symbol (new symbol_t);
			auto status = symbol_map.emplace (std::make_pair (query_it->legs.first.symbol_name, std::move (new_symbol)));
			symbol_it = status.first;
			symbol_set.emplace (symbol_it->first);
		} 

		if (MATH_OP_NOOP == query_it->math_op) {
			symbol_it->second->non_synthetic = query_it;
			return;
		}

/* reset now */
		query_it->low = query_it->high = 0.0;

/* this ric is a synthetic pair, XxxYyy, add to list of Xxx a link to -Yyy and add to Yyy a link to Xxx- */
		assert (query_it->legs.first.symbol_name != query_it->legs.second.symbol_name);

/* Xxx */
		auto first_it = std::ref (symbol_it).get();
/* Yyy */
		auto second_it = symbol_map.find (query_it->legs.second.symbol_name);
		if (symbol_map.end() == second_it) {
			std::shared_ptr<symbol_t> new_symbol (new symbol_t);
			auto status = symbol_map.emplace (std::make_pair (query_it->legs.second.symbol_name, std::move (new_symbol)));
			second_it = status.first;
			symbol_set.emplace (second_it->first);
		} 

		first_it->second->as_first_leg_list.emplace_back (std::make_pair (second_it->second, query_it));

		second_it->second->as_second_leg_list.emplace_back (std::make_pair (first_it->second, query_it));
	});

	FlexRecReader fr;
	double bid_price, ask_price;

// temp ignore custom fids
	FlexRecBinding binding (kQuoteId);
	binding.Bind ("BidPrice", &bid_price);
	binding.Bind ("AskPrice", &ask_price);
	binding_set.insert (binding);

	auto update_non_synthetic = [&bid_price, &ask_price](hilo_t& query_item) {
		if (query_item.is_null) {
			query_item.is_null = false;
			query_item.low  = bid_price;
			query_item.high = ask_price;
			DLOG(INFO) << "Start low=" << bid_price << " high=" << ask_price;
			return;
		}
		if (bid_price < query_item.low) {
			query_item.low  = bid_price;
			DLOG(INFO) << "New low=" << bid_price;
		}
		if (ask_price > query_item.high) {
			query_item.high = ask_price;
			DLOG(INFO) << "New high=" << ask_price;
		}
	};

	auto update_synthetic = [&fr](hilo_t& query_item, symbol_t& first_leg, symbol_t& second_leg) {
		if (first_leg.is_null || second_leg.is_null)
			return;

/* lambda to function pointer is incomplete in MSVC2010, punt to the compiler to clean up. */
		auto math_func = [&query_item](double a, double b) -> double {
			if (MATH_OP_TIMES == query_item.math_op)
				return (double)(a * b);
			else if (b == 0.0)
				return b;
			else
				return (double)(a / b);
		};

		const double synthetic_bid_price = math_func (first_leg.last_bid_price, second_leg.last_bid_price);
		const double synthetic_ask_price = math_func (first_leg.last_ask_price, second_leg.last_ask_price);

		if (query_item.is_null) {
			query_item.is_null = false;
			query_item.low  = synthetic_bid_price;
			query_item.high = synthetic_ask_price;
			DLOG(INFO) << "Start low=" << synthetic_bid_price << " high=" << synthetic_ask_price;
			return;
		}

		if (synthetic_bid_price < query_item.low) {
			query_item.low  = synthetic_bid_price;
			DLOG(INFO) << "New low=" << query_item.low;
		}
		if (synthetic_ask_price > query_item.high) {
			query_item.high = synthetic_ask_price;
			DLOG(INFO) << "New high=" << query_item.high;
		}
	};

/* run one single big query */
	fr.Open (symbol_set, binding_set, from, till, 0 /* forward */, 0 /* no limit */);
	while (fr.Next()) {
		auto symbol = symbol_map[fr.GetCurrentSymbolName()];
/* non-synthetic */
		if ((bool)symbol->non_synthetic)
		{
			update_non_synthetic (*symbol->non_synthetic.get());
		}
/* synthetics */
		symbol->is_null = false;
		symbol->last_bid_price = bid_price;
		symbol->last_ask_price = ask_price;
		std::for_each (symbol->as_first_leg_list.begin(), symbol->as_first_leg_list.end(),
			[&](std::pair<std::shared_ptr<symbol_t>, std::shared_ptr<hilo_t>> second_leg)
		{
			update_synthetic (*second_leg.second.get(), *symbol.get(), *second_leg.first.get());
		});
		std::for_each (symbol->as_second_leg_list.begin(), symbol->as_second_leg_list.end(),
			[&](std::pair<std::shared_ptr<symbol_t>, std::shared_ptr<hilo_t>> first_leg)
		{
			update_synthetic (*first_leg.second.get(), *first_leg.first.get(), *symbol.get());
		});
	}	
	fr.Close();
	DLOG(INFO) << "get_hilo() finished.";
}
#endif

/* eof */