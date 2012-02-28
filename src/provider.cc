/* RFA provider.
 *
 * One single provider, and hence wraps a RFA session for simplicity.
 * Connection events (7.4.7.4, 7.5.8.3) are ignored as they're completely
 * useless.
 *
 * Definition of overlapping terms:
 *   OMM Provider:  Underlying RFA provider object.
 *   Provider:      Application encapsulation of provider functionality.
 *   Session:       RFA session object that contains one or more "Connection"
 *                  objects for horizontal scaling, e.g. RDF, GARBAN, TOPIC3.
 *   Connection:    RFA connection object that contains one or more servers.
 *   Server List:   A list of servers with round-robin failover connectivity.
 */

#include "provider.hh"

#include <algorithm>
#include <utility>

#include "chromium/logging.hh"
#include "error.hh"
#include "rfaostream.hh"
#include "session.hh"

using rfa::common::RFA_String;

/* Reuters Wire Format nomenclature for dictionary names. */
static const RFA_String kRdmFieldDictionaryName ("RWFFld");
static const RFA_String kEnumTypeDictionaryName ("RWFEnum");

hilo::provider_t::provider_t (
	const hilo::config_t& config,
	std::shared_ptr<hilo::rfa_t> rfa,
	std::shared_ptr<rfa::common::EventQueue> event_queue
	) :
	last_activity_ (boost::posix_time::microsec_clock::universal_time()),
	config_ (config),
	min_rwf_major_version_ (0),
	min_rwf_minor_version_ (0)
{
	ZeroMemory (cumulative_stats_, sizeof (cumulative_stats_));
	ZeroMemory (snap_stats_, sizeof (snap_stats_));

	sessions_.reserve (config.sessions.size());
	unsigned i = 0;
	for (auto it = config.sessions.begin();
		it != config.sessions.end();
		++it)
	{
		std::unique_ptr<session_t> session (new session_t (*this, i++, *it, rfa, event_queue));
		sessions_.push_back (std::move (session));
	}
}

hilo::provider_t::~provider_t()
{
}

bool
hilo::provider_t::init()
{
	std::for_each (sessions_.begin(), sessions_.end(),
		[](std::unique_ptr<session_t>& it)
	{
		it->init ();
	});

/* 6.2.2.1 RFA Version Info.  The version is only available if an application
 * has acquired a Session (i.e., the Session Layer library is loaded).
 */
	LOG(INFO) << "RFA: { productVersion: \"" << rfa::common::Context::getRFAVersionInfo()->getProductVersion() << "\" }";
	return true;
}

/* Create an item stream for a given symbol name.  The Item Stream maintains
 * the provider state on behalf of the application.
 */
bool
hilo::provider_t::createItemStream (
	const char* name,
	std::shared_ptr<item_stream_t> item_stream
	)
{
	VLOG(4) << "Creating item stream for RIC \"" << name << "\".";
	item_stream->rfa_name.set (name, 0, true);
	item_stream->token.resize (sessions_.size());
	item_stream->token.shrink_to_fit();
	unsigned i = 0;
	std::for_each (sessions_.begin(), sessions_.end(),
		[&name, &item_stream, &i](std::unique_ptr<session_t>& it)
	{
		assert ((bool)it);
		assert (!item_stream->token.empty());
		it->createItemStream (name, &item_stream->token[i]);
		++i;
	});
	const std::string key (name);
	auto status = directory_.emplace (std::make_pair (key, item_stream));
	assert (true == status.second);
	assert (directory_.end() != directory_.find (key));
	DVLOG(4) << "Directory size: " << directory_.size();
	last_activity_ = boost::posix_time::microsec_clock::universal_time();
	return true;
}

/* Send an Rfa message through the pre-created item stream.
 */

