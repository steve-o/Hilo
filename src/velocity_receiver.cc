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
	const uint32_t record_id = event_->getFlexRecBase()->fDefinitionId;

/* verify we have the FlexRecord identifier to decompress the contents */
	const auto it = flexrecord_map.find (record_id);
	if (it == flexrecord_map.end()) {
		temp_warn ("Unknown FlexRecord id %" PRIu32 " on symbol name '%s'",
			record_id,
			event_->getSymbolName());
		unknown_flexrecord_count++;
		return;
	}

{
const FlexRecBaseTickData* base = event_->getFlexRecBase();
vpf::FlexRecData* data = it->second.get();
char buffer[1024];

VarFieldsView* tick = data->deblobToView (event_->getFlexRecBlob());
if (nullptr == tick) {
	temp_warn ("tick is nullptr");
}

data->toString (buffer);
	temp_info ("symbol name=%s, flexrecord id=%" PRIu32 " dump=%s",
		event_->getSymbolName(),
		record_id
,buffer);
}

	const FlexRecBaseTickData* base = event_->getFlexRecBase();
	const vpf::FlexRecData* data = it->second.get();
	const VarFieldsView* fields = data->getFieldsView();
	const int fieldCount = data->getFieldCount();

	for (int i = kFRFixedFields;
		i < fieldCount;
		++i)
	{
		if (!fields[i].length) {
			temp_info ("field: %d", i);
			continue;
		}
		switch (fields[i].theType) {
		case VarFieldsEnums::t_NOTYPE:
		case VarFieldsEnums::t_NOTYPE2:
			temp_info ("field: %d, type:\"(nul)\"", i);
			break;
		case VarFieldsEnums::t_string:
			temp_info ("field: %d, type:\"string\", value=\"%s\"", i, (const char*)fields[i].data);
			break;
		case VarFieldsEnums::t_float:
			temp_info ("field: %d, type:\"float\", value=%f", i, *(const float*)fields[i].data);
			break;
		case VarFieldsEnums::t_double:
		case VarFieldsEnums::t_doubleReally:
			temp_info ("field: %d, type:\"double\", value=%g", i, *(const double*)fields[i].data);
			break;
		case VarFieldsEnums::t_s8:
			temp_info ("field: %d, type:\"int8_t\", value=%d", i, *(const int8_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_u8:
			temp_info ("field: %d, type:\"uint8_t\", value=%u", i, *(const uint8_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_s16:
			temp_info ("field: %d, type:\"int16_t\", value=%d", i, *(const int16_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_u16:
			temp_info ("field: %d, type:\"uint16_t\", value=%u", i, *(const uint16_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_s32:
			temp_info ("field: %d, type:\"int32_t\", value=%d", i, *(const int32_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_u32:
			temp_info ("field: %d, type:\"uint32_t\", value=%u", i, *(const uint32_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_s64:
			temp_info ("field: %d, type:\"int64_t\", value=%d", i, *(const int64_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_u64:
			temp_info ("field: %d, type:\"uint64_t\", value=%u", i, *(const uint64_t*)fields[i].data);
			break;
		case VarFieldsEnums::t_byte:
			temp_info ("field: %d, type:\"byte array\"", i);
			break;
		case VarFieldsEnums::t_s:
			temp_info ("field: %d, type:\"varint\"", i);
			break;
		case VarFieldsEnums::t_u:
			temp_info ("field: %d, type:\"varuint\"", i);
			break;
		case VarFieldsEnums::t_PD:
			temp_info ("field: %d, type:\"packed decimal\"", i);
			break;
		case VarFieldsEnums::t_time_t:
			temp_info ("field: %d, type:\"time_t\"", i);
			break;
		case VarFieldsEnums::t_VHTime:
		case VarFieldsEnums::t_NTTime:
			temp_info ("field: %d, type:\"FILETIME\"", i);
			break;
		case VarFieldsEnums::t_UNICODE:
			temp_info ("field: %d, type:\"unicode string\"", i);
			break;
		default:
			temp_info ("field: %d, type:\"(unknown)\"", i);
			break;
		}
			
	}
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
