/* A basic Velocity Analytics User-Plugin to exporting a new Tcl command and
 * periodically publishing out to ADH via RFA using RDM/MarketPrice.
 */

#include "stitch.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

/* Boost Posix Time */
#include <boost/date_time/gregorian/gregorian_types.hpp>

#include <windows.h>
#include <tchar.h>

#include "chromium/logging.hh"
#include "microsoft/unique_handle.hh"
#include "get_hilo.hh"
#include "snmp_agent.hh"
#include "error.hh"
#include "rfa_logging.hh"
#include "rfaostream.hh"
#include "version.hh"
#include "bnymellon.hh"

/* RDM Usage Guide: Section 6.5: Enterprise Platform
 * For future compatibility, the DictionaryId should be set to 1 by providers.
 * The DictionaryId for the RDMFieldDictionary is 1.
 */
static const int kDictionaryId = 1;

/* RDM: Absolutely no idea. */
static const int kFieldListId = 3;

/* RDF direct limit on symbol list entries */
static const unsigned kSymbolListLimit = 150;

/* RDM FIDs. */
static const int kRdmTimeOfUpdateId	= 5;
static const int kRdmTodaysHighId	= 12;
static const int kRdmTodaysLowId	= 13;
static const int kRdmActiveDateId	= 17;

static const int kRdmReferenceCountId	= 239;
static const int kRdmLinkId[]		= { 800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 810, 811, 812, 813 };
static const int kRdmNextLinkId		= 815;
static const int kRdmPreviousLinkId	= 814;

/* FlexRecord Quote identifier. */
static const uint32_t kQuoteId = 40002;

/* Default FlexRecord fields. */
static const char* kDefaultBidField = "BidPrice";
static const char* kDefaultAskField = "AskPrice";

/* http://en.wikipedia.org/wiki/Unix_epoch */
static const boost::gregorian::date kUnixEpoch (1970, 1, 1);

LONG volatile hilo::stitch_t::instance_count_ = 0;

std::list<hilo::stitch_t*> hilo::stitch_t::global_list_;
boost::shared_mutex hilo::stitch_t::global_list_lock_;

using rfa::common::RFA_String;

/* Convert Posix time to Unix Epoch time.
 */
template< typename TimeT >
inline
TimeT
to_unix_epoch (
	const boost::posix_time::ptime t
	)
{
	return (t - boost::posix_time::ptime (kUnixEpoch)).total_seconds();
}

bool
hilo::stitch_t::parseRule (
	const std::string&	str,
	hilo::hilo_t&	rule
	)
{
	std::vector<std::string> v;

	DLOG(INFO) << "param = [" << str << "]";

	const string::size_type size = str.size(); 
	std::string::size_type pos1 = 0; 
	while (pos1 < size) { 
		std::string::size_type pos2 = str.find_first_of (",", pos1); 
		if (pos2 == std::string::npos) pos2 = size; 
		v.push_back (str.substr (pos1, pos2 - pos1)); 
		pos1 = pos2 + 1; 
	}

	DLOG(INFO) << "#" << v.size() << " tokens";

	rule.name			= v[0];
	rule.legs.first.symbol_name	= v[0];
	rule.legs.first.bid_field	= kDefaultBidField;
	rule.legs.first.ask_field	= kDefaultAskField;
	rule.legs.second.symbol_name	= v[0];
	rule.legs.second.bid_field	= kDefaultBidField;
	rule.legs.second.ask_field	= kDefaultAskField;
	rule.math_op			= hilo::MATH_OP_NOOP;

	switch (v.size()) {
/* seven fields -> name, operator, RIC, bid, ask, RIC, bid, ask */
	case 8:
		rule.is_synthetic		= true;
		rule.math_op			= v[1] == "MUL" ? hilo::MATH_OP_TIMES : hilo::MATH_OP_DIVIDE;
		rule.legs.first.symbol_name	= v[2];
		rule.legs.first.bid_field	= v[3];
		rule.legs.first.ask_field	= v[4];
		rule.legs.second.symbol_name	= v[5];
		rule.legs.second.bid_field	= v[6];
		rule.legs.second.ask_field	= v[7];
		break;
/* five fields -> name, EQ, RIC, bid, ask */
	case 5:
		rule.is_synthetic		= false;
		rule.legs.first.symbol_name	= v[2];
		rule.legs.first.bid_field	= v[3];
		rule.legs.first.ask_field	= v[4];
		rule.legs.second.symbol_name	= rule.legs.first.symbol_name;
		rule.legs.second.bid_field	= rule.legs.first.bid_field;
		rule.legs.second.ask_field	= rule.legs.first.ask_field;
		break;

/* four fields -> name, operator, RIC, RIC */
	case 4:
		rule.is_synthetic		= true;
		rule.math_op			= v[1] == "MUL" ? hilo::MATH_OP_TIMES : hilo::MATH_OP_DIVIDE;
		rule.legs.first.symbol_name	= v[2];
		rule.legs.second.symbol_name	= v[3];
		break;

/* two fields -> name, RIC */
	case 2:
		rule.is_synthetic		= false;
		rule.legs.first.symbol_name	= rule.legs.second.symbol_name = v[1];
		break;

/* one field -> name = RIC */
	case 1:
		rule.is_synthetic		= false;
		break;

	default:
		return false;
	}

	return true;
}

