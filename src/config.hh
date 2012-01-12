/* User-configurable settings.
 *
 * NB: all strings are locale bound, RFA provides no Unicode support.
 */

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#pragma once

#include <string>
#include <vector>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace hilo
{

	struct config_t
	{
		config_t();

		bool parseDomElement (const xercesc::DOMElement* elem);
		bool parseConfigNode (const xercesc::DOMNode* node);
		bool parseSnmpNode (const xercesc::DOMNode* node);
		bool parseAgentXNode (const xercesc::DOMNode* node);
		bool parseRfaNode (const xercesc::DOMNode* node);
		bool parseServiceNode (const xercesc::DOMNode* node);
		bool parseConnectionNode (const xercesc::DOMNode* node);
		bool parseServerNode (const xercesc::DOMNode* node, const char* port);
		bool parseLoginNode (const xercesc::DOMNode* node);
		bool parseSessionNode (const xercesc::DOMNode* node);
		bool parseMonitorNode (const xercesc::DOMNode* node);
		bool parseEventQueueNode (const xercesc::DOMNode* node);
		bool parsePublisherNode (const xercesc::DOMNode* node);
		bool parseVendorNode (const xercesc::DOMNode* node);
		bool parseCrossesNode (const xercesc::DOMNode* node);
		bool parseSyntheticNode (const xercesc::DOMNode* node);
		bool parsePairNode (const xercesc::DOMNode* node);

//  SNMP implant.
		bool is_snmp_enabled;

//  Net-SNMP agent or sub-agent.
		bool is_agentx_subagent;

//  AgentX port number to connect to master agent.
		std::string agentx_socket;

//  Windows registry key path.
		std::string key;

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

//  Windows timer coalescing tolerable delay.
//  At least 32ms, corresponding to two 15.6ms platform timer interrupts.
//  Appropriate values are 10% to timer period.
//  Specify tolerable delay values and timer periods in multiples of 50 ms.
//  http://www.microsoft.com/whdc/system/pnppwr/powermgmt/TimerCoal.mspx
		std::string tolerable_delay;

//  FX High-Low reset time in UTC.
		std::string reset_time;

//  FX symbol name suffix for every publish.
		std::string suffix;

//  FX feed log file name for storing derived values.
		std::string feedlog_path;

//  FX currency cross rules.
		std::vector<std::string> rules;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const config_t& config) {
		o << "config_t: { "
			  "is_snmp_enabled: \"" << config.is_snmp_enabled << "\""
			", is_agentx_subagent: \"" << config.is_agentx_subagent << "\""
			", agentx_socket: \"" << config.agentx_socket << "\""
			", key: \"" << config.key << "\""
			", service_name: \"" << config.service_name << "\""
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
			", tolerable_delay: \"" << config.tolerable_delay << "\""
			", reset_time: \"" << config.reset_time << "\""
			", suffix: \"" << config.suffix << "\""
			", feedlog_path: \"" << config.feedlog_path << "\""
			", rules: [ ";
		for (auto it = config.rules.begin();
			it != config.rules.end();
			++it)
		{
			if (it != config.rules.begin())
				o << ", ";
			o << '"' << *it << '"';
		}
		o << " ] }";
		return o;
	}

} /* namespace hilo */

#endif /* __CONFIG_HH__ */

/* eof */
