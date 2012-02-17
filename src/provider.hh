/* RFA provider.
 */

#ifndef __PROVIDER_HH__
#define __PROVIDER_HH__

#pragma once

#include <cstdint>
#include <unordered_map>

/* Boost Posix Time */
#include "boost/date_time/posix_time/posix_time.hpp"

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* RFA 7.2 */
#include <rfa.hh>

#include "rfa.hh"
#include "config.hh"
#include "deleter.hh"

namespace hilo
{
/* Performance Counters */
	enum {
		PROVIDER_PC_MSGS_SENT,
/* marker */
		PROVIDER_PC_MAX
	};

	class item_stream_t : boost::noncopyable
	{
	public:
/* Fixed name for this stream. */
		rfa::common::RFA_String rfa_name;
/* Session token which is valid from login success to login close. */
		std::vector<rfa::sessionLayer::ItemToken*> token;
	};

	class session_t;

	class provider_t :
		boost::noncopyable
	{
	public:
		provider_t (const config_t& config, std::shared_ptr<rfa_t> rfa, std::shared_ptr<rfa::common::EventQueue> event_queue);
		~provider_t();

		bool init() throw (rfa::common::InvalidConfigurationException, rfa::common::InvalidUsageException);

		bool createItemStream (const char* name, std::shared_ptr<item_stream_t> item_stream) throw (rfa::common::InvalidUsageException);
		bool send (item_stream_t& item_stream, rfa::common::Msg& msg) throw (rfa::common::InvalidUsageException);

		uint8_t getRwfMajorVersion() {
			return min_rwf_major_version_;
		}
		uint8_t getRwfMinorVersion() {
			return min_rwf_minor_version_;
		}

	private:
		void getServiceDirectory (rfa::data::Map& map);
		void getServiceFilterList (rfa::data::FilterList& filterList);
		void getServiceInformation (rfa::data::ElementList& elementList);
		void getServiceCapabilities (rfa::data::Array& capabilities);
		void getServiceDictionaries (rfa::data::Array& dictionaries);
		void getServiceState (rfa::data::ElementList& elementList);

		const config_t& config_;

/* Reuters Wire Format versions. */
		uint8_t min_rwf_major_version_;
		uint8_t min_rwf_minor_version_;

		std::vector<std::unique_ptr<session_t>> sessions_;

/* Container of all item streams keyed by symbol name. */
		std::unordered_map<std::string, std::weak_ptr<item_stream_t>> directory_;

		friend session_t;

/** Performance Counters **/
		boost::posix_time::ptime last_activity_;
		uint32_t cumulative_stats_[PROVIDER_PC_MAX];
		uint32_t snap_stats_[PROVIDER_PC_MAX];

#ifdef STITCHMIB_H
		friend Netsnmp_Node_Handler stitchPluginPerformanceTable_handler;

		friend Netsnmp_First_Data_Point stitchSessionTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point stitchSessionTable_get_next_data_point;

		friend Netsnmp_First_Data_Point stitchSessionPerformanceTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point stitchSessionPerformanceTable_get_next_data_point;
#endif /* STITCHMIB_H */
	};

} /* namespace hilo */

#endif /* __PROVIDER_HH__ */

/* eof */