bool
hilo::provider_t::send (
	item_stream_t& item_stream,
	rfa::common::Msg& msg
)
{
	unsigned i = 0;
	std::for_each (sessions_.begin(), sessions_.end(),
		[&item_stream, &msg, &i](std::unique_ptr<session_t>& it)
	{
		assert ((bool)it);
		assert (!item_stream.token.empty());
		it->send (msg, *item_stream.token[i], nullptr);
		++i;
	});
	cumulative_stats_[PROVIDER_PC_MSGS_SENT]++;
	last_activity_ = boost::posix_time::microsec_clock::universal_time();
	return true;
}

void
hilo::provider_t::getServiceDirectory (
	rfa::data::Map& map
	)
{
	rfa::data::MapWriteIterator it;
	rfa::data::MapEntry mapEntry;
	rfa::data::DataBuffer dataBuffer;
	rfa::data::FilterList filterList;
	const RFA_String serviceName (config_.service_name.c_str(), 0, false);

	map.setAssociatedMetaInfo (min_rwf_major_version_, min_rwf_minor_version_);
	it.start (map);

/* No idea ... */
	map.setKeyDataType (rfa::data::DataBuffer::StringAsciiEnum);
/* One service. */
	map.setTotalCountHint (1);

/* Service name -> service filter list */
	mapEntry.setAction (rfa::data::MapEntry::Add);
	dataBuffer.setFromString (serviceName, rfa::data::DataBuffer::StringAsciiEnum);
	mapEntry.setKeyData (dataBuffer);
	getServiceFilterList (filterList);
	mapEntry.setData (static_cast<rfa::common::Data&>(filterList));
	it.bind (mapEntry);

	it.complete();
	last_activity_ = boost::posix_time::microsec_clock::universal_time();
}

void
hilo::provider_t::getServiceFilterList (
	rfa::data::FilterList& filterList
	)
{
	rfa::data::FilterListWriteIterator it;
	rfa::data::FilterEntry filterEntry;
	rfa::data::ElementList elementList;

	filterList.setAssociatedMetaInfo (min_rwf_major_version_, min_rwf_minor_version_);
	it.start (filterList);  

/* SERVICE_INFO_ID and SERVICE_STATE_ID */
	filterList.setTotalCountHint (2);

/* SERVICE_INFO_ID */
	filterEntry.setFilterId (rfa::rdm::SERVICE_INFO_ID);
	filterEntry.setAction (rfa::data::FilterEntry::Set);
	getServiceInformation (elementList);
	filterEntry.setData (static_cast<const rfa::common::Data&>(elementList));
	it.bind (filterEntry);

/* SERVICE_STATE_ID */
	filterEntry.setFilterId (rfa::rdm::SERVICE_STATE_ID);
	filterEntry.setAction (rfa::data::FilterEntry::Set);
	getServiceState (elementList);
	filterEntry.setData (static_cast<const rfa::common::Data&>(elementList));
	it.bind (filterEntry);

	it.complete();
}

/* SERVICE_INFO_ID
 * Information about a service that does not update very often.
 */
void
hilo::provider_t::getServiceInformation (
	rfa::data::ElementList& elementList
	)
{
	rfa::data::ElementListWriteIterator it;
	rfa::data::ElementEntry element;
	rfa::data::DataBuffer dataBuffer;
	rfa::data::Array array_;
	const RFA_String serviceName (config_.service_name.c_str(), 0, false);
	const RFA_String vendorName (config_.vendor_name.c_str(), 0, false);

	elementList.setAssociatedMetaInfo (min_rwf_major_version_, min_rwf_minor_version_);
	it.start (elementList);

/* Name<AsciiString>
 * Service name. This will match the concrete service name or the service group
 * name that is in the Map.Key.
 */
	element.setName (rfa::rdm::ENAME_NAME);
	dataBuffer.setFromString (serviceName, rfa::data::DataBuffer::StringAsciiEnum);
	element.setData (dataBuffer);
	it.bind (element);

/* Vendor<AsciiString> (optional)
 * Vendor whom provides the data.
 */
//	element.setName (rfa::rdm::ENAME_VENDOR);
//	dataBuffer.setFromString (vendorName, rfa::data::DataBuffer::StringAsciiEnum);
//	element.setData (dataBuffer);
//	it.bind (element);
	
/* Capabilities<Array of UInt>
 * Array of valid MessageModelTypes that the service can provide. The UInt
 * MesageModelType is extensible, using values defined in the RDM Usage Guide
 * (1-255). Login and Service Directory are omitted from this list. This
 * element must be set correctly because RFA will only request an item from a
 * service if the MessageModelType of the request is listed in this element.
 */
	element.setName (rfa::rdm::ENAME_CAPABILITIES);
	getServiceCapabilities (array_);
	element.setData (static_cast<const rfa::common::Data&>(array_));
	it.bind (element);

/* DictionariesUsed<Array of AsciiString>
 * List of Dictionary names that may be required to process all of the data 
 * from this service. Whether or not the dictionary is required depends on 
 * the needs of the consumer (e.g. display application, caching application)
 */
	element.setName (rfa::rdm::ENAME_DICTIONARYS_USED);
	getServiceDictionaries (array_);
	element.setData (static_cast<const rfa::common::Data&>(array_));
	it.bind (element);

	it.complete();
}