hilo::stitch_t::stitch_t() :
	is_shutdown_ (false),
	last_activity_ (boost::posix_time::microsec_clock::universal_time()),
	min_tcl_time_ (boost::posix_time::pos_infin),
	max_tcl_time_ (boost::posix_time::neg_infin),
	total_tcl_time_ (boost::posix_time::seconds(0)),
	min_refresh_time_ (boost::posix_time::pos_infin),
	max_refresh_time_ (boost::posix_time::neg_infin),
	total_refresh_time_ (boost::posix_time::seconds(0))
{
	ZeroMemory (cumulative_stats_, sizeof (cumulative_stats_));
	ZeroMemory (snap_stats_, sizeof (snap_stats_));

/* Unique instance number, never decremented. */
	instance_ = InterlockedExchangeAdd (&instance_count_, 1L);

	boost::unique_lock<boost::shared_mutex> (global_list_lock_);
	global_list_.push_back (this);
}

hilo::stitch_t::~stitch_t()
{
/* Remove from list before clearing. */
	boost::unique_lock<boost::shared_mutex> (global_list_lock_);
	global_list_.remove (this);

	clear();
}

/* Plugin entry point from the Velocity Analytics Engine.
 */

void
hilo::stitch_t::init (
	const vpf::UserPluginConfig& vpf_config
	)
{
/* Thunk to VA user-plugin base class. */
	vpf::AbstractUserPlugin::init (vpf_config);

/* Save copies of provided identifiers. */
	plugin_id_.assign (vpf_config.getPluginId());
	plugin_type_.assign (vpf_config.getPluginType());
	LOG(INFO) << "{ "
		  "\"pluginType\": \"" << plugin_type_ << "\""
		", \"pluginId\": \"" << plugin_id_ << "\""
		", \"instance\": " << instance_ <<
		", \"version\": \"" << version_major << '.' << version_minor << '.' << version_build << "\""
		", \"build\": { "
			  "\"date\": \"" << build_date << "\""
			", \"time\": \"" << build_time << "\""
			", \"system\": \"" << build_system << "\""
			", \"machine\": \"" << build_machine << "\""
			" }"
		" }";

	if (!config_.parseDomElement (vpf_config.getXmlConfigData()))
		goto cleanup;

	LOG(INFO) << config_;

/** RFA initialisation. **/
	try {
/* RFA context. */
		rfa_.reset (new rfa_t (config_));
		if (!(bool)rfa_ || !rfa_->init())
			goto cleanup;

/* RFA asynchronous event queue. */
		const RFA_String eventQueueName (config_.event_queue_name.c_str(), 0, false);
		event_queue_.reset (rfa::common::EventQueue::create (eventQueueName), std::mem_fun (&rfa::common::EventQueue::destroy));
		if (!(bool)event_queue_)
			goto cleanup;

/* RFA logging. */
		log_.reset (new logging::LogEventProvider (config_, event_queue_));
		if (!(bool)log_ || !log_->Register())
			goto cleanup;

/* RFA provider. */
		provider_.reset (new provider_t (config_, rfa_, event_queue_));
		if (!(bool)provider_ || !provider_->init())
			goto cleanup;

/* Create state for published instruments. */
		for (auto it = config_.rules.begin();
			it != config_.rules.end();
			++it)
		{
/* rule */
			VLOG(3) << "Parsing rule \"" << *it << "\"";
			auto rule = std::make_shared<hilo_t> ();
			assert ((bool)rule);
			if (!parseRule (*it, *rule.get()))
				goto cleanup;
			query_vector_.push_back (rule);

/* analytic publish stream */
			std::string symbol_name = rule->name + config_.suffix;
			auto stream = std::make_shared<broadcast_stream_t> (rule);
			assert ((bool)stream);
			if (!provider_->createItemStream (symbol_name.c_str(), stream))
				goto cleanup;

/* streams for historical analytics */
			using namespace boost::posix_time;
			const time_duration reset_tod = duration_from_string (config_.reset_time);
			const ptime start (kUnixEpoch, reset_tod);
			ptime end (kUnixEpoch, reset_tod);
			end += boost::gregorian::days (1);

			const int interval_seconds = std::stoi (config_.interval);
			time_iterator time_it (start, seconds (interval_seconds));
			unsigned link_count = 0;
			while (++time_it <= end) {
				const time_duration interval_end_tod = time_it->time_of_day();
				std::ostringstream ss;
				ss << std::setfill ('0')
				   << symbol_name
				   << '.'
				   << std::setw (2) << interval_end_tod.hours()
				   << std::setw (2) << interval_end_tod.minutes();
				auto historical_stream = std::make_shared<item_stream_t> ();
				assert ((bool)historical_stream);
				if (!provider_->createItemStream (ss.str().c_str(), historical_stream))
					goto cleanup;
				auto status = stream->historical.insert (std::make_pair (ss.str(), std::move (historical_stream)));
				assert (true == status.second);

				++link_count;
			}

			assert (link_count > 0);

/* marketfeed chain, not rdm symbol list */
			for (int i = link_count - 1; i >= 0; i -= _countof (kRdmLinkId))
			{
				const unsigned chain_index = (i > 0) ? (i / _countof (kRdmLinkId)) : 0;
				std::ostringstream chain_name;
				chain_name << chain_index
					   << '#'
					   << symbol_name;
				auto chain_stream = std::make_shared<item_stream_t> ();
				assert ((bool)chain_stream);
				if (!provider_->createItemStream (chain_name.str().c_str(), chain_stream))
					goto cleanup;
				auto status = stream->chain.insert (std::make_pair (chain_name.str(), std::move (chain_stream)));
				assert (true == status.second);
			}

			stream_vector_.push_back (std::move (stream));
		}

	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			  "\"Severity\": \"" << severity_string (e.getSeverity()) << "\""
			", \"Classification\": \"" << classification_string (e.getClassification()) << "\""
			", \"StatusText\": \"" << e.getStatus().getStatusText() << "\" }";
		goto cleanup;
	} catch (rfa::common::InvalidConfigurationException& e) {
		LOG(ERROR) << "InvalidConfigurationException: { "
			  "\"Severity\": \"" << severity_string (e.getSeverity()) << "\""
			", \"Classification\": \"" << classification_string (e.getClassification()) << "\""
			", \"StatusText\": \"" << e.getStatus().getStatusText() << "\""
			", \"ParameterName\": \"" << e.getParameterName() << "\""
			", \"ParameterValue\": \"" << e.getParameterValue() << "\" }";
		goto cleanup;
	}

/* No main loop inside this thread, must spawn new thread for message pump. */
	event_pump_.reset (new event_pump_t (event_queue_));
	if (!(bool)event_pump_) {
		LOG(ERROR) << "Cannot create event pump.";
		goto cleanup;
	}

	event_thread_.reset (new boost::thread (*event_pump_.get()));
	if (!(bool)event_thread_) {
		LOG(ERROR) << "Cannot spawn event thread.";
		goto cleanup;
	}

/* Spawn SNMP implant. */
	if (config_.is_snmp_enabled) {
		snmp_agent_.reset (new snmp_agent_t (*this));
		if (!(bool)snmp_agent_)
			goto cleanup;
	}

/* Register Tcl API. */
	if (!register_tcl_api (getId()))
		goto cleanup;

	{
/* Timer for periodic publishing.
 */
		using namespace boost;
		using namespace posix_time;

		ptime due_time;
		if (!get_next_interval (&due_time)) {
			LOG(ERROR) << "Cannot calculate next interval.";
			goto cleanup;
		}
		const time_duration td = seconds (std::stoul (config_.interval));
		timer_.reset (new time_pump_t (due_time, td, this));
		if (!(bool)timer_) {
			LOG(ERROR) << "Cannot create time pump.";
			goto cleanup;
		}
		timer_thread_.reset (new boost::thread (*timer_.get()));
		if (!(bool)timer_thread_) {
			LOG(ERROR) << "Cannot spawn timer thread.";
			goto cleanup;
		}
		LOG(INFO) << "Added periodic timer, interval " << to_simple_string (td)
			<< ", due time " << to_simple_string (due_time);
	}

	LOG(INFO) << "Init complete, awaiting queries.";
	return;
cleanup:
	LOG(INFO) << "Init failed, cleaning up.";
	clear();
	is_shutdown_ = true;
	throw vpf::UserPluginException ("Init failed.");
}

