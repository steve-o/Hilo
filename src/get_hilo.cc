/* A basic Velocity Analytics function to calculate high and low bid and ask.
 */

#include "get_hilo.hh"

#include <cstdint>
#include <forward_list>
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
 *
 * caller must reset high, low, is_null values if clean query is required,
 * existing values can be used to extended a previous query.
 */
#if 0
void
hilo::get_hilo (
	std::vector<std::shared_ptr<hilo::hilo_t>>& query,
	__time32_t	from,		/* legacy from before 2003, yay. */
	__time32_t	till
	)
{
// BUG: FlexRecReader caches last cursor binding_set, create new reader per iteration.
//	FlexRecReader fr;

	DLOG(INFO) << "get_hilo(from=" << from << " till=" << till << ")";

	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t>& it)
	{
		FlexRecReader fr;
		std::set<FlexRecBinding> binding_set;
		FlexRecBinding binding (kQuoteId);
		double bid_price, ask_price, alt_bid_price, alt_ask_price;
		bool have_alt_bid_price = false, have_alt_ask_price = false;

/* source instruments */
		std::set<std::string> symbol_set;
		symbol_set.insert (it->legs.first.symbol_name);
		binding.Bind (it->legs.first.bid_field.c_str(), &bid_price);
		binding.Bind (it->legs.first.ask_field.c_str(), &ask_price);

		if (it->is_synthetic) {
			symbol_set.insert (it->legs.second.symbol_name);
			if (it->legs.first.bid_field != it->legs.second.bid_field) {
				binding.Bind (it->legs.second.bid_field.c_str(), &alt_bid_price);
				have_alt_bid_price = true;
			}
			if (it->legs.first.ask_field != it->legs.second.ask_field) {
				binding.Bind (it->legs.second.ask_field.c_str(), &alt_ask_price);
				have_alt_ask_price = true;
			}
		}

		binding_set.insert (binding);

/* lambda to function pointer is incomplete in MSVC2010, punt to the compiler to clean up. */
		auto math_func = [&it](double a, double b) -> double {
			if (MATH_OP_TIMES == it->math_op)
				return (double)(a * b);
			else if (b == 0.0)
				return b;
			else
				return (double)(a / b);
		};

/* does this analytic update the query state */
		bool is_updated = false;

		fr.Open (symbol_set, binding_set, from, till, 0 /* forward */, 0 /* no limit */);
		if (it->is_synthetic)
		{
		LOG(INFO) << it->name << " 1st: bid=" << it->legs.first.bid_field
				      <<      " ask=" << it->legs.first.ask_field
				      << " 2nd: bid=" << it->legs.second.bid_field
				      <<      " ask=" << it->legs.second.ask_field;


			double first_leg_bid_price  = it->legs.first.last_bid,
			       first_leg_ask_price  = it->legs.first.last_ask,
			       second_leg_bid_price = it->legs.second.last_bid,
			       second_leg_ask_price = it->legs.second.last_ask;

			if (it->legs.first.is_null &&
			    it->legs.second.is_null &&
			    fr.Next())
			{
/* find first value for both legs */
				if (fr.GetCurrentSymbolName() == it->legs.first.symbol_name)
				{
					it->legs.first.is_null = false;
					do {
						first_leg_bid_price = bid_price;
						first_leg_ask_price = ask_price;
					} while (fr.Next() && fr.GetCurrentSymbolName() == it->legs.first.symbol_name);
/* follow */
					if (fr.GetCurrentSymbolName() == it->legs.second.symbol_name) {
						is_updated = true;
						second_leg_bid_price = have_alt_bid_price ? alt_bid_price : bid_price;
						second_leg_ask_price = have_alt_ask_price ? alt_ask_price : ask_price;
						it->low  = math_func (first_leg_bid_price, second_leg_bid_price);
						it->high = math_func (first_leg_ask_price, second_leg_ask_price);
						LOG(INFO) << it->name << " start low=" << it->low << " high=" << it->high;
LOG(INFO) << fr.GetCurrentSymbolName() << " bid " << bid_price << " alt-bid " << alt_bid_price << " ask " << ask_price << " alt-ask " << alt_ask_price;
					}
				}
				else
				{
					it->legs.second.is_null = false;
					do {
LOG(INFO) << fr.GetCurrentSymbolName() << " bid " << bid_price << " alt-bid " << alt_bid_price << " ask " << ask_price << " alt-ask " << alt_ask_price;
						second_leg_bid_price = have_alt_bid_price ? alt_bid_price : bid_price;
						second_leg_ask_price = have_alt_ask_price ? alt_ask_price : ask_price;
					} while (fr.Next() && fr.GetCurrentSymbolName() == it->legs.second.symbol_name);
/* follow */
					if (fr.GetCurrentSymbolName() == it->legs.first.symbol_name) {
						is_updated = true;
						first_leg_bid_price = bid_price;
						first_leg_ask_price = ask_price;
						it->low  = math_func (first_leg_bid_price, second_leg_bid_price);
						it->high = math_func (first_leg_ask_price, second_leg_ask_price);
						LOG(INFO) << it->name << " start low=" << it->low << " high=" << it->high;
					}
				}
			}

/* till end */
			while (fr.Next())
			{
				is_updated = true;

				if (fr.GetCurrentSymbolName() == it->legs.first.symbol_name) {
					first_leg_bid_price = bid_price;
					first_leg_ask_price = ask_price;
				} else {
					second_leg_bid_price = have_alt_bid_price ? alt_bid_price : bid_price;
					second_leg_ask_price = have_alt_ask_price ? alt_ask_price : ask_price;
				}
			
				const double synthetic_bid_price = math_func (first_leg_bid_price, second_leg_bid_price),
					     synthetic_ask_price = math_func (first_leg_ask_price, second_leg_ask_price);
				if (synthetic_bid_price < it->low) {
					it->low  = synthetic_bid_price;
					DLOG(INFO) << it->name << " new low=" << it->low;
				}
				if (synthetic_ask_price > it->high) {
					it->high = synthetic_ask_price;
					DLOG(INFO) << it->name << " new high=" << it->high;
				}
			}

			if (is_updated) {
				it->legs.first.is_null = it->legs.second.is_null = false;
				it->legs.first.last_bid = first_leg_bid_price;
				it->legs.first.last_ask = first_leg_ask_price;
				it->legs.second.last_bid = second_leg_bid_price;
				it->legs.second.last_ask = second_leg_ask_price;
			}
		}
		else
/* non-synthetic */
		{
			if (it->legs.first.is_null &&
			    fr.Next())
			{
				is_updated = true;
				it->low  = bid_price;
				it->high = ask_price;
				DLOG(INFO) << it->name << " start low=" << it->low << " high=" << it->high;
			}
				
			while (fr.Next())
			{
				is_updated = true;
				if (bid_price < it->low) {
					it->low  = bid_price;
					DLOG(INFO) << it->name << " new low=" << bid_price;
				}
				if (ask_price > it->high) {
					it->high = ask_price;
					DLOG(INFO) << it->name << " new high=" << ask_price;
				}
			}

			if (is_updated) {
				it->legs.first.is_null = false;
				it->legs.first.last_bid = bid_price;
				it->legs.first.last_ask = ask_price;
			}
		}
		fr.Close();

		DLOG(INFO) << "iteration complete, low=" << it->low << " high=" << it->high;
	});

	DLOG(INFO) << "get_hilo() finished.";
}
#else

