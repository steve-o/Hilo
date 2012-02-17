/* User-configurable settings.
 */

#include "config.hh"

#include "chromium/logging.hh"

hilo::config_t::config_t() :
/* default values */
	is_snmp_enabled (false),
	is_agentx_subagent (true)
{
/* C++11 initializer lists not supported in MSVC2010 */
}

/* Minimal error handling parsing of an Xml node pulled from the
 * Analytics Engine.
 *
 * Returns true if configuration is valid, returns false on invalid content.
 */

using namespace xercesc;

/** L"" prefix is used in preference to u"" because of MSVC2010 **/

bool
hilo::config_t::validate()
{
	if (service_name.empty()) {
		LOG(ERROR) << "Undefined service name.";
		return false;
	}
	if (sessions.empty()) {
		LOG(ERROR) << "Undefined session, expecting one or more session node.";
		return false;
	}
	for (auto it = sessions.begin();
		it != sessions.end();
		++it)
	{
		if (it->session_name.empty()) {
			LOG(ERROR) << "Undefined session name.";
			return false;
		}
		if (it->connection_name.empty()) {
			LOG(ERROR) << "Undefined connection name for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->publisher_name.empty()) {
			LOG(ERROR) << "Undefined publisher name for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->rssl_servers.empty()) {
			LOG(ERROR) << "Undefined server list for <connection name=\"" << it->connection_name << "\">.";
			return false;
		}
		if (it->application_id.empty()) {
			LOG(ERROR) << "Undefined application ID for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->instance_id.empty()) {
			LOG(ERROR) << "Undefined instance ID for <session name=\"" << it->session_name << "\">.";
			return false;
		}
		if (it->user_name.empty()) {
			LOG(ERROR) << "Undefined user name for <session name=\"" << it->session_name << "\">.";
			return false;
		}
	}
	if (monitor_name.empty()) {
		LOG(ERROR) << "Undefined monitor name.";
		return false;
	}
	if (event_queue_name.empty()) {
		LOG(ERROR) << "Undefined event queue name.";
		return false;
	}
	if (vendor_name.empty()) {
		LOG(ERROR) << "Undefined vendor name.";
		return false;
	}
	if (interval.empty()) {
		LOG(ERROR) << "Undefined interval.";
		return false;
	}
	if (reset_time.empty()) {
		LOG(ERROR) << "Undefined reset time.";
		return false;
	}
	if (rules.empty()) {
		LOG(ERROR) << "Undefined FX currency cross rules.";
		return false;
	}
	return true;
}

bool
hilo::config_t::parseDomElement (
	const DOMElement*	root
	)
{
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

	LOG(INFO) << "Parsing configuration ...";
/* Plugin configuration wrapped within a <config> node. */
	nodeList = root->getElementsByTagName (L"config");

	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseConfigNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <config> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <config> nodes found in configuration.";

	if (!validate()) {
		LOG(ERROR) << "Failed validation, malformed configuration file requires correction.";
		return false;
	}

	LOG(INFO) << "Parsing complete.";
	return true;
}

bool
hilo::config_t::parseConfigNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

/* <Snmp> */
	nodeList = elem->getElementsByTagName (L"Snmp");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseSnmpNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <Snmp> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
/* <Rfa> */
	nodeList = elem->getElementsByTagName (L"Rfa");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseRfaNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <Rfa> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <Rfa> nodes found in configuration.";
/* <crosses> */
	nodeList = elem->getElementsByTagName (L"crosses");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseCrossesNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <crosses> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <crosses> nodes found in configuration.";
	return true;
}

/* <Snmp> */
bool
hilo::config_t::parseSnmpNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	const DOMNodeList* nodeList;
	vpf::XMLStringPool xml;
	std::string attr;

/* logfile="file path" */
	attr = xml.transcode (elem->getAttribute (L"filelog"));
	if (!attr.empty())
		snmp_filelog = attr;