void
hilo::stitch_t::clear()
{
/* Stop generating new events. */
	if (timer_thread_) {
		timer_thread_->interrupt();
		timer_thread_->join();
	}	
	timer_thread_.reset();
	timer_.reset();

/* Close SNMP agent. */
	snmp_agent_.reset();

/* Signal message pump thread to exit. */
	if ((bool)event_queue_)
		event_queue_->deactivate();
/* Drain and close event queue. */
	if ((bool)event_thread_)
		event_thread_->join();

/* Release everything with an RFA dependency. */
	event_thread_.reset();
	event_pump_.reset();
	stream_vector_.clear();
	query_vector_.clear();
	assert (provider_.use_count() <= 1);
	provider_.reset();
	assert (log_.use_count() <= 1);
	log_.reset();
	assert (event_queue_.use_count() <= 1);
	event_queue_.reset();
	assert (rfa_.use_count() <= 1);
	rfa_.reset();
}

/* Plugin exit point.
 */

void
hilo::stitch_t::destroy()
{
	LOG(INFO) << "Closing instance.";
/* Unregister Tcl API. */
	unregister_tcl_api (getId());
	clear();
	LOG(INFO) << "Runtime summary: {"
		    " \"tclQueryReceived\": " << cumulative_stats_[STITCH_PC_TCL_QUERY_RECEIVED] <<
		   ", \"timerQueryReceived\": " << cumulative_stats_[STITCH_PC_TIMER_QUERY_RECEIVED] <<
		" }";
	LOG(INFO) << "Instance closed.";
	vpf::AbstractUserPlugin::destroy();
}

