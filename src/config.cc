/* User-configurable settings.
 */

#include "config.hh"

#include "chromium/logging.hh"

static const char* kDefaultAdhPort = "14003";

hilo::config_t::config_t() :
/* default values */
	is_snmp_enabled (false),
	is_agentx_subagent (true),
	service_name ("NI_VTA"),
	rssl_default_port (kDefaultAdhPort),
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
/* 15 mins = 900 seconds */
	interval ("900"),
/* 100ms */
	tolerable_delay ("100"),
/* 5pm EST = 10pm GMT = 22:00 UTC */
	reset_time ("22:00:00.000"),
	suffix ("VTA")
{
/* C++11 initializer lists not supported in MSVC2010 */
	rssl_servers.push_back ("localhost");

	rules.push_back ("JPYKRW=,DIV,KRW=,BidPrice,AskPrice,JPY=EBS,BidPrice,AskPrice");
	rules.push_back ("GBPHKD=,MUL,GBP=D2,BidPrice,AskPrice,HKD=D2,BidPrice,AskPrice");
	rules.push_back ("USDCAD=,EQ,CAD=D2,BidPrice,AskPrice");
	rules.push_back ("USDCAD=TOB,EQ,CAD=D2,GeneralValue1,GeneralValue3");
}

/* Minimal error handling parsing of an Xml node pulled from the
 * Analytics Engine.
 *
 * Returns true if configuration is valid, returns false on invalid content.
 */

using namespace xercesc;

/** L"" prefix is used in preference to u"" because of MSVC2010 **/

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

	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseConfigNode (nodeList->item (i)))
			return false;
	LOG(INFO) << "Parsing complete.";
	return true;
}

bool
hilo::config_t::parseConfigNode (
	const DOMNode*		node
	)
{
	const DOMElement* config = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

/* <Snmp> */
	nodeList = config->getElementsByTagName (L"Snmp");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseSnmpNode (nodeList->item (i)))
			return false;
/* <Rfa> */
	nodeList = config->getElementsByTagName (L"Rfa");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseRfaNode (nodeList->item (i)))
			return false;
/* <crosses> */
	nodeList = config->getElementsByTagName (L"crosses");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseCrossesNode (nodeList->item (i)))
			return false;
	return true;
}

/* <Snmp> */
bool
hilo::config_t::parseSnmpNode (
	const DOMNode*		node
	)
{
	const DOMElement* snmp = static_cast<const DOMElement*>(node);
	const DOMNodeList* nodeList;

/* <agentX> */
	nodeList = snmp->getElementsByTagName (L"agentX");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseAgentXNode (nodeList->item (i)))
			return false;
	this->is_snmp_enabled = true;
	return true;
}

bool
hilo::config_t::parseAgentXNode (
	const DOMNode*		node
	)
{
	const DOMElement* agentX = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* subagent="bool" */
	attr = xml.transcode (agentX->getAttribute (L"subagent"));
	if (!attr.empty())
		is_agentx_subagent = (0 == attr.compare ("true"));

/* socket="..." */
	attr = xml.transcode (agentX->getAttribute (L"socket"));
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
	const DOMElement* rfa = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* key="name" */
	attr = xml.transcode (rfa->getAttribute (L"key"));
	if (!attr.empty())
		key = attr;

/* <service> */
	nodeList = rfa->getElementsByTagName (L"service");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseServiceNode (nodeList->item (i)))
			return false;
/* <connection> */
	nodeList = rfa->getElementsByTagName (L"connection");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseConnectionNode (nodeList->item (i)))
			return false;
/* <login> */
	nodeList = rfa->getElementsByTagName (L"login");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseLoginNode (nodeList->item (i)))
			return false;
/* <session> */
	nodeList = rfa->getElementsByTagName (L"session");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseSessionNode (nodeList->item (i)))
			return false;
/* <monitor> */
	nodeList = rfa->getElementsByTagName (L"monitor");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseMonitorNode (nodeList->item (i)))
			return false;
