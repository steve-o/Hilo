/* A basic Velocity Analytics User-Plugin to exporting a new Tcl command and
 * periodically publishing out to ADH via RFA using RDM/MarketPrice.
 */

#ifndef __STITCH_HH__
#define __STITCH_HH__
#pragma once

#include <cstdint>
#include <unordered_map>

/* Boost Posix Time */
#include "boost/date_time/posix_time/posix_time.hpp"

/* Boost noncopyable base class. */
#include <boost/utility.hpp>

/* Boost threading. */
#include <boost/thread.hpp>

/* RFA 7.2 */
#include <rfa/rfa.hh>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "chromium/logging.hh"

#include "config.hh"
#include "provider.hh"

namespace logging
{
	class LogEventProvider;
}

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
	class rfa_t;
	class provider_t;
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
		std::unordered_map<std::string, std::shared_ptr<item_stream_t>> historical;
		std::unordered_map<std::string, std::shared_ptr<item_stream_t>> chain;
	};

	class event_pump_t
	{
	public:
		event_pump_t (std::shared_ptr<rfa::common::EventQueue> event_queue) :
			event_queue_ (event_queue)
		{
		}

		void operator()()
		{
			while (event_queue_->isActive()) {
				event_queue_->dispatch (rfa::common::Dispatchable::InfiniteWait);
			}
		}

	private:
		std::shared_ptr<rfa::common::EventQueue> event_queue_;
	};

/* Periodic timer event source */
	class time_base_t
	{
	public:
		virtual bool processTimer (boost::posix_time::ptime t) = 0;
	};

	class time_pump_t
	{
	public:
		time_pump_t (boost::posix_time::ptime due_time, boost::posix_time::time_duration td, time_base_t* cb) :
			due_time_ (due_time),
			td_ (td),
			cb_ (cb)
		{
			CHECK(nullptr != cb_);
			if (due_time_.is_not_a_date_time())
				due_time_ = boost::get_system_time() + td_;
		}

		void operator()()
		{
			try {
				while (true) {
					boost::this_thread::sleep (due_time_);
					if (!cb_->processTimer (due_time_))
						break;
					due_time_ += td_;
				}
			} catch (boost::thread_interrupted const&) {
				LOG(INFO) << "Timer thread interrupted.";
			}
		}

	private:
		boost::system_time due_time_;
		boost::posix_time::time_duration td_;
		time_base_t* cb_;
	};

	class stitch_t :
		public time_base_t,
		public vpf::AbstractUserPlugin,
		public vpf::Command,
		boost::noncopyable
	{
	public:
		stitch_t();
		virtual ~stitch_t();

/* Plugin entry point. */
		virtual void init (const vpf::UserPluginConfig& config_) override;

/* Reset state suitable for recalling init(). */
		void clear();

/* Plugin termination point. */
		virtual void destroy() override;

/* Tcl entry point. */
		virtual int execute (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData) override;

/* Configured period timer entry point. */
		bool processTimer (boost::posix_time::ptime t) override;

/* Global list of all plugin instances.  AE owns pointer. */
		static std::list<stitch_t*> global_list_;
		static boost::shared_mutex global_list_lock_;

	private:

		bool parseRule (const std::string& str, hilo::hilo_t& rule);

		bool register_tcl_api (const char* id);
		bool unregister_tcl_api (const char* id);

/* Run core event loop. */
		void mainLoop();

		int tclHiloQuery (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);
		int tclFeedLogQuery (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);
		int tclRepublishQuery (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData);

		bool get_last_reset_time (__time32_t* t);
		bool get_next_interval (boost::posix_time::ptime* t);
		bool get_start_of_last_interval (__time32_t* t);
		bool get_end_of_last_interval (__time32_t* t);

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
		std::unique_ptr<snmp_agent_t> snmp_agent_;
		friend class snmp_agent_t;

#ifdef STITCHMIB_H
		friend Netsnmp_Next_Data_Point stitchPluginTable_get_next_data_point;
		friend Netsnmp_Node_Handler stitchPluginTable_handler;

		friend Netsnmp_Next_Data_Point stitchPluginPerformanceTable_get_next_data_point;
		friend Netsnmp_Node_Handler stitchPluginPerformanceTable_handler;

		friend Netsnmp_First_Data_Point stitchSessionTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point stitchSessionTable_get_next_data_point;

		friend Netsnmp_First_Data_Point stitchSessionPerformanceTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point stitchSessionPerformanceTable_get_next_data_point;
#endif /* STITCHMIB_H */

/* RFA context. */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* RFA logging */
		std::shared_ptr<logging::LogEventProvider> log_;

/* RFA provider */
		std::shared_ptr<provider_t> provider_;

/* Publish instruments. */
		std::vector<std::shared_ptr<hilo_t>> query_vector_;
		std::vector<std::shared_ptr<broadcast_stream_t>> stream_vector_;
		boost::shared_mutex query_mutex_;

/* Event pump and thread. */
		std::unique_ptr<event_pump_t> event_pump_;
		std::unique_ptr<boost::thread> event_thread_;

/* Publish fields. */
		rfa::data::FieldList fields_;

/* Thread timer. */
		std::unique_ptr<time_pump_t> timer_;
		std::unique_ptr<boost::thread> timer_thread_;

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