/* callback from periodic timer.
 */
bool
hilo::stitch_t::processTimer (
	boost::posix_time::ptime t
	)
{
/* calculate timer accuracy, typically 15-1ms with default timer resolution.
 */
	if (DLOG_IS_ON(INFO)) {
		const boost::posix_time::ptime now (boost::posix_time::microsec_clock::universal_time());
		const auto ms = (now - t).total_milliseconds();
		if (0 == ms)
			LOG(INFO) << "delta " << (now - t).total_microseconds() << "us";
		else
			LOG(INFO) << "delta " << ms << "ms";
	}

	cumulative_stats_[STITCH_PC_TIMER_QUERY_RECEIVED]++;

/* Prevent overlapped queries. */
	boost::unique_lock<boost::shared_mutex> lock (query_mutex_, boost::try_to_lock_t());
	if (!lock.owns_lock()) {
		LOG(WARNING) << "Periodic refresh aborted due to running query.";
		return true;
	}

	try {
		sendRefresh();
	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			  "\"Severity\": \"" << severity_string (e.getSeverity()) << "\""
			", \"Classification\": \"" << classification_string (e.getClassification()) << "\""
			", \"StatusText\": \"" << e.getStatus().getStatusText() << "\" }";
	}
	return true;
}

/* calculate the last reset time.
 */
bool
hilo::stitch_t::get_last_reset_time (
	__time32_t*	t
	)
{
	using namespace boost::posix_time;
	const time_duration reset_tod = duration_from_string (config_.reset_time);
	const int interval_seconds = std::stoi (config_.interval);
	const ptime now_ptime (second_clock::universal_time());
	const time_duration now_tod = now_ptime.time_of_day();

	ptime reset_ptime (now_ptime.date(), reset_tod);

/* yesterday */
	if ((reset_tod + seconds (interval_seconds)) > now_tod)
		reset_ptime -= boost::gregorian::days (1);

	*t = to_unix_epoch<__time32_t> (reset_ptime);
	return true;
}

/* Calculate start of next interval.
 */
bool
hilo::stitch_t::get_next_interval (
	boost::posix_time::ptime* t
	)
{
	using namespace boost::posix_time;

	const time_duration reset_tod = duration_from_string (config_.reset_time);
	const int interval_seconds = std::stoi (config_.interval);
	const ptime now_ptime (second_clock::universal_time());
	const time_duration now_tod = now_ptime.time_of_day();

	ptime reset_ptime (now_ptime.date(), reset_tod);

/* yesterday */
	if ((reset_tod + seconds (interval_seconds)) > now_tod)
		reset_ptime -= boost::gregorian::days (1);

	const time_duration offset = now_ptime - reset_ptime;

/* round down to multiple of interval */
	const ptime end_ptime = reset_ptime + seconds ((offset.total_seconds() / interval_seconds) * interval_seconds);

/* increment to next period */
	const ptime next_ptime = end_ptime + seconds (interval_seconds);

	*t = next_ptime;
	return true;
}

