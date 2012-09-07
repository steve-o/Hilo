/* RFA provider session.
 */

#ifndef __SESSION_HH__
#define __SESSION_HH__
#pragma once

#include <cstdint>
#include <unordered_map>

/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* RFA 7.2 */
#include <rfa/rfa.hh>

#include "rfa.hh"
#include "config.hh"
#include "deleter.hh"

namespace hilo
{
/* Performance Counters */
	enum {
		SESSION_PC_RFA_MSGS_SENT,
		SESSION_PC_RFA_EVENTS_RECEIVED,
		SESSION_PC_RFA_EVENTS_DISCARDED,
		SESSION_PC_OMM_ITEM_EVENTS_RECEIVED,
		SESSION_PC_OMM_ITEM_EVENTS_DISCARDED,
		SESSION_PC_RESPONSE_MSGS_RECEIVED,
		SESSION_PC_RESPONSE_MSGS_DISCARDED,
		SESSION_PC_MMT_LOGIN_RESPONSE_RECEIVED,
		SESSION_PC_MMT_LOGIN_RESPONSE_DISCARDED,
		SESSION_PC_MMT_LOGIN_SUCCESS_RECEIVED,
		SESSION_PC_MMT_LOGIN_SUSPECT_RECEIVED,
		SESSION_PC_MMT_LOGIN_CLOSED_RECEIVED,
		SESSION_PC_OMM_CMD_ERRORS,
		SESSION_PC_MMT_LOGIN_VALIDATED,
		SESSION_PC_MMT_LOGIN_MALFORMED,
		SESSION_PC_MMT_LOGIN_SENT,
		SESSION_PC_MMT_DIRECTORY_VALIDATED,
		SESSION_PC_MMT_DIRECTORY_MALFORMED,
		SESSION_PC_MMT_DIRECTORY_SENT,
		SESSION_PC_TOKENS_GENERATED,
/* marker */
		SESSION_PC_MAX
	};

	class provider_t;
	class cool_t;

	class session_t :
		public rfa::common::Client,
		boost::noncopyable
	{
	public:
		session_t (std::shared_ptr<provider_t> provider, const unsigned instance_id, const session_config_t& config, std::shared_ptr<rfa_t> rfa, std::shared_ptr<rfa::common::EventQueue> event_queue, std::shared_ptr<cool_t> cool);
		~session_t();

		bool Init() throw (rfa::common::InvalidConfigurationException, rfa::common::InvalidUsageException);
		bool CreateOMMProvider() throw (rfa::common::InvalidConfigurationException, rfa::common::InvalidUsageException);

		bool CreateItemStream (const char* name, rfa::sessionLayer::ItemToken** token) throw (rfa::common::InvalidUsageException);
		uint32_t Send (rfa::message::RespMsg*const msg, rfa::sessionLayer::ItemToken*const token, void* closure) throw (rfa::common::InvalidUsageException);

/* RFA event callback. */
		void processEvent (const rfa::common::Event& event) override;

		uint8_t GetRwfMajorVersion() const {
			return rwf_major_version_;
		}
		uint8_t GetRwfMinorVersion() const {
			return rwf_minor_version_;
		}

	private:
		uint32_t Submit (rfa::message::RespMsg*const msg, rfa::sessionLayer::ItemToken*const token, void* closure) throw (rfa::common::InvalidUsageException);

		void OnOMMItemEvent (const rfa::sessionLayer::OMMItemEvent& event);
                void OnRespMsg (const rfa::message::RespMsg& msg);
                void OnLoginResponse (const rfa::message::RespMsg& msg);
                void OnLoginSuccess (const rfa::message::RespMsg& msg);
                void OnLoginSuspect (const rfa::message::RespMsg& msg);
                void OnLoginClosed (const rfa::message::RespMsg& msg);
		void OnOMMCmdErrorEvent (const rfa::sessionLayer::OMMCmdErrorEvent& event);

		bool SendLoginRequest() throw (rfa::common::InvalidUsageException);
		bool SendDirectoryResponse();
		bool ResetTokens();

		std::shared_ptr<provider_t> provider_;
		const session_config_t& config_;

/* unique id per connection. */
		unsigned instance_id_;
		std::string prefix_;

/* RFA context. */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* RFA session defines one or more connections for horizontal scaling. */
		std::unique_ptr<rfa::sessionLayer::Session, internal::release_deleter> session_;

/* RFA OMM provider interface. */
		std::unique_ptr<rfa::sessionLayer::OMMProvider, internal::destroy_deleter> omm_provider_;

/* RFA Error Item event consumer */
		rfa::common::Handle* error_item_handle_;
/* RFA Item event consumer */
		rfa::common::Handle* item_handle_;

/* Reuters Wire Format versions. */
		uint8_t rwf_major_version_;
		uint8_t rwf_minor_version_;

/* RFA will return a CmdError message if the provider application submits data
 * before receiving a login success message.  Mute downstream publishing until
 * permission is granted to submit data.
 */
		bool is_muted_;

/* Last RespStatus details. */
		int stream_state_;
		int data_state_;

/* Session is registered for outage recording.
 */
		std::shared_ptr<cool_t> cool_;

/** Performance Counters **/
		boost::posix_time::ptime last_activity_;
		uint32_t cumulative_stats_[SESSION_PC_MAX];
		uint32_t snap_stats_[SESSION_PC_MAX];

#ifdef STITCHMIB_H
		friend Netsnmp_Next_Data_Point stitchSessionTable_get_next_data_point;
		friend Netsnmp_Node_Handler stitchSessionTable_handler;

		friend Netsnmp_Next_Data_Point stitchSessionPerformanceTable_get_next_data_point;
		friend Netsnmp_Node_Handler stitchSessionPerformanceTable_handler;
#endif /* STITCHMIB_H */
	};

} /* namespace hilo */

#endif /* __SESSION_HH__ */

/* eof */