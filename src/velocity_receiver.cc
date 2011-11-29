/* Plugin for Velocity Analytics Engine.
 */

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include "velocity_receiver.hh"

temp::velocity_receiver_t::velocity_receiver_t()
	: m_flexrecord_events (0)
	, m_discarded_events (0)
{
}

temp::velocity_receiver_t::~velocity_receiver_t()
{
}

/* Plugin entry point from the Velocity Analytics Engine.
 */

void
temp::velocity_receiver_t::init (
	const vpf::UserPluginConfig& config
	)
{
	temp_info ("init: start");

	vpf::AbstractEventConsumer::init (config);

	get_flexrecord_dictionary();

	temp_info ("init: complete");
}

/* Plugin exit point.
 */

void
temp::velocity_receiver_t::destroy()
{
	temp_info ("destroy: start");

	temp_info ("run summary: events={FlexRecord=%" PRIu64
			", discarded=%" PRIu64
			"}",
			m_flexrecord_events,
			m_discarded_events);

	vpf::AbstractEventConsumer::destroy();

	temp_info ("destroy: complete");
}

/* Analytic Engine event handler with RAII.
 */

void
temp::velocity_receiver_t::processEvent (
	vpf::Event*	pevent
	)
{
	std::unique_ptr<vpf::Event> vpf_event (pevent);

	if (!vpf_event->isOfType (vpf::FlexRecordEvent::getEventType())) {
		m_discarded_events++;
		return;
	}

	m_flexrecord_events++;

/* move to derived class */
	std::unique_ptr<vpf::FlexRecordEvent> fr_event (static_cast<vpf::FlexRecordEvent*> (vpf_event.release()));

	temp_info ("processEvent: symbol-name=%s", fr_event->getSymbolName());
}

/* Enumerate the FlexRecord dictionary from the Analytic Engine process.
 */

void
temp::velocity_receiver_t::get_flexrecord_dictionary()
{
	temp_info ("get_flexrecord_dictionary: start");
	temp_info ("get_flexrecord_dictionary: complete");
}

/* eof */