/* <agentX> */
	nodeList = elem->getElementsByTagName (L"agentX");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseAgentXNode (nodeList->item (i))) {
			vpf::XMLStringPool xml;
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <agentX> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	this->is_snmp_enabled = true;
	return true;
}

bool
hilo::config_t::parseAgentXNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* subagent="bool" */
	attr = xml.transcode (elem->getAttribute (L"subagent"));
	if (!attr.empty())
		is_agentx_subagent = (0 == attr.compare ("true"));

/* socket="..." */
	attr = xml.transcode (elem->getAttribute (L"socket"));
	if (!attr.empty())
		agentx_socket = attr;
	return true;
}

/* </Snmp> */

/* <Rfa> */
bool
hilo::config_t::parseRfaNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* key="name" */
	attr = xml.transcode (elem->getAttribute (L"key"));
	if (!attr.empty())
		key = attr;

/* <service> */
	nodeList = elem->getElementsByTagName (L"service");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseServiceNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <service> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <service> nodes found in configuration.";
/* <session> */
	nodeList = elem->getElementsByTagName (L"session");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseSessionNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <session> nth-node #" << (1 + i) << ".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <session> nodes found, RFA behaviour is undefined without a server list.";
/* <monitor> */
	nodeList = elem->getElementsByTagName (L"monitor");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseMonitorNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <monitor> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
/* <eventQueue> */
	nodeList = elem->getElementsByTagName (L"eventQueue");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseEventQueueNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <eventQueue> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
/* <vendor> */
	nodeList = elem->getElementsByTagName (L"vendor");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseVendorNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <vendor> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	return true;
}

bool
hilo::config_t::parseServiceNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (attr.empty()) {
/* service name cannot be empty */
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
	service_name = attr;
	return true;
}

bool
hilo::config_t::parseSessionNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	session_config_t session;

/* name="name" */
	session.session_name = xml.transcode (elem->getAttribute (L"name"));
	if (session.session_name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}

/* <publisher> */
	nodeList = elem->getElementsByTagName (L"publisher");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parsePublisherNode (nodeList->item (i), session.publisher_name)) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <publisher> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
/* <connection> */
	nodeList = elem->getElementsByTagName (L"connection");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseConnectionNode (nodeList->item (i), session)) {
			LOG(ERROR) << "Failed parsing <connection> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <connection> nodes found, RFA behaviour is undefined without a server list.";
/* <login> */
	nodeList = elem->getElementsByTagName (L"login");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseLoginNode (nodeList->item (i), session)) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <login> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <login> nodes found in configuration.";	
		
	sessions.push_back (session);
	return true;
}

bool
hilo::config_t::parseConnectionNode (
	const DOMNode*		node,
	session_config_t&	session
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

/* name="name" */
	session.connection_name = xml.transcode (elem->getAttribute (L"name"));
	if (session.connection_name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
/* defaultPort="port" */
	session.rssl_default_port = xml.transcode (elem->getAttribute (L"defaultPort"));

/* <server> */
	nodeList = elem->getElementsByTagName (L"server");
	for (int i = 0; i < nodeList->getLength(); i++) {
		std::string server;
		if (!parseServerNode (nodeList->item (i), server)) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <server> nth-node #" << (1 + i) << ": \"" << text_content << "\".";			
			return false;
		}
		session.rssl_servers.push_back (server);
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <server> nodes found, RFA behaviour is undefined without a server list.";

	return true;
}

bool
hilo::config_t::parseServerNode (
	const DOMNode*		node,
	std::string&		server
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	server = xml.transcode (elem->getTextContent());
	if (server.size() == 0) {
		LOG(ERROR) << "Undefined hostname or IPv4 address.";
		return false;
	}
	return true;
}

bool
hilo::config_t::parseLoginNode (
	const DOMNode*		node,
	session_config_t&	session
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

/* applicationId="id" */
	session.application_id = xml.transcode (elem->getAttribute (L"applicationId"));
/* instanceId="id" */
	session.instance_id = xml.transcode (elem->getAttribute (L"instanceId"));
/* userName="name" */
	session.user_name = xml.transcode (elem->getAttribute (L"userName"));
/* position="..." */
	session.position = xml.transcode (elem->getAttribute (L"position"));
	return true;
}

bool
hilo::config_t::parseMonitorNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (!attr.empty())
		monitor_name = attr;
	return true;
}