/* Calculate the __time32_t of the end of the last interval, specified in
 * seconds.
 */
bool
hilo::stitch_t::get_end_of_last_interval (
	__time32_t*	t
	)
{
	using namespace boost::posix_time;
	const time_duration reset_tod = duration_from_string (config_.reset_time);
	const int interval_seconds = std::stoi (config_.interval);
	const ptime now_ptime (second_clock::universal_time());
	const time_duration now_tod = now_ptime.time_of_day();

	ptime reset_ptime (now_ptime.date(), reset_tod);

/* yesterday */
	if ((reset_tod + seconds (interval_seconds)) > now_tod)
		reset_ptime -= boost::gregorian::days (1);

	const time_duration offset = now_ptime - reset_ptime;

/* round down to multiple of interval */
	const ptime end_ptime = reset_ptime + seconds ((offset.total_seconds() / interval_seconds) * interval_seconds);

	*t = to_unix_epoch<__time32_t> (end_ptime);
	return true;
}

/* Calculate the __time32_t of the start of the last interval, specified in
 * seconds.
 */
bool
hilo::stitch_t::get_start_of_last_interval (
	__time32_t*	t
	)
{
	__time32_t end_time;
	if (!get_end_of_last_interval (&end_time))
		return false;
	const int interval_seconds = std::stoi (config_.interval);
	*t = end_time - interval_seconds;
	return true;
}

/* http://msdn.microsoft.com/en-us/library/4ey61ayt.aspx */
#define CTIME_LENGTH	26

