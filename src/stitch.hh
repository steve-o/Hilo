/* A basic Velocity Analytics User-Plugin to exporting a new Tcl command and
 * periodically publishing out to ADH via RFA using RDM/MarketPrice.
 */

#ifndef __STITCH_HH__
#define __STITCH_HH__

#pragma once

#include <cstdint>

/* Boost noncopyable base class. */
#include <boost/utility.hpp>

/* Boost threading. */
#include <boost/thread.hpp>

/* RFA 7.2 */
#include <rfa.hh>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

/* Microsoft wrappers */
#include "microsoft/timer.hh"

#include "config.hh"
#include "rfa.hh"
#include "rfa_logging.hh"
#include "provider.hh"

namespace hilo
{
/* Performance Counters */
	enum {
		STITCH_PC_TCL_QUERY_RECEIVED,
		STITCH_PC_TIMER_QUERY_RECEIVED,
/*		STITCH_PC_LAST_ACTIVITY,*/
/*		STITCH_PC_TCL_SVC_TIME_MIN,*/
/*		STITCH_PC_TCL_SVC_TIME_MEAN,*/
/*		STITCH_PC_TCL_SVC_TIME_MAX,*/
/*		STITCH_PC_TIMER_SVC_TIME_MIN,*/
/*		STITCH_PC_TIMER_SVC_TIME_MEAN,*/
/*		STITCH_PC_TIMER_SVC_TIME_MAX,*/

/* marker */
		STITCH_PC_MAX
	};

	class hilo_t;
	class snmp_agent_t;

/* Basic state for each item stream. */
	class broadcast_stream_t : public item_stream_t
	{
	public:
		broadcast_stream_t (std::shared_ptr<hilo_t> hilo_) :
			hilo (hilo_)
		{
		}

		std::shared_ptr<hilo_t>	hilo;
		std::unordered_map<std::string, std::unique_ptr<item_stream_t>> historical;
	};

	struct flex_filter_t
	{
		double bid_price;
		double ask_price;
	};

	class event_pump_t
	{
	public:
		event_pump_t (rfa::common::EventQueue& event_queue) :
			event_queue_ (event_queue)
		{
		}

		void operator()()
		{
			while (event_queue_.isActive()) {
				event_queue_.dispatch (rfa::common::Dispatchable::InfiniteWait);
			}
		}

	private:
		rfa::common::EventQueue& event_queue_;
	};

	class stitch_t :
		public vpf::AbstractUserPlugin,
		public vpf::Command,
		boost::noncopyable
	{
	public:
		stitch_t();
		virtual ~stitch_t();

/* Plugin entry point. */
		virtual void init (const vpf::UserPluginConfig& config_);

/* Reset state suitable for recalling init(). */
		void clear();

/* Plugin termination point. */
		virtual void destroy();

/* Tcl entry point. */
		virtual int execute (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);

/* Configured period timer entry point. */
		void processTimer (void* closure);

/* Global list of all plugin instances.  AE owns pointer. */
		static std::list<stitch_t*> global_list_;
		static boost::shared_mutex global_list_lock_;

	private:

/* Run core event loop. */
		void mainLoop();

		int tclHiloQuery (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);
		int tclFeedLogQuery (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);

		void get_last_reset_time (__time32_t& t);
		void get_next_interval (FILETIME& ft);
		void get_end_of_last_interval (__time32_t& t);

/* Broadcast out message. */
		bool sendRefresh() throw (rfa::common::InvalidUsageException);

/* Unique instance number per process. */
		LONG instance_;
		static LONG volatile instance_count_;

/* Plugin Xml identifiers. */
		std::string plugin_id_, plugin_type_;

/* Application configuration. */
		config_t config_;

/* Significant failure has occurred, so ignore all runtime events flag. */
		bool is_shutdown_;

/* SNMP implant. */
		snmp_agent_t* snmp_agent_;
		friend class snmp_agent_t;

#ifdef STITCHMIB_H
		friend Netsnmp_Next_Data_Point stitchPluginTable_get_next_data_point;
		friend Netsnmp_Node_Handler stitchPluginTable_handler;
		friend Netsnmp_Next_Data_Point stitchPluginPerformanceTable_get_next_data_point;
		friend Netsnmp_Node_Handler stitchPluginPerformanceTable_handler;
#endif /* STITCHMIB_H */

/* RFA context. */
		rfa_t* rfa_;

/* RFA asynchronous event queue. */
		rfa::common::EventQueue* event_queue_;

/* RFA logging */
		logging::LogEventProvider* log_;

/* RFA provider */
		provider_t* provider_;

/* Publish instruments. */
		std::vector<std::shared_ptr<hilo_t>> query_vector_;
		std::vector<std::unique_ptr<broadcast_stream_t>> stream_vector_;

/* Event pump and thread. */
		event_pump_t* event_pump_;
		boost::thread* thread_;

/* Publish fields. */
		rfa::data::FieldList fields_;

/* Threadpool timer. */
		ms::timer* timer_;

/** Performance Counters. **/
		boost::posix_time::ptime last_activity_;
		boost::posix_time::time_duration min_tcl_time_, max_tcl_time_, total_tcl_time_;
		boost::posix_time::time_duration min_refresh_time_, max_refresh_time_, total_refresh_time_;

		uint32_t cumulative_stats_[STITCH_PC_MAX];
		uint32_t snap_stats_[STITCH_PC_MAX];
		boost::posix_time::ptime snap_time_;
	};

} /* namespace hilo */

#endif /* __STITCH_HH__ */

/* eof */
