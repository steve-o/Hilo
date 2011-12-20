/* User-configurable settings.
 */

#include "config.hh"

temp::config_t::config_t() :
	field_dictionary_path ("c:/rfa7.2.0.L1.win-shared.rrg/etc/RDM/RDMFieldDictionary"),
	enumtype_dictionary_path ("c:/rfa7.2.0.L1.win-shared.rrg/etc/RDM/enumtype.def"),
	session_name ("SessionName"),
	monitor_name ("ApplicationLoggerMonitorName"),
	event_queue_name ("EventQueueName"),
	connection_name ("ConnectionName"),
	publisher_name ("PublisherName"),
	vendor_name ("VendorName"),
	adh_address ("nylabadh2"),
	adh_port ("14003"),
	service_name ("NI_VTA"),
	instance_id (/* 1 */ "<instance id>"),
	application_id (/* 256 */ "256"),
	user_name ("user1"),
	position ("")
{
}

/* eof */