/* Array of valid MessageModelTypes that the service can provide.
 * rfa::data::Array does not require version tagging according to examples.
 */
void
hilo::provider_t::getServiceCapabilities (
	rfa::data::Array& capabilities
	)
{
	rfa::data::ArrayWriteIterator it;
	rfa::data::ArrayEntry arrayEntry;
	rfa::data::DataBuffer dataBuffer;

	it.start (capabilities);

/* MarketPrice = 6 */
	dataBuffer.setUInt32 (rfa::rdm::MMT_MARKET_PRICE);
	arrayEntry.setData (dataBuffer);
	it.bind (arrayEntry);

	it.complete();
}

void
hilo::provider_t::getServiceDictionaries (
	rfa::data::Array& dictionaries
	)
{
	rfa::data::ArrayWriteIterator it;
	rfa::data::ArrayEntry arrayEntry;
	rfa::data::DataBuffer dataBuffer;

	it.start (dictionaries);

/* RDM Field Dictionary */
	dataBuffer.setFromString (kRdmFieldDictionaryName, rfa::data::DataBuffer::StringAsciiEnum);
	arrayEntry.setData (dataBuffer);
	it.bind (arrayEntry);

/* Enumerated Type Dictionary */
	dataBuffer.setFromString (kEnumTypeDictionaryName, rfa::data::DataBuffer::StringAsciiEnum);
	arrayEntry.setData (dataBuffer);
	it.bind (arrayEntry);

	it.complete();
}

/* SERVICE_STATE_ID
 * State of a service.
 */
void
hilo::provider_t::getServiceState (
	rfa::data::ElementList& elementList
	)
{
	rfa::data::ElementListWriteIterator it;
	rfa::data::ElementEntry element;
	rfa::data::DataBuffer dataBuffer;

	elementList.setAssociatedMetaInfo (min_rwf_major_version_, min_rwf_minor_version_);
	it.start (elementList);

/* ServiceState<UInt>
 * 1: Up/Yes
 * 0: Down/No
 * Is the original provider of the data responding to new requests. All
 * existing streams are left unchanged.
 */
	element.setName (rfa::rdm::ENAME_SVC_STATE);
	dataBuffer.setUInt32 (1);
	element.setData (dataBuffer);
	it.bind (element);

/* AcceptingRequests<UInt> (optional, interactive-only)
 * 1: Yes
 * 0: No
 * If the value is 0, then consuming applications should not send any new
 * requests to the service provider. (Reissues may still be sent.) If an RFA
 * application makes new requests to the service, they will be queued. All
 * existing streams are left unchanged.
 */
#if 0
	element.setName (rfa::rdm::ENAME_ACCEPTING_REQS);
	dataBuffer.setUInt32 (1);
	element.setData (dataBuffer);
	it.bind (element);
#endif

	it.complete();
}

/* eof */
