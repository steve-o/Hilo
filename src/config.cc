/* User-configurable settings.
 */

#include "config.hh"

hilo::config_t::config_t() :
/* default values */
	service_name ("NI_VTA"),
	adh_address ("nylabadh2"),
	adh_port ("14003"),
	application_id ("256"),
	instance_id ("Instance1"),
	user_name ("user1"),
	position (""),
	session_name ("SessionName"),
	monitor_name ("ApplicationLoggerMonitorName"),
	event_queue_name ("EventQueueName"),
	connection_name ("ConnectionName"),
	publisher_name ("PublisherName"),
	vendor_name ("VendorName"),
/* 10 mins = 600 seconds */
	interval ("600"),
/* 5pm EST = 10pm GMT = 22:00 UTC */
	reset_time ("2200"),
	symbol_suffix ("VTA"),
	feedlog_path ("C:/Vhayu/Feeds/Derived")
{
/* C++11 initializer lists not supported in MSVC2010 */
	using std::make_pair;
	rules.push_back (make_pair ("JPYKRW=", make_pair ("JPY=EBS.BID/KRW=.BID", "JPY=EBS.ASK/KRW=.ASK")));
	rules.push_back (make_pair ("GBPHKD=", make_pair ("GBP=D2.BID*HKD=D2.BID", "GBP=D2.ASK*HKD=D2.ASK")));
	rules.push_back (make_pair ("HKDTHB=", make_pair ("HKDTHB=.BID", "HKDTHB=.ASK")));
	rules.push_back (make_pair ("USDCAD=", make_pair ("CAD=D2.GEN_VAL1", "CAD=D2.GEN_VAL3")));
}

/* eof */