/* <eventQueue> */
	nodeList = rfa->getElementsByTagName (L"eventQueue");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseEventQueueNode (nodeList->item (i)))
			return false;
/* <publisher> */
	nodeList = rfa->getElementsByTagName (L"publisher");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parsePublisherNode (nodeList->item (i)))
			return false;
/* <vendor> */
	nodeList = rfa->getElementsByTagName (L"vendor");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseVendorNode (nodeList->item (i)))
			return false;
	return true;
}

bool
hilo::config_t::parseServiceNode (
	const DOMNode*		node
	)
{
	const DOMElement* service = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (service->getAttribute (L"name"));
	if (attr.empty()) {
/* service name cannot be empty */
		LOG(ERROR) << "<service> node 'name' attribute not specified.";
		return false;
	}
	service_name = attr;
	return true;
}

bool
hilo::config_t::parseConnectionNode (
	const DOMNode*		node
	)
{
	const DOMElement* connection = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* name="name" */
	attr = xml.transcode (connection->getAttribute (L"name"));
	if (!attr.empty())
		connection_name = attr;
/* defaultPort="port" */
	attr = xml.transcode (connection->getAttribute (L"defaultPort"));
	const char* default_port = attr.empty() ? kDefaultAdhPort : attr.c_str();

/* reset all connections */
	rssl_servers.clear();
	
/* <server> */
	nodeList = connection->getElementsByTagName (L"server");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseServerNode (nodeList->item (i)))
			return false;
	return true;
}

bool
hilo::config_t::parseServerNode (
	const DOMNode*		node
	)
{
	const DOMElement* server = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const std::string server_text = xml.transcode (server->getTextContent());
	if (server_text.size() > 0)
		rssl_servers.push_back (server_text);
	return true;
}

bool
hilo::config_t::parseLoginNode (
	const DOMNode*		node
	)
{
	const DOMElement* login = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* applicationId="id" */
	attr = xml.transcode (login->getAttribute (L"applicationId"));
	if (!attr.empty())
		application_id = attr;
/* instanceId="id" */
	attr = xml.transcode (login->getAttribute (L"instanceId"));
	if (!attr.empty())
		instance_id = attr;
/* userName="name" */
	attr = xml.transcode (login->getAttribute (L"userName"));
	if (!attr.empty())
		user_name = attr;
/* position="..." */
	attr = xml.transcode (login->getAttribute (L"position"));
	if (!attr.empty())
		position = attr;
	return true;
}

bool
hilo::config_t::parseSessionNode (
	const DOMNode*		node
	)
{
	const DOMElement* session = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (session->getAttribute (L"name"));
	if (!attr.empty())
		session_name = attr;
	return true;
}

bool
hilo::config_t::parseMonitorNode (
	const DOMNode*		node
	)
{
	const DOMElement* monitor = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (monitor->getAttribute (L"name"));
	if (!attr.empty())
		monitor_name = attr;
	return true;
}

bool
hilo::config_t::parseEventQueueNode (
	const DOMNode*		node
	)
{
	const DOMElement* event_queue = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (event_queue->getAttribute (L"name"));
	if (!attr.empty())
		event_queue_name = attr;
	return true;
}

bool
hilo::config_t::parsePublisherNode (
	const DOMNode*		node
	)
{
	const DOMElement* publisher = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (publisher->getAttribute (L"name"));
	if (!attr.empty())
		publisher_name = attr;
	return true;
}

bool
hilo::config_t::parseVendorNode (
	const DOMNode*		node
	)
{
	const DOMElement* vendor = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* name="name" */
	attr = xml.transcode (vendor->getAttribute (L"name"));
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
	const DOMElement* crosses = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;
	std::string attr;

/* interval="seconds" */
	attr = xml.transcode (crosses->getAttribute (L"interval"));
	if (!attr.empty())
		interval = attr;
/* tolerableDelay="milliseconds" */
	attr = xml.transcode (crosses->getAttribute (L"tolerableDelay"));
	if (!attr.empty())
		tolerable_delay = attr;
/* reset="time" */
	attr = xml.transcode (crosses->getAttribute (L"reset"));
	if (!attr.empty())
		reset_time = attr;
/* suffix="text" */
	attr = xml.transcode (crosses->getAttribute (L"suffix"));
	if (!attr.empty())
		suffix = attr;

/* reset all rules */
	rules.clear();
/* <synthetic> */
	nodeList = crosses->getElementsByTagName (L"synthetic");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parseSyntheticNode (nodeList->item (i)))
			return false;