namespace hilo {

	class symbol_t : boost::noncopyable
	{
	public:
		symbol_t() :
			is_null (true)
		{
		}

		symbol_t (bool is_null_) :
			is_null (is_null_)
		{
		}

		void set_last_value (size_t idx_, double value_) {
			if (idx_ >= last_value.size())
				last_value.resize (1 + idx_);
			last_value[idx_] = value_;
		}

		std::forward_list<std::shared_ptr<hilo_t>> non_synthetic_list;
/* synthetic members */
		std::vector<double> last_value;
		bool is_null;
		std::list<std::pair<std::shared_ptr<symbol_t>, std::shared_ptr<hilo_t>>> as_first_leg_list, as_second_leg_list;
	};

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
	std::unordered_map<std::string, size_t> field_map;
	std::vector<double> fields;

/* convert multiple queries into a single query expression */
	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t>& query_it)
	{
		auto symbol_it = symbol_map.find (query_it->legs.first.symbol_name);
/* add new symbol into set */
		if (symbol_map.end() == symbol_it) {
			std::shared_ptr<symbol_t> new_symbol (new symbol_t (query_it->legs.first.is_null));
			auto status = symbol_map.emplace (std::make_pair (query_it->legs.first.symbol_name, std::move (new_symbol)));
			symbol_it = status.first;
			symbol_set.emplace (symbol_it->first);
		}

/* map field into binding index and set last value cache */
		auto add_field = [&](std::string& name) -> size_t {
			auto it = field_map.find (name);
			size_t idx;
			if (field_map.end() == it) {
				idx = fields.size();
				fields.resize (idx + 1);
				field_map[name] = idx;
			} else {
				idx = it->second;
			}
			return idx;
		};

		query_it->legs.first.bid_field_idx = add_field (query_it->legs.first.bid_field);
		symbol_it->second->set_last_value (query_it->legs.first.bid_field_idx, query_it->legs.first.last_bid);

		query_it->legs.first.ask_field_idx = add_field (query_it->legs.first.ask_field);
		symbol_it->second->set_last_value (query_it->legs.first.ask_field_idx, query_it->legs.first.last_ask);

		if (!query_it->is_synthetic) {
			symbol_it->second->non_synthetic_list.push_front (query_it);
			return;
		}

/* this ric is a synthetic pair, XxxYyy, add to list of Xxx a link to -Yyy and add to Yyy a link to Xxx- */
		assert (query_it->legs.first.symbol_name != query_it->legs.second.symbol_name);

/* Xxx */
		auto first_it = std::ref (symbol_it).get();
/* Yyy */
		auto second_it = symbol_map.find (query_it->legs.second.symbol_name);
		if (symbol_map.end() == second_it) {
			std::shared_ptr<symbol_t> new_symbol (new symbol_t (query_it->legs.second.is_null));
			auto status = symbol_map.emplace (std::make_pair (query_it->legs.second.symbol_name, std::move (new_symbol)));
			second_it = status.first;
			symbol_set.emplace (second_it->first);
		}

		query_it->legs.second.bid_field_idx = add_field (query_it->legs.second.bid_field);
		second_it->second->set_last_value (query_it->legs.second.bid_field_idx, query_it->legs.second.last_bid);

		query_it->legs.second.ask_field_idx = add_field (query_it->legs.second.ask_field);
		second_it->second->set_last_value (query_it->legs.second.ask_field_idx, query_it->legs.second.last_ask);

		first_it->second->as_first_leg_list.emplace_back (std::make_pair (second_it->second, query_it));
		second_it->second->as_second_leg_list.emplace_back (std::make_pair (first_it->second, query_it));
	});

	FlexRecReader fr;
	FlexRecBinding binding (kQuoteId);

