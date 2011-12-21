/* User-configurable settings.
 */

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#pragma once

#include <string>
#include <vector>

namespace hilo
{

	struct config_t
	{
		config_t();

//  TREP-RT service name, e.g. IDN_RDF.
		std::string service_name;

//  TREP-RT ADH hostname
		std::string adh_address;

//  TREP-RT ADH port, e.g. 14002
		std::string adh_port;

/* DACS application Id.  If the server authenticates with DACS, the consumer
 * application may be required to pass in a valid ApplicationId.
 * Range: "" (None) or 1-511 as an Ascii string.
 */
		std::string application_id;

/* InstanceId is used to differentiate applications running on the same host.
 * If there is more than one noninteractive provider instance running on the
 * same host, they must be set as a different value by the provider
 * application. Otherwise, the infrastructure component which the providers
 * connect to will reject a login request that has the same InstanceId value
 * and cut the connection.
 * Range: "" (None) or any Ascii string, presumably to maximum RFA_String length.
 */
		std::string instance_id;

/* DACS username, frequently non-checked and set to similar: user1.
 */
		std::string user_name;

/* DACS position, the station which the user is using.
 * Range: "" (None) or "<IPv4 address>/hostname" or "<IPv4 address>/net"
 */
		std::string position;

//  RFA session name.
		std::string session_name;

//  RFA application logger monitor name.
		std::string monitor_name;

//  RFA event queue name.
		std::string event_queue_name;

//  RFA connection name.
		std::string connection_name;

//  RFA publisher name.
		std::string publisher_name;

//  RFA vendor name.
		std::string vendor_name;

//  FX High-Low calculation and publish interval in seconds.
		std::string interval;

//  FX High-Low reset time in UTC.
		std::string reset_time;

//  FX symbol name suffix for every publish.
		std::string symbol_suffix;

//  FX feed log file name for storing derived values.
		std::string feedlog_path;

//  FX currency cross rules.
		std::vector<std::pair<std::string, std::pair<std::string, std::string>>> rules;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const config_t& config) {
		o << "config_t: { service_name: \"" << config.service_name << "\""
			", adh_address: \"" << config.adh_address << "\""
			", adh_port: \"" << config.adh_port << "\""
			", application_id: \"" << config.application_id << "\""
			", instance_id: \"" << config.instance_id << "\""
			", user_name: \"" << config.user_name << "\""
			", position: \"" << config.position << "\" }"
			", session_name: \"" << config.session_name << "\""
			", monitor_name: \"" << config.monitor_name << "\""
			", event_queue_name: \"" << config.event_queue_name << "\""
			", connection_name: \"" << config.connection_name << "\""
			", publisher_name: \"" << config.publisher_name << "\""
			", vendor_name: \"" << config.vendor_name << "\""
			", interval: \"" << config.interval << "\""
			", reset_time: \"" << config.reset_time << "\""
			", symbol_suffix: \"" << config.symbol_suffix << "\""
			", feedlog_path: \"" << config.feedlog_path << "\""
			", rules: [ ";
		for (auto it = config.rules.begin();
			it != config.rules.end();
			++it)
		{
			if (it != config.rules.begin())
				o << ", ";
			o << '"' << it->first << "\": { "
				"LOW: \"" << it->second.first << "\", "
				"HIGH: \"" << it->second.second << "\" }";
		}
		o << " ] }";
		return o;
	}

} /* namespace hilo */

#endif /* __CONFIG_HH__ */

/* eof */