/* <pair> */
	nodeList = crosses->getElementsByTagName (L"pair");
	for (int i = 0; i < nodeList->getLength(); i++)
		if (!parsePairNode (nodeList->item (i)))
			return false;
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
	const DOMElement* synthetic = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!synthetic->hasAttributes()) {
		LOG(ERROR) << "<synthetic> node attributes not found.";
		return false;
	}
	if (!synthetic->hasChildNodes()) {
		LOG(ERROR) << "<synthetic> node empty.";
		return false;
	}

/* name="RIC" */
	const std::string name = xml.transcode (synthetic->getAttribute (L"name"));
	if (name.empty()) {
		LOG(ERROR) << "<synthetic> node 'name' attribute not specified.";
		return false;
	}

/* iterate through children */
	const DOMNode* cursor = synthetic->getFirstChild();
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
		LOG(ERROR) << "<synthetic name='" << name << "'> node math operator node not found.";
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
		LOG(ERROR) << "<synthetic name='" << name << "'> node <leg> child nodes not found.";
		return false;
	}
/* if specified both bid and ask fields must be present */
	if (cursor->hasAttributes()) {
		const DOMElement* elem = static_cast<const DOMElement*>(cursor);
		bid = xml.transcode (elem->getAttribute (L"bid"));
		ask = xml.transcode (elem->getAttribute (L"ask"));
		if ((!bid.empty() && ask.empty()) || (bid.empty() && !ask.empty())) {
			LOG(ERROR) << "<synthetic name='" << name << "'>, first <leg> node 'bid' and 'ask' attributes not specified.";
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
		LOG(ERROR) << "<synthetic name='" << name << "'> node missing second <leg> child node.";
		return false;
	}
/* fields must be present for second leg if provided for first. */
	if (cursor->hasAttributes()) {
		const DOMElement* elem = static_cast<const DOMElement*>(cursor);
		bid = xml.transcode (elem->getAttribute (L"bid"));
		ask = xml.transcode (elem->getAttribute (L"ask"));
		if ((!bid.empty() && ask.empty()) || (bid.empty() && !ask.empty())) {
			LOG(ERROR) << "<synthetic name='" << name << "'>, second <leg> node 'bid' and 'ask' attributes not specified.";
			return false;
		}
		second_leg += ',' + bid + ',' + ask;
	} else if (!bid.empty()) {
		LOG(ERROR) << "<synthetic name='" << name << "'>, second <leg> node 'bid' and 'ask' attributes required.";
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
	const DOMElement* pair = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;

	if (!pair->hasAttributes()) {
		LOG(ERROR) << "<pair> node attributes not found.";
		return false;
	}
/* name="RIC" */
	const std::string name = xml.transcode (pair->getAttribute (L"name"));
	if (name.empty()) {
		LOG(ERROR) << "<pair> node 'name' attribute not specified.";
		return false;
	}
/* src="RIC" */
	std::string src = xml.transcode (pair->getAttribute (L"src"));
	if (src.empty())
		src = name;
/* bid="field" */
/* ask="field" */
	std::string bid, ask;
	bid = xml.transcode (pair->getAttribute (L"bid"));
	ask = xml.transcode (pair->getAttribute (L"ask"));
	if ((!bid.empty() && ask.empty()) || (bid.empty() && !ask.empty())) {
		LOG(ERROR) << "<pair name='" << name << "'> node 'bid' and 'ask' attributes not specified.";
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