/* copy finalized bindings into new set */
	std::for_each (field_map.begin(), field_map.end(),
		[&](std::pair<const std::string, size_t>& field_pair)
	{
		binding.Bind (field_pair.first.c_str(), &fields[field_pair.second]);
	});
	binding_set.insert (binding);

	auto update_non_synthetic = [&](hilo_t& query_item, std::shared_ptr<symbol_t>& symbol) {
		const double bid_price = fields[query_item.legs.first.bid_field_idx];
		const double ask_price = fields[query_item.legs.first.ask_field_idx];
		if (query_item.is_null) {
			query_item.is_null = false;
			query_item.low  = bid_price;
			query_item.high = ask_price;
			LOG(INFO) << query_item.name << " start low=" << bid_price << " high=" << ask_price;
			return;
		}
		if (bid_price < query_item.low) {
			query_item.low  = bid_price;
			DLOG(INFO) << query_item.name << " new low=" << bid_price;
		}
		if (ask_price > query_item.high) {
			query_item.high = ask_price;
			DLOG(INFO) << query_item.name << " new high=" << ask_price;
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

		const double synthetic_bid_price = math_func (first_leg.last_value[query_item.legs.first.bid_field_idx], second_leg.last_value[query_item.legs.second.bid_field_idx]);
		const double synthetic_ask_price = math_func (first_leg.last_value[query_item.legs.first.ask_field_idx], second_leg.last_value[query_item.legs.second.ask_field_idx]);

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
		std::for_each (symbol->non_synthetic_list.begin(), symbol->non_synthetic_list.end(),
			[&](std::shared_ptr<hilo_t>& query_it)
		{
			update_non_synthetic (*query_it.get(), symbol);
		});
/* synthetics */
		symbol->is_null = false;
/* cache last value */
		for (int i = fields.size() - 1; i >= 0; i--)
			symbol->last_value[i] = fields[i];
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

/* cache symbol last values back into query vector, 1:M operation */
	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t>& query_it)
	{
		auto first_leg = symbol_map[query_it->legs.first.symbol_name];
		query_it->legs.first.is_null  = first_leg->is_null;
		query_it->legs.first.last_bid = first_leg->last_value[query_it->legs.first.bid_field_idx];
		query_it->legs.first.last_ask = first_leg->last_value[query_it->legs.first.ask_field_idx];

		if (!query_it->is_synthetic) return;

		auto second_leg = symbol_map[query_it->legs.second.symbol_name];
		query_it->legs.second.is_null  = second_leg->is_null;
		query_it->legs.second.last_bid = second_leg->last_value[query_it->legs.second.bid_field_idx];
		query_it->legs.second.last_ask = second_leg->last_value[query_it->legs.second.ask_field_idx];
	});

	DLOG(INFO) << "get_hilo() finished.";
}
#endif

/* eof */