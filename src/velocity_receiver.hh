
#ifndef __TEMP_VELOCITY_RECEIVER_HH__
#define __TEMP_VELOCITY_RECEIVER_HH__

#pragma once

#include <cstdint>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace temp
{

	#define temp_debug(...) \
		do { \
			MsgLog (eMsgDebug, __LINE__, __VA_ARGS__); \
		} while (0)
	#define temp_minor(...) \
		do { \
			MsgLog (eMsgLow, __LINE__, __VA_ARGS__); \
		} while (0)
	#define temp_info(...) \
		do { \
			MsgLog (eMsgInfo, __LINE__, __VA_ARGS__); \
		} while (0)
	#define temp_warn(...) \
		do { \
			MsgLog (eMsgMedium, __LINE__, __VA_ARGS__); \
		} while (0)
	#define temp_error(...) \
		do { \
			MsgLog (eMsgHigh, __LINE__, __VA_ARGS__); \
		} while (0)
	#define temp_fatal(...) \
		do { \
			MsgLog (eMsgFatal, __LINE__, __VA_ARGS__); \
		} while (0)

	typedef std::map<uint32_t, std::unique_ptr<vpf::FlexRecData>> flexrecord_map_t;

	class velocity_receiver_t : public vpf::AbstractEventConsumer
	{
	public:

		velocity_receiver_t();
		virtual ~velocity_receiver_t();
		virtual void init (const vpf::UserPluginConfig& config);
		virtual void destroy();
		virtual void processEvent (vpf::Event* event);

	protected:

/* FlexRecord definition id => definition */
		flexrecord_map_t m_flexrecord_map;

/* Count of incoming FlexRecord events */
		uint64_t m_flexrecord_events;

/* Count of FlexRecord events with unknown identifier */
		uint64_t m_unknown_flexrecords;

/* Count of all discarded events */
		uint64_t m_discarded_events;

		void on_flexrecord (std::unique_ptr<vpf::FlexRecordEvent> event);
		void get_flexrecord_map();
	};

} /* namespace temp */

#endif /* __TEMP_VELOCITY_RECEIVER_HH__ */

/* eof */
