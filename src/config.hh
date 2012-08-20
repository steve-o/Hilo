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
	struct session_config_t
	{
//  RFA session name, one session contains a horizontal scaling set of connections.
		std::string session_name;

//  RFA connection name, used for logging.
		std::string connection_name;

//  RFA publisher name, used for logging.
		std::string publisher_name;

//  TREP-RT ADH hostname or IP address.
		std::vector<std::string> rssl_servers;

//  Default TREP-RT RSSL port, e.g. 14002 (interactive), 14003 (non-interactive).
		std::string rssl_default_port;

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
	};

	struct config_t
	{
		config_t();

		bool ParseDomElement (const xercesc::DOMElement* elem);
		bool ParseConfigNode (const xercesc::DOMNode* node);
		bool ParseSnmpNode (const xercesc::DOMNode* node);
		bool ParseAgentXNode (const xercesc::DOMNode* node);
		bool ParseRfaNode (const xercesc::DOMNode* node);
		bool ParseServiceNode (const xercesc::DOMNode* node);
		bool ParseConnectionNode (const xercesc::DOMNode* node, session_config_t& session);
		bool ParseServerNode (const xercesc::DOMNode* node, std::string& server);
		bool ParsePublisherNode (const xercesc::DOMNode* node, std::string& publisher);
		bool ParseLoginNode (const xercesc::DOMNode* node, session_config_t& session);
		bool ParseSessionNode (const xercesc::DOMNode* node);
		bool ParseMonitorNode (const xercesc::DOMNode* node);
		bool ParseEventQueueNode (const xercesc::DOMNode* node);
		bool ParseVendorNode (const xercesc::DOMNode* node);
		bool ParseCrossesNode (const xercesc::DOMNode* node);
		bool ParseSyntheticNode (const xercesc::DOMNode* node);
		bool ParsePairNode (const xercesc::DOMNode* node);

		bool Validate();

//  SNMP implant.
		bool is_snmp_enabled;

//  Net-SNMP agent or sub-agent.
		bool is_agentx_subagent;

//  Net-SNMP file log target.
		std::string snmp_filelog;

//  AgentX port number to connect to master agent.
		std::string agentx_socket;

//  Windows registry key path.
		std::string key;

//  TREP-RT service name, e.g. IDN_RDF.
		std::string service_name;

//  RFA sessions comprising of session names, connection names,
//  RSSL hostname or IP address and default RSSL port, e.g. 14002, 14003.
		std::vector<session_config_t> sessions;

//  RFA application logger monitor name.
		std::string monitor_name;

//  RFA event queue name.
		std::string event_queue_name;

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

//  FX currency cross rules.
		std::vector<std::string> rules;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const session_config_t& session) {
		o << "{ "
			  "\"session_name\": \"" << session.session_name << "\""
			", \"connection_name\": \"" << session.connection_name << "\""
			", \"publisher_name\": \"" << session.publisher_name << "\""
			", \"rssl_servers\": [ ";
		for (auto it = session.rssl_servers.begin();
			it != session.rssl_servers.end();
			++it)
		{
			if (it != session.rssl_servers.begin())
				o << ", ";
			o << '"' << *it << '"';
		}
		o << " ]"
			", \"rssl_default_port\": \"" << session.rssl_default_port << "\""
			", \"application_id\": \"" << session.application_id << "\""
			", \"instance_id\": \"" << session.instance_id << "\""
			", \"user_name\": \"" << session.user_name << "\""
			", \"position\": \"" << session.position << "\""
			" }";
		return o;
	}

	inline
	std::ostream& operator<< (std::ostream& o, const config_t& config) {
		o << "config_t: { "
			  "\"is_snmp_enabled\": " << (0 == config.is_snmp_enabled ? "false" : "true") << ""
			", \"is_agentx_subagent\": " << (0 == config.is_agentx_subagent ? "false" : "true") << ""
			", \"agentx_socket\": \"" << config.agentx_socket << "\""
			", \"key\": \"" << config.key << "\""
			", \"service_name:\" \"" << config.service_name << "\""
			", \"sessions\": [";
		for (auto it = config.sessions.begin();
			it != config.sessions.end();
			++it)
		{
			if (it != config.sessions.begin())
				o << ", ";
			o << *it;
		}
		o << " ]"
			", \"monitor_name\": \"" << config.monitor_name << "\""
			", \"event_queue_name\": \"" << config.event_queue_name << "\""
			", \"vendor_name\": \"" << config.vendor_name << "\""
			", \"interval\": \"" << config.interval << "\""
			", \"tolerable_delay\": \"" << config.tolerable_delay << "\""
			", \"reset_time\": \"" << config.reset_time << "\""
			", \"suffix\": \"" << config.suffix << "\""
			", \"rules\": [ ";
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