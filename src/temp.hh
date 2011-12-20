
#ifndef __TEMP_HH__
#define __TEMP_HH__

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

#include "config.hh"
#include "logging.hh"
#include "rfa.hh"
#include "rfa_logging.hh"
#include "provider.hh"

namespace temp
{
/* Basic state for each item stream. */
	struct broadcast_stream_t : item_stream_t
	{
		broadcast_stream_t () :
			count (0)
		{
		}

		uint64_t	count;
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

	class temp_t :
		public vpf::AbstractEventConsumer,
		boost::noncopyable
	{
	public:
		temp_t();
		virtual ~temp_t();

/* Plugin entry point. */
		virtual void init (const vpf::UserPluginConfig& config_);

/* Reset state suitable for recalling init(). */
		void clear();

/* Plugin termination point. */
		virtual void destroy();

/* Plugin broadcast callback. */
		virtual void processEvent (vpf::Event* event_);

	private:

/* Run core event loop. */
		void mainLoop();

/* Event handler for incoming FlexRecords. */
		void processFlexRecord (std::unique_ptr<vpf::FlexRecordEvent> event_);

/* Application configuration. */
		const config_t config_;

/* Significant failure has occurred, so ignore all runtime events flag */
		bool is_shutdown_;

/* Quote FlexRecord definition. */
		vpf::FlexRecData* quote_flexrecord_;

/* FlexRecord FID filter. */
		flex_filter_t filter_;
		FlexRecBinding* binding_;

/* RFA context */
		rfa_t* rfa_;

/* RFA asynchronous event queue. */
		rfa::common::EventQueue* event_queue_;

/* RFA logging */
		logging::LogEventProvider* log_;

/* RFA provider */
		provider_t* provider_;

/* Item stream. */
		broadcast_stream_t msft_stream_;

/* Event pump and thread. */
		event_pump_t* event_pump_;
		boost::thread* thread_;

/* Publish fields. */
		rfa::data::FieldList fields_;

/** Performance Counters **/
/* Count of incoming FlexRecord events. */
		uint64_t flexrecord_event_count_;

/* Count of FlexRecord events without Quote record identifier. */
		uint64_t ignored_flexrecord_count_;

/* Count of FlexRecords that could not be unpacked. */
		uint64_t corrupt_flexrecord_count_;

/* Count of all discarded events. */
		uint64_t discarded_event_count_;

	};

} /* namespace temp */

#endif /* __TEMP_HH__ */

/* eof */
