/* Plugin for Velocity Analytics Engine.
 */

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include "velocity_receiver.hh"

temp::velocity_receiver_t::velocity_receiver_t() :
	flexrecord_event_count (0),
	unknown_flexrecord_count (0),
	discarded_event_count (0)
{
}

temp::velocity_receiver_t::~velocity_receiver_t()
{
}

/* Plugin entry point from the Velocity Analytics Engine.
 */

void
temp::velocity_receiver_t::init (
	const vpf::UserPluginConfig& config_
	)
{
	temp_info ("init: start");

	vpf::AbstractEventConsumer::init (config_);

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
			flexrecord_event_count,
			discarded_event_count);

	vpf::AbstractEventConsumer::destroy();

	temp_info ("destroy: complete");
}

/* Analytic Engine event handler with RAII.
 */

void
temp::velocity_receiver_t::processEvent (
	vpf::Event*	event_
	)
{
	std::unique_ptr<vpf::Event> vpf_event (event_);

	if (!vpf_event->isOfType (vpf::FlexRecordEvent::getEventType())) {
		discarded_event_count++;
		return;
	}

/* move to derived class */
	std::unique_ptr<vpf::FlexRecordEvent> fr_event (static_cast<vpf::FlexRecordEvent*> (vpf_event.release()));

	processFlexRecord (std::move (fr_event));
	flexrecord_event_count++;
}

/* Incoming FlexRecord formatted update.
 */

void
temp::velocity_receiver_t::processFlexRecord (
	std::unique_ptr<vpf::FlexRecordEvent> event_
	)
{
	const uint32_t fr_id = event_->getFlexRecBase()->fDefinitionId;

/* verify we have the FlexRecord identifier to decompress the contents */
	const auto it = flexrecord_map.find (fr_id);
	if (it == flexrecord_map.end()) {
		temp_warn ("Unknown FlexRecord id %" PRIu32 " on symbol name '%s'",
			fr_id,
			event_->getSymbolName());
		unknown_flexrecord_count++;
		return;
	}

	temp_info ("symbol name=%s, flexrecord id=%" PRIu32,
		event_->getSymbolName(),
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
	     ++it)
	{
		const std::string& name = *it;
		std::unique_ptr<vpf::FlexRecData> data (new vpf::FlexRecData (name.c_str()));

		flexrecord_map.insert (make_pair (data->getDefinitionId(), std::move (data)));
	}

	temp_info ("get_flexrecord_map: complete: %u entries.", flexrecord_map.size());
}

/* eof */