bool
hilo::stitch_t::sendRefresh()
{
	using namespace boost::posix_time;
	const ptime t0 (microsec_clock::universal_time());
	last_activity_ = t0;

/* Calculate time boundary for query */
	__time32_t last_reset_time, from, till;
	get_last_reset_time (&last_reset_time);
	get_start_of_last_interval (&from);
	get_end_of_last_interval (&till);

	if (LOG_IS_ON(INFO)) {
		char from_str[CTIME_LENGTH], till_str[CTIME_LENGTH];
		_ctime32_s (from_str, _countof (from_str), &from);
		_ctime32_s (till_str, _countof (till_str), &till);
		from_str[CTIME_LENGTH - 2] = till_str[CTIME_LENGTH - 2] = '\0';
		LOG(INFO) << "refresh " << from_str << "-" << till_str;
	}

/* clear state for all items */
	std::for_each (stream_vector_.begin(), stream_vector_.end(), [](std::shared_ptr<broadcast_stream_t>& stream) {
		stream->hilo->clear();
	});

	DLOG(INFO) << "get_hilo /" << to_simple_string (ptime (kUnixEpoch, seconds (from))) << "/ /" << to_simple_string (ptime (kUnixEpoch, seconds (till))) << "/";
	single_iterator::get_hilo (query_vector_, from, till);

/* 7.5.9.1 Create a response message (4.2.2) */
	rfa::message::RespMsg response (false);	/* reference */

/* 7.5.9.2 Set the message model type of the response. */
	response.setMsgModelType (rfa::rdm::MMT_MARKET_PRICE);
/* 7.5.9.3 Set response type. */
	response.setRespType (rfa::message::RespMsg::RefreshEnum);
	response.setIndicationMask (rfa::message::RespMsg::RefreshCompleteFlag);
/* 7.5.9.4 Set the response type enumation. */
	response.setRespTypeNum (rfa::rdm::REFRESH_UNSOLICITED);

/* 7.5.9.5 Create or re-use a request attribute object (4.2.4) */
	rfa::message::AttribInfo attribInfo (false);	/* reference */
	attribInfo.setNameType (rfa::rdm::INSTRUMENT_NAME_RIC);
	RFA_String service_name (config_.service_name.c_str(), 0, false);	/* reference */
	attribInfo.setServiceName (service_name);
	response.setAttribInfo (attribInfo);

/* 6.2.8 Quality of Service. */
	rfa::common::QualityOfService QoS;
/* Timeliness: age of data, either real-time, unspecified delayed timeliness,
 * unspecified timeliness, or any positive number representing the actual
 * delay in seconds.
 */
	QoS.setTimeliness (rfa::common::QualityOfService::realTime);
/* Rate: minimum period of change in data, either tick-by-tick, just-in-time
 * filtered rate, unspecified rate, or any positive number representing the
 * actual rate in milliseconds.
 */
	QoS.setRate (rfa::common::QualityOfService::tickByTick);
	response.setQualityOfService (QoS);

/* 4.3.1 RespMsg.Payload */
// not std::map :(  derived from rfa::common::Data
	fields_.setAssociatedMetaInfo (provider_->getRwfMajorVersion(), provider_->getRwfMinorVersion());
	fields_.setInfo (kDictionaryId, kFieldListId);

/* DataBuffer based fields must be pre-encoded and post-bound. */
	rfa::data::FieldListWriteIterator it;
	rfa::data::FieldEntry field (false);
	rfa::data::DataBuffer data (false);
#ifdef CONFIG_32BIT_PRICE
	rfa::data::Real32 real_value;
	auto SetReal = [](rfa::data::DataBuffer* data, const rfa::data::Real32& value) {
		data->setReal32 (value);
	};
#else
	rfa::data::Real64 real_value;
	auto SetReal = [](rfa::data::DataBuffer* data, const rfa::data::Real64& value) {
		data->setReal64 (value);
	};
#endif /* CONFIG_32BIT_PRICE */
	rfa::data::Time rfaTime;
	rfa::data::Date rfaDate;
	struct tm _tm;

/* TIMEACT */
	_gmtime32_s (&_tm, &till);
	rfaTime.setHour   (_tm.tm_hour);
	rfaTime.setMinute (_tm.tm_min);
	rfaTime.setSecond (_tm.tm_sec);
	rfaTime.setMillisecond (0);

/* HIGH_1, LOW_1 as PRICE field type */
	real_value.setMagnitudeType (bnymellon::kMagnitude);

/* ACTIV_DATE */
	rfaDate.setDay   (/* rfa(1-31) */ _tm.tm_mday        /* tm(1-31) */);
	rfaDate.setMonth (/* rfa(1-12) */ 1 + _tm.tm_mon     /* tm(0-11) */);
	rfaDate.setYear  (/* rfa(yyyy) */ 1900 + _tm.tm_year /* tm(yyyy-1900 */);

	rfa::common::RespStatus status;
/* Item interaction state: Open, Closed, ClosedRecover, Redirected, NonStreaming, or Unspecified. */
	status.setStreamState (rfa::common::RespStatus::OpenEnum);
/* Data quality state: Ok, Suspect, or Unspecified. */
	status.setDataState (rfa::common::RespStatus::OkEnum);
/* Error code, e.g. NotFound, InvalidArgument, ... */
	status.setStatusCode (rfa::common::RespStatus::NoneEnum);
	response.setRespStatus (status);

	std::for_each (stream_vector_.begin(), stream_vector_.end(), [&](std::shared_ptr<broadcast_stream_t>& stream)
	{
		attribInfo.setName (stream->rfa_name);
		it.start (fields_);
/* TIMACT */
		field.setFieldID (kRdmTimeOfUpdateId);
		data.setTime (rfaTime);
		field.setData (data), it.bind (field);

/* PRICE field is a rfa::Real64 value specified as <mantissa> × 10?.
 * Rfa deprecates setting via <double> data types so we create a mantissa from
 * source value and consider that we publish to 6 decimal places.
 */
/* HIGH_1 */
		field.setFieldID (kRdmTodaysHighId);
		real_value.setValue (bnymellon::mantissa (stream->hilo->high));
		SetReal (&data, real_value);
		field.setData (data), it.bind (field);
/* LOW_1 */
		field.setFieldID (kRdmTodaysLowId);
		real_value.setValue (bnymellon::mantissa (stream->hilo->low));
		SetReal (&data, real_value);
		field.setData (data), it.bind (field);
/* ACTIV_DATE */
		field.setFieldID (kRdmActiveDateId);
		data.setDate (rfaDate);
		field.setData (data), it.bind (field);

		it.complete();
		response.setPayload (fields_);

#ifdef DEBUG
/* 4.2.8 Message Validation.  RFA provides an interface to verify that
 * constructed messages of these types conform to the Reuters Domain
 * Models as specified in RFA API 7 RDM Usage Guide.
 */
		RFA_String warningText;
		const uint8_t validation_status = response.validateMsg (&warningText);
		if (rfa::message::MsgValidationWarning == validation_status) {
			LOG(ERROR) << "respMsg::validateMsg: { \"warningText\": \"" << warningText << "\" }";
		} else {
			assert (rfa::message::MsgValidationOk == validation_status);
		}
#endif
		provider_->send (*stream.get(), static_cast<rfa::common::Msg&> (response));
		VLOG(1) << stream->rfa_name << " hi:" << stream->hilo->high << " lo:" << stream->hilo->low;
	});

/* create time period for bar and shift x-minutes for the specified range */
	const auto end_time32 (till);
	const int interval_seconds = std::stoi (config_.interval);
	time_period tp (ptime (kUnixEpoch, seconds (last_reset_time)), seconds (interval_seconds));

	while (true) {
		const __time32_t from = to_unix_epoch<__time32_t> (tp.begin());
		const __time32_t till = to_unix_epoch<__time32_t> (tp.end());

/* inclusive of specified end time */
		if (till > end_time32)
			break;

/* reset bars */
		std::for_each (stream_vector_.begin(), stream_vector_.end(), [](std::shared_ptr<broadcast_stream_t>& stream) {
			stream->hilo->clear();
		});

		DLOG(INFO) << "get_hilo /" << to_simple_string (ptime (kUnixEpoch, seconds (from))) << "/ /" << to_simple_string (ptime (kUnixEpoch, seconds (till))) << "/";
		single_iterator::get_hilo (query_vector_, from, till);
		
/* create flexrecord for each pair */
		std::ostringstream ss;
		ss << std::setfill ('0')
		   << '.'
		   << std::setw (2) << tp.end().time_of_day().hours()
		   << std::setw (2) << tp.end().time_of_day().minutes();

/* TIMEACT */
		_gmtime32_s (&_tm, &till);
		rfaTime.setHour   (_tm.tm_hour);
		rfaTime.setMinute (_tm.tm_min);
		rfaTime.setSecond (_tm.tm_sec);
		rfaTime.setMillisecond (0);

/* ACTIV_DATE */
		rfaDate.setDay   (/* rfa(1-31) */ _tm.tm_mday        /* tm(1-31) */);
		rfaDate.setMonth (/* rfa(1-12) */ 1 + _tm.tm_mon     /* tm(0-11) */);
		rfaDate.setYear  (/* rfa(yyyy) */ 1900 + _tm.tm_year /* tm(yyyy-1900 */);

		std::for_each (stream_vector_.begin(), stream_vector_.end(), [&](std::shared_ptr<broadcast_stream_t>& stream)
		{
			rfa::common::RFA_String rfa_name (stream->rfa_name);
			rfa_name.append (ss.str().c_str());
			attribInfo.setName (rfa_name);
			it.start (fields_);
/* TIMACT */
			field.setFieldID (kRdmTimeOfUpdateId);
			data.setTime (rfaTime);
			field.setData (data), it.bind (field);

/* PRICE field is a rfa::Real64 value specified as <mantissa> × 10?.
 * Rfa deprecates setting via <double> data types so we create a mantissa from
 * source value and consider that we publish to 6 decimal places.
 */
/* HIGH_1 */
			field.setFieldID (kRdmTodaysHighId);
			real_value.setValue (bnymellon::mantissa (stream->hilo->high));
			SetReal (&data, real_value);
			field.setData (data), it.bind (field);
/* LOW_1 */
			field.setFieldID (kRdmTodaysLowId);
			real_value.setValue (bnymellon::mantissa (stream->hilo->low));
			SetReal (&data, real_value);
			field.setData (data), it.bind (field);
/* ACTIV_DATE */
			field.setFieldID (kRdmActiveDateId);
			data.setDate (rfaDate);
			field.setData (data), it.bind (field);

			it.complete();
			response.setPayload (fields_);

#ifdef DEBUG
/* 4.2.8 Message Validation.  RFA provides an interface to verify that
 * constructed messages of these types conform to the Reuters Domain
 * Models as specified in RFA API 7 RDM Usage Guide.
 */
			RFA_String warningText;
			const uint8_t validation_status = response.validateMsg (&warningText);
			if (rfa::message::MsgValidationWarning == validation_status) {
				LOG(ERROR) << "respMsg::validateMsg: { \"warningText\": \"" << warningText << "\" }";
			} else {
				assert (rfa::message::MsgValidationOk == validation_status);
			}
#endif
			const std::string key (rfa_name.c_str());
			provider_->send (*stream->historical[key].get(), static_cast<rfa::common::Msg&> (response));
			DVLOG(1) << rfa_name << " hi:" << stream->hilo->high << " lo:" << stream->hilo->low;

/* do not reset analytic result set so next query extends previous result set */

		});

		tp.shift (seconds (interval_seconds));
	}

/* Publish a symbol list aka chain of all published historical item streams. */
	std::vector<std::string> link_times;
	link_times.reserve ((24 * 60 * 60) / interval_seconds);
	const ptime last_reset_ptime (kUnixEpoch, seconds (last_reset_time));
	const ptime till_ptime (kUnixEpoch, seconds (till));
	for (time_iterator time_it (last_reset_ptime, seconds (interval_seconds)); ++time_it <= till_ptime;)
	{
		std::ostringstream ss;
		ss << std::setfill ('0')
		   << '.'
		   << std::setw (2) << time_it->time_of_day().hours()
		   << std::setw (2) << time_it->time_of_day().minutes();
		link_times.push_back (ss.str());
	}
	assert (link_times.size() > 0);

	const size_t chain_index_max = link_times.size() / _countof (kRdmLinkId);
	std::for_each (stream_vector_.begin(), stream_vector_.end(), [&](std::shared_ptr<broadcast_stream_t>& stream)
	{
/* e.g. 0#.DJI .. 3#.DJI */
		for (int j = chain_index_max; j >= 0; j--)
		{
			size_t i = j * _countof (kRdmLinkId);
			std::ostringstream ss;
			ss << j
			   << '#'
			   << stream->rfa_name.c_str();
			RFA_String rfa_name (ss.str().c_str(), (int)ss.str().length(), true);
			attribInfo.setName (rfa_name);

			it.start (fields_);

/* REF_COUNT */
			field.setFieldID (kRdmReferenceCountId);
			const uint32_t ref_count = min (_countof (kRdmLinkId), (uint32_t)(link_times.size() - i));
			data.setUInt32 (ref_count);
			field.setData (data), it.bind (field);
/* LONGLINKx */
			for (unsigned k = 0;
			     k < _countof (kRdmLinkId);
			     k++, i++)
			{
				field.setFieldID (kRdmLinkId[k]);
				if (i < link_times.size()) {
					RFA_String link_name (stream->rfa_name);
					link_name.append (link_times[i].c_str());
					data.setFromString (link_name, rfa::data::DataBuffer::StringRMTESEnum);
					field.setData (data), it.bind (field);
				} else {
					RFA_String empty_string ("", 0, false);
					data.setFromString (empty_string, rfa::data::DataBuffer::StringRMTESEnum);
					field.setData (data), it.bind (field);
				}				
			} 
/* LONGPREVLR */
			field.setFieldID (kRdmPreviousLinkId);
			if (j > 0) {
				std::ostringstream ss;
				ss << (j - 1)
				   << '#'
				   << stream->rfa_name.c_str();
				RFA_String previous_name (ss.str().c_str(), (int)ss.str().length(), true);
				data.setFromString (previous_name, rfa::data::DataBuffer::StringRMTESEnum);
				field.setData (data), it.bind (field);
			} else {
				RFA_String empty_string ("", 0, false);
				data.setFromString (empty_string, rfa::data::DataBuffer::StringRMTESEnum);
				field.setData (data), it.bind (field);
			}
/* LONGNEXTLR */
			field.setFieldID (kRdmNextLinkId);
			if (j < chain_index_max) {
				std::ostringstream ss;
				ss << (j + 1)
				   << '#'
				   << stream->rfa_name.c_str();
				RFA_String next_name (ss.str().c_str(), (int)ss.str().length(), true);
				data.setFromString (next_name, rfa::data::DataBuffer::StringRMTESEnum);
				field.setData (data), it.bind (field);
			} else {
				RFA_String empty_string ("", 0, false);
				data.setFromString (empty_string, rfa::data::DataBuffer::StringRMTESEnum);
				field.setData (data), it.bind (field);
			}

			it.complete();
			response.setPayload (fields_);

#ifdef DEBUG
/* 4.2.8 Message Validation.  RFA provides an interface to verify that
 * constructed messages of these types conform to the Reuters Domain
 * Models as specified in RFA API 7 RDM Usage Guide.
 */
			RFA_String warningText;
			const uint8_t validation_status = response.validateMsg (&warningText);
			if (rfa::message::MsgValidationWarning == validation_status) {
				LOG(ERROR) << "respMsg::validateMsg: { \"warningText\": \"" << warningText << "\" }";
			} else {
				assert (rfa::message::MsgValidationOk == validation_status);
			}
#endif
			const std::string key (rfa_name.c_str());
			provider_->send (*stream->chain[key].get(), static_cast<rfa::common::Msg&> (response));
		}
	});

/* Timing */
	const ptime t1 (microsec_clock::universal_time());
	const time_duration td = t1 - t0;
	LOG(INFO) << "refresh complete " << td.total_milliseconds() << "ms";
	if (td < min_refresh_time_) min_refresh_time_ = td;
	if (td > max_refresh_time_) max_refresh_time_ = td;
	total_refresh_time_ += td;

	return true;
}

/* eof */
