
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


	class velocity_receiver_t : public vpf::AbstractEventConsumer
	{
	public:
		velocity_receiver_t();
		virtual ~velocity_receiver_t();
		virtual void init (const vpf::UserPluginConfig& config);
		virtual void destroy();
		virtual void processEvent (vpf::Event* event);

	protected:
		uint64_t m_flexrecord_events;
		uint64_t m_discarded_events;

		void get_flexrecord_dictionary();
	};

} /* namespace temp */

#endif /* __TEMP_VELOCITY_RECEIVER_HH__ */

/* eof */