bool
hilo::config_t::parseEventQueueNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (!attr.empty())
		event_queue_name = attr;
	return true;
}

bool
hilo::config_t::parsePublisherNode (
	const DOMNode*		node,
	std::string&		name
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

/* name="name" */
	name = xml.transcode (elem->getAttribute (L"name"));
	return true;
}

bool
hilo::config_t::parseVendorNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (elem->getAttribute (L"name"));
	if (!attr.empty())
		vendor_name = attr;
	return true;
}

/* </Rfa> */

/* <crosses> */
bool
hilo::config_t::parseCrossesNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* interval="seconds" */
	attr = xml.transcode (elem->getAttribute (L"interval"));
	if (!attr.empty())
		interval = attr;
/* tolerableDelay="milliseconds" */
	attr = xml.transcode (elem->getAttribute (L"tolerableDelay"));
	if (!attr.empty())
		tolerable_delay = attr;
/* reset="time" */
	attr = xml.transcode (elem->getAttribute (L"reset"));
	if (!attr.empty())
		reset_time = attr;
/* suffix="text" */
	attr = xml.transcode (elem->getAttribute (L"suffix"));
	if (!attr.empty())
		suffix = attr;

/* reset all rules */
	rules.clear();
/* <synthetic> */
	nodeList = elem->getElementsByTagName (L"synthetic");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parseSyntheticNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <synthetic> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <synthetic> nodes found.";
/* <pair> */
	nodeList = elem->getElementsByTagName (L"pair");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!parsePairNode (nodeList->item (i))) {
			const std::string text_content = xml.transcode (nodeList->item (i)->getTextContent());
			LOG(ERROR) << "Failed parsing <pair> nth-node #" << (1 + i) << ": \"" << text_content << "\".";
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <pair> nodes found.";
	return true;
}

/* Convert Xml node from:
 *
 *	<synthetic name="JPYKRW=">
 *		<divide/>
 *		<leg>JPY=</leg>
 *		<leg>KRW=</leg>
 *	</synthetic>
 *
 * into:
 *
 *	"JPYKRW=,DIV,KRW=,JPY=EBS"
 *	"JPYKRW=,DIV,KRW=,BidPrice,AskPrice,JPY=EBS,BidPrice,AskPrice"
 */

bool
hilo::config_t::parseSyntheticNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!elem->hasAttributes()) {
		LOG(ERROR) << "No attributes found, a \"name\" attribute is required.";
		return false;
	}
	if (!elem->hasChildNodes()) {
		LOG(ERROR) << "No child nodes found, a math operator node and two <leg> nodes are required.";
		return false;
	}

/* name="RIC" */
	const std::string name = xml.transcode (elem->getAttribute (L"name"));
	if (name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}

/* iterate through children */
	const DOMNode* cursor = elem->getFirstChild();
/* math operator */
	std::string math_op;
	while (nullptr != cursor) {
		if (DOMNode::ELEMENT_NODE == cursor->getNodeType()) {
			const std::string node_name = xml.transcode (cursor->getNodeName());
			if (node_name == "times") {
				math_op = "MUL";
				break;
			} else if (node_name == "divide") {
				math_op = "DIV";
				break;
			}
		}
		cursor = cursor->getNextSibling();
	}
	if (math_op.empty()) {
		LOG(ERROR) << "Malformed math operator, a <times> or <divide> node is expected.";
		return false;
	}
