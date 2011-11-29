/* Plugin for Velocity Analytics Engine.
 */

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include "velocity_receiver.hh"

temp::velocity_receiver_t::velocity_receiver_t()
	: m_flexrecord_events (0)
	, m_unknown_flexrecords (0)
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

	get_flexrecord_map();

	temp_info ("init: complete");
}

/* Plugin exit point.
 */

void
temp::velocity_receiver_t::destroy()
{
	temp_info ("destroy: start");

	temp_info ("summary: events={FlexRecord=%" PRIu64
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

/* move to derived class */
	std::unique_ptr<vpf::FlexRecordEvent> fr_event (static_cast<vpf::FlexRecordEvent*> (vpf_event.release()));

	on_flexrecord (std::move (fr_event));
	m_flexrecord_events++;
}

/* Incoming FlexRecord formatted update.
 */

void
temp::velocity_receiver_t::on_flexrecord (
	std::unique_ptr<vpf::FlexRecordEvent> event
	)
{
	const uint32_t fr_id = event->getFlexRecBase()->fDefinitionId;

/* verify we have the FlexRecord identifier to decompress the contents */
	flexrecord_map_t::const_iterator it = m_flexrecord_map.find (fr_id);
	if (it == m_flexrecord_map.end()) {
		temp_warn ("Unknown FlexRecord id %" PRIu32 " on symbol name '%s'",
			fr_id,
			event->getSymbolName());
		m_unknown_flexrecords++;
		return;
	}

	temp_info ("symbol name=%s, flexrecord id=%" PRIu32,
		event->getSymbolName(),
		fr_id);
}

/* Enumerate the FlexRecord dictionary from the Analytic Engine process.
 */

void
temp::velocity_receiver_t::get_flexrecord_map()
{
	temp_info ("get_flexrecord_map: start");

	FlexRecDefinitionManager* manager = FlexRecDefinitionManager::GetInstance (nullptr);
	std::vector<std::string> names;

	manager->GetAllDefinitionNames (names);

	for (std::vector<std::string>::const_iterator it = names.begin();
	     it != names.end();
	     it++)
	{
		const std::string& name = *it;
		std::unique_ptr<vpf::FlexRecData> data (new vpf::FlexRecData (name.c_str()));

		m_flexrecord_map.insert (make_pair (data->getDefinitionId(), std::move (data)));
	}

	temp_info ("get_flexrecord_map: complete: %u entries.", m_flexrecord_map.size());
}

/* eof */
