/* RFA provider.
 */

#ifndef __PROVIDER_HH__
#define __PROVIDER_HH__
#pragma once

#include <cstdint>
#include <unordered_map>

/* Circular buffer */
#include <boost/circular_buffer.hpp>

/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

/* Boost unordered map: bypass 2^19 limit in MSVC std::unordered_map */
#include <boost/unordered_map.hpp>

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* Boost threading. */
#include <boost/thread.hpp>

/* RFA 7.2 */
#include <rfa/rfa.hh>

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
	class event_t;

	class cool_t : boost::noncopyable
	{
	public:
		cool_t (const std::string& name, boost::circular_buffer<event_t>& events, boost::shared_mutex& events_lock, unsigned& event_id)
			: name_ (name),
			  is_online_ (false),
			  accumulated_failures_ (1),
			  recording_start_time_ (boost::posix_time::second_clock::universal_time()),
			  transition_time_ (recording_start_time_),
			  events_ (events),
			  events_lock_ (events_lock),
			  event_id_ (event_id)
		{
		}
		~cool_t();

		void OnRecovery();
		void OnOutage();

		bool IsOnline() const { return is_online_; }
		const std::string& GetLoginName() const { return name_; }
		boost::posix_time::time_duration GetAccumulatedOutageTime (boost::posix_time::ptime now) const;
		boost::posix_time::ptime GetLastChangeTime() const { return transition_time_; }
		uint32_t GetAccumulatedFailures() const { return accumulated_failures_; }
		boost::posix_time::ptime GetRecordingStartTime() const { return recording_start_time_; }

		double GetAvailability (boost::posix_time::ptime now) const;
		double GetMTTR (boost::posix_time::ptime now) const;
		double GetMTBF (boost::posix_time::ptime now) const;

	protected:
		std::string name_;
		bool is_online_;
		uint32_t accumulated_failures_;
		boost::posix_time::time_duration accumulated_outage_time_;
		boost::posix_time::ptime recording_start_time_, transition_time_;

/* terrible but convenient */
		boost::circular_buffer<event_t>& events_;
		boost::shared_mutex& events_lock_;
		unsigned& event_id_;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const cool_t& cool) {
		using namespace boost::posix_time;
		const auto now (second_clock::universal_time());
		const time_duration MTTR = seconds (static_cast<long> (cool.GetMTTR (now)));
		const time_duration MTBF = seconds (static_cast<long> (cool.GetMTBF (now)));
		o << "{ "
			  "\"Session\": \"" << cool.GetLoginName() << "\""
			", \"AOT\": \"" << to_simple_string (cool.GetAccumulatedOutageTime (now)) << "\""
			", \"NAF\": " << cool.GetAccumulatedFailures() <<
			", \"Availability\": \"" << std::setprecision (3) << (100.0 * cool.GetAvailability (now)) << "%\""
			", \"MTTR\": \"" << to_simple_string (MTTR) << "\""
			", \"MTBF\": \"" << to_simple_string (MTBF) << "\""
			" }";
		return o;
	}

	class event_t
	{
	public:
/* implicit */
		event_t()
			: id_ (0), start_time_ (boost::posix_time::not_a_date_time), end_time_ (boost::posix_time::not_a_date_time), is_online_ (false)
		{
		}

		explicit event_t (unsigned id, const std::string& name, boost::posix_time::ptime start_time, boost::posix_time::ptime end_time, bool is_online)
			: id_ (id), name_ (name), start_time_ (start_time), end_time_ (end_time), is_online_ (is_online)
		{
		}

/* copy ctor */
		event_t (const event_t& other)
			: id_ (other.id_), name_ (other.name_), start_time_ (other.start_time_), end_time_ (other.end_time_), is_online_ (other.is_online_)
		{
		}

/* move ctor: http://msdn.microsoft.com/en-us/library/dd293665.aspx */
		event_t (event_t&& other)
		{
			*this = std::move (other);
		}

/* copy assignment */
		event_t& operator= (const event_t& other)
		{
			if (this != &other) {
				id_ = other.id_;
				name_ = other.name_;
				start_time_ = other.start_time_;
				end_time_ = other.end_time_;
				is_online_ = other.is_online_;
			}
			return *this;
		}

/* move assignment */
		event_t& operator= (event_t&& other)
		{
			if (this != &other) {
				/* free */

				/* copy */
				id_ = other.id_;
				name_ = other.name_;
				start_time_ = other.start_time_;
				end_time_ = other.end_time_;
				is_online_ = other.is_online_;

				/* release */
				other.id_ = 0;
				other.name_.clear();
				other.start_time_ = boost::posix_time::not_a_date_time;
				other.end_time_ = boost::posix_time::not_a_date_time;
				other.is_online_ = false;
			}
			return *this;
		}

		unsigned GetIndex() const { return id_; }
		const std::string& GetLoginName() const { return name_; }
		boost::posix_time::ptime GetStartTime() const { return start_time_; }
		boost::posix_time::ptime GetEndTime() const { return end_time_; }
		boost::posix_time::time_duration GetDuration() const { return end_time_ - start_time_; }
		bool IsOnline() const { return is_online_; }

	protected:
		unsigned id_;
		std::string name_;
		boost::posix_time::ptime start_time_, end_time_;
		bool is_online_;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const event_t& event) {
		using namespace boost::posix_time;
		o << "{ "
			  "\"Index\": \"" << event.GetIndex() << "\""
			", \"State\": \"" << (event.IsOnline() ? "UP" : "DOWN") << "\""
			", \"Duration\": \"" << to_simple_string (event.GetDuration()) << "\""
			", \"StartTime\": \"" << to_simple_string (event.GetStartTime()) << "\""
			", \"EndTime\": \"" << to_simple_string (event.GetEndTime()) << "\""
			", \"Session\": \"" << event.GetLoginName() << "\""
			" }";
		return o;
	}

	class provider_t :
		public std::enable_shared_from_this<provider_t>,
		boost::noncopyable
	{
	public:
		provider_t (const config_t& config, std::shared_ptr<rfa_t> rfa, std::shared_ptr<rfa::common::EventQueue> event_queue);
		~provider_t();

		bool Init() throw (rfa::common::InvalidConfigurationException, rfa::common::InvalidUsageException);
		void Clear();

		bool CreateItemStream (const char* name, std::shared_ptr<item_stream_t> item_stream) throw (rfa::common::InvalidUsageException);
		bool Send (item_stream_t*const item_stream, rfa::message::RespMsg*const msg) throw (rfa::common::InvalidUsageException);

		uint8_t GetRwfMajorVersion() const {
			return min_rwf_major_version_;
		}
		uint8_t GetRwfMinorVersion() const {
			return min_rwf_minor_version_;
		}

		void WriteCoolTables (std::string* output);

	private:
		void GetServiceDirectory (rfa::data::Map*const map);
		void GetServiceFilterList (rfa::data::FilterList*const filterList);
		void GetServiceInformation (rfa::data::ElementList*const elementList);
		void GetServiceCapabilities (rfa::data::Array*const capabilities);
		void GetServiceDictionaries (rfa::data::Array*const dictionaries);
#ifndef SRC_DIST_REQUIRES_QOS_FIXED
		void GetDirectoryQoS (rfa::data::Array*const qos);
#endif
		void GetServiceState (rfa::data::ElementList*const elementList);

		const config_t& config_;

/* Copy of RFA context */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* Reuters Wire Format versions. */
		uint8_t min_rwf_major_version_;
		uint8_t min_rwf_minor_version_;

		std::vector<std::unique_ptr<session_t>> sessions_;

/* Container of all item streams keyed by symbol name. */
		std::unordered_map<std::string, std::weak_ptr<item_stream_t>> directory_;

		friend session_t;

/* COOL measurement: index is read-only */
		boost::unordered_map<std::string, std::shared_ptr<cool_t>> cool_;

/* COOL events */
		std::shared_ptr<boost::circular_buffer<event_t>> events_;
		boost::shared_mutex events_lock_;
		unsigned event_id_;

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