/* first leg */
	std::string first_leg, bid, ask;
	while (nullptr != cursor) {
		if (DOMNode::ELEMENT_NODE == cursor->getNodeType()) {
			const std::string node_name = xml.transcode (cursor->getNodeName());
			if (node_name == "leg") {
				first_leg = xml.transcode (cursor->getTextContent());
				break;
			}
		}
		cursor = cursor->getNextSibling();
	}
	if (first_leg.empty()) {
		LOG(ERROR) << "Undefined first leg.";
		return false;
	}
/* if specified both bid and ask fields must be present */
	if (cursor->hasAttributes()) {
		const DOMElement* elem = static_cast<const DOMElement*>(cursor);
		bid = xml.transcode (elem->getAttribute (L"bid"));
		ask = xml.transcode (elem->getAttribute (L"ask"));
		if ((!bid.empty() && ask.empty()) || (bid.empty() && !ask.empty())) {
			LOG(ERROR) << "Bid and ask fields must both be present.";
			return false;
		}
		first_leg += ',' + bid + ',' + ask;
	}
/* second leg */
	std::string second_leg;
	if (nullptr != cursor)
		cursor = cursor->getNextSibling();
	while (nullptr != cursor) {
		if (DOMNode::ELEMENT_NODE == cursor->getNodeType()) {
			const std::string node_name = xml.transcode (cursor->getNodeName());
			if (node_name == "leg") {
				second_leg = xml.transcode (cursor->getTextContent());
				break;
			}
		}
		cursor = cursor->getNextSibling();
	}
	if (second_leg.empty()) {
		LOG(ERROR) << "Undefined second leg.";
		return false;
	}
/* fields must be present for second leg if provided for first. */
	if (cursor->hasAttributes()) {
		const DOMElement* elem = static_cast<const DOMElement*>(cursor);
		bid = xml.transcode (elem->getAttribute (L"bid"));
		ask = xml.transcode (elem->getAttribute (L"ask"));
		if ((!bid.empty() && ask.empty()) || (bid.empty() && !ask.empty())) {
			LOG(ERROR) << "Bid and ask fields must both be present.";
			return false;
		}
		second_leg += ',' + bid + ',' + ask;
	} else if (!bid.empty()) {
		LOG(ERROR) << "Bid and ask fields must be present for both legs.";
		return false;
	}

/* add rule. */
	const std::string new_rule = name + ',' + math_op + ',' + first_leg + ',' + second_leg;
	rules.push_back (new_rule);

	return true;
}

/* Convert Xml node from:
 *
 *	<pair name="AUDUSD=" src="AUD="/>
 *
 * into:
 *
 *	"AUDUSD=,AUD="
 *	"AUDUSD=,EQ,AUD=,BidPrice,AskPrice"
 */

bool
hilo::config_t::parsePairNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!elem->hasAttributes()) {
		LOG(ERROR) << "No attributes found, \"name\" and \"src\" attributes are required.";
		return false;
	}
/* name="RIC" */
	const std::string name = xml.transcode (elem->getAttribute (L"name"));
	if (name.empty()) {
		LOG(ERROR) << "Undefined \"name\" attribute, value cannot be empty.";
		return false;
	}
/* src="RIC" */
	std::string src = xml.transcode (elem->getAttribute (L"src"));
	if (src.empty())
		src = name;
/* bid="field" */
/* ask="field" */
	std::string bid, ask;
	bid = xml.transcode (elem->getAttribute (L"bid"));
	ask = xml.transcode (elem->getAttribute (L"ask"));
	if ((!bid.empty() && ask.empty()) || (bid.empty() && !ask.empty())) {
		LOG(ERROR) << "Bid and ask fields must both be present.";
		return false;
	}
/* add rule. */
	const std::string new_rule = bid.empty() ? (name + ',' + src) : (name + ",EQ," + src + ',' + bid + ',' + ask);
	rules.push_back (new_rule);

	return true;
}
	
/* </crosses> */
/* </config> */

/* eof */
