/* A basic Velocity Analytics User-Plugin to exporting a new Tcl command and
 * periodically publishing out to ADH via RFA using RDM/MarketPrice.
 */

#include "stitch.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

/* Boost Posix Time */
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/gregorian/gregorian_types.hpp"

#include "chromium/logging.hh"
#include "get_hilo.hh"
#include "error.hh"

/* RDM Usage Guide: Section 6.5: Enterprise Platform
 * For future compatibility, the DictionaryId should be set to 1 by providers.
 * The DictionaryId for the RDMFieldDictionary is 1.
 */
static const int kDictionaryId = 1;

/* RDM: Absolutely no idea. */
static const int kFieldListId = 3;

/* RDM FIDs. */
static const int kRdmTimeOfUpdateId	= 5;
static const int kRdmTodaysHighId	= 12;
static const int kRdmTodaysLowId	= 13;
static const int kRdmActiveDateId	= 17;

/* FlexRecord Quote identifier. */
static const uint32_t kQuoteId = 40002;

/* Tcl exported API. */
static const char* kFunctionName = "hilo_query";

/* Default FlexRecord fields. */
static const char* kDefaultBidField = "BidPrice";
static const char* kDefaultAskField = "AskPrice";

using rfa::common::RFA_String;

static
bool
parseRule (
	const std::string	str,
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
		rule.legs.first.symbol_name	= v[2];
		rule.legs.first.bid_field	= v[3];
		rule.legs.first.ask_field	= v[4];
		rule.legs.second.symbol_name	= rule.legs.first.symbol_name;
		rule.legs.second.bid_field	= rule.legs.first.bid_field;
		rule.legs.second.ask_field	= rule.legs.first.ask_field;
		break;

/* four fields -> name, operator, RIC, RIC */
	case 4:
		rule.math_op			= v[1] == "MUL" ? hilo::MATH_OP_TIMES : hilo::MATH_OP_DIVIDE;
		rule.legs.first.symbol_name	= v[2];
		rule.legs.second.symbol_name	= v[3];
		break;

/* two fields -> name, RIC */
	case 2:
		rule.legs.first.symbol_name = rule.legs.second.symbol_name = v[1];
		break;

/* one field -> name = RIC */
	case 1:
		break;

	default:
		return false;
	}

	return true;
}

static
void
on_timer (
	PTP_CALLBACK_INSTANCE Instance,
	PVOID Context,
	PTP_TIMER Timer
	)
{
	hilo::stitch_t* stitch = static_cast<hilo::stitch_t*>(Context);
	stitch->processTimer (nullptr);
}

hilo::stitch_t::stitch_t() :
	is_shutdown_ (false),
	rfa_ (nullptr),
	event_queue_ (nullptr),
	log_ (nullptr),
	provider_ (nullptr),
	event_pump_ (nullptr),
	thread_ (nullptr)
{
}

hilo::stitch_t::~stitch_t()
{
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

	if (!config_.parseDomElement (vpf_config.getXmlConfigData()))
		goto cleanup;

	LOG(INFO) << config_;

/** RFA initialisation. **/
	try {
/* RFA context. */
		rfa_ = new rfa_t (config_);
		if (nullptr == rfa_ || !rfa_->init())
			goto cleanup;

/* RFA asynchronous event queue. */
		const RFA_String eventQueueName (config_.event_queue_name.c_str(), 0, false);
		event_queue_ = rfa::common::EventQueue::create (eventQueueName);
		if (nullptr == event_queue_)
			goto cleanup;

/* RFA logging. */
		log_ = new logging::LogEventProvider (config_, *event_queue_);
		if (nullptr == log_ || !log_->Register())
			goto cleanup;

/* RFA provider. */
		provider_ = new provider_t (config_, *rfa_, *event_queue_);
		if (nullptr == provider_ || !provider_->init())
			goto cleanup;

/* Create state for published instruments. */
		for (auto it = config_.rules.begin();
			it != config_.rules.end();
			++it)
		{
			std::shared_ptr<hilo_t> rule (new hilo_t);
			assert ((bool)rule);
			if (!parseRule (*it, *rule.get()))
				goto cleanup;
			query_vector_.push_back (rule);
			std::string symbol_name = rule.get()->name + config_.suffix;
			std::unique_ptr<broadcast_stream_t> stream (new broadcast_stream_t (rule));
			assert ((bool)stream);
			if (!provider_->createItemStream (symbol_name.c_str(), *stream.get()))
				goto cleanup;
			stream_vector_.push_back (std::move (stream));
		}

/* Microsoft threadpool timer. */
		timer_ = new ms::timer (CreateThreadpoolTimer (static_cast<PTP_TIMER_CALLBACK>(on_timer), this /* closure */, nullptr /* env */));
		if (!timer_)
			goto cleanup;

	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			"Severity: \"" << severity_string (e.getSeverity()) << "\""
			", Classification: \"" << classification_string (e.getClassification()) << "\""
			", StatusText: \"" << e.getStatus().getStatusText() << "\" }";
		goto cleanup;
	} catch (rfa::common::InvalidConfigurationException& e) {
		LOG(ERROR) << "InvalidConfigurationException: { "
			"Severity: \"" << severity_string (e.getSeverity()) << "\""
			", Classification: \"" << classification_string (e.getClassification()) << "\""
			", StatusText: \"" << e.getStatus().getStatusText() << "\""
			", ParameterName: \"" << e.getParameterName() << "\""
			", ParameterValue: \"" << e.getParameterValue() << "\" }";
		goto cleanup;
	}

/* No main loop inside this thread, must spawn new thread for message pump. */
	event_pump_ = new event_pump_t (*event_queue_);
	if (nullptr == event_pump_)
		goto cleanup;
	thread_ = new boost::thread (*event_pump_);
	if (nullptr == thread_)
		goto cleanup;

/* Register Tcl API. */
	registerCommand (getId(), kFunctionName);
	LOG(INFO) << "Registered Tcl API \"" << kFunctionName << "\"";

/* Timer for periodic publishing.
 */
	FILETIME due_time;
	get_next_interval (due_time);
	const DWORD timer_period = std::stoi (config_.interval) * 1000;
	SetThreadpoolTimer (timer_->get(), &due_time, timer_period, 0);
	LOG(INFO) << "Added periodic timer, interval " << timer_period << "ms";

	return;
cleanup:
	clear();
	is_shutdown_ = true;
	throw vpf::UserPluginException ("Init failed.");
}

void
hilo::stitch_t::clear()
{
/* Stop generating new events. */
#if _WIN32_WINNT >= _WIN32_WINNT_WS08
	if (timer_)
		SetThreadpoolTimer (timer_->get(), nullptr, 0, 0);
#endif
	delete timer_; timer_ = nullptr;
/* Signal message pump thread to exit. */
	if (nullptr != event_queue_)
		event_queue_->deactivate();
/* Drain and close event queue. */
	if (nullptr != thread_)
		thread_->join();

	delete provider_; provider_ = nullptr;
	if (nullptr != log_)
		log_->Unregister();
	delete log_; log_ = nullptr;
	if (nullptr != event_queue_)
		event_queue_->destroy();
	event_queue_ = nullptr;
	delete rfa_; rfa_ = nullptr;
}

/* Plugin exit point.
 */

void
hilo::stitch_t::destroy()
{
/* Unregister Tcl API. */
	deregisterCommand (getId(), kFunctionName);
	LOG(INFO) << "Unregistered Tcl API \"" << kFunctionName << "\"";
	clear();
	LOG(INFO) << "Runtime summary: {}";
	vpf::AbstractUserPlugin::destroy();
}

/* Tcl boilerplate.
 *
 * stitch_query [startTime] [endTime]
 */

#define Tcl_GetLongFromObj \
	(tclStubsPtr->PTcl_GetLongFromObj)	/* 39 */
#define Tcl_GetStringFromObj \
	(tclStubsPtr->PTcl_GetStringFromObj)	/* 41 */
#define Tcl_ListObjAppendElement \
	(tclStubsPtr->PTcl_ListObjAppendElement)/* 44 */
#define Tcl_ListObjIndex \
	(tclStubsPtr->PTcl_ListObjIndex)	/* 46 */
#define Tcl_ListObjLength \
	(tclStubsPtr->PTcl_ListObjLength)	/* 47 */
#define Tcl_NewDoubleObj \
	(tclStubsPtr->PTcl_NewDoubleObj)	/* 51 */
#define Tcl_NewListObj \
	(tclStubsPtr->PTcl_NewListObj)		/* 53 */
#define Tcl_NewStringObj \
	(tclStubsPtr->PTcl_NewStringObj)	/* 56 */
#define Tcl_SetResult \
	(tclStubsPtr->PTcl_SetResult)		/* 232 */
#define Tcl_SetObjResult \
	(tclStubsPtr->PTcl_SetObjResult)	/* 235 */
#define Tcl_WrongNumArgs \
	(tclStubsPtr->PTcl_WrongNumArgs)	/* 264 */

int
hilo::stitch_t::execute (
	const vpf::CommandInfo& cmdInfo,
	vpf::TCLCommandData& cmdData
	)
{
	TCLLibPtrs* tclStubsPtr = (TCLLibPtrs*)cmdData.mClientData;
	Tcl_Interp* interp = cmdData.mInterp;		/* Current interpreter. */
	int objc = cmdData.mObjc;			/* Number of arguments. */
	Tcl_Obj** CONST objv = cmdData.mObjv;		/* Argument strings. */

	DLOG(INFO) << "Tcl execute objc=" << objc;

	try {
		if (objc < 2 || objc > 5) {
			Tcl_WrongNumArgs (interp, 1, objv, "symbolList ?startTime? ?endTime?");
			return TCL_ERROR;
		}

/* startTime if not provided is market open. */
		__time32_t startTime;
		if (objc >= 3)
			Tcl_GetLongFromObj (interp, objv[2], &startTime);
		else
			startTime = TBPrimitives::GetOpeningTime();

/* endTime if not provided is now. */
		__time32_t endTime;
		if (objc >= 4)
			Tcl_GetLongFromObj (interp, objv[3], &endTime);
		else
			endTime = TBPrimitives::GetCurrentTime();

/* Time must be ascending. */
		if (endTime <= startTime) {
			Tcl_SetResult (interp, "endTime must be after startTime", TCL_STATIC);
			return TCL_ERROR;
		}

		DLOG(INFO) << "startTime=" << startTime << ", endTime=" << endTime;

/* symbolList must be a list object.
 * NB: VA 7.0 does not export Tcl_ListObjGetElements()
 */
		int listLen, result = Tcl_ListObjLength (interp, objv[1], &listLen);
		if (TCL_OK != result)
			return result;
		if (0 == listLen) {
			Tcl_SetResult (interp, "bad symbol list", TCL_STATIC);
			return TCL_ERROR;
		}

		DLOG(INFO) << "symbol list with #" << listLen << " entries";

/* Convert TCl list parameter into STL container. */
		std::vector<std::shared_ptr<hilo_t>> query;
		for (int i = 0; i < listLen; i++)
		{
			Tcl_Obj* objPtr = nullptr;

			Tcl_ListObjIndex (interp, objv[1], i, &objPtr);

			int len = 0;
			char* rule_text = Tcl_GetStringFromObj (objPtr, &len);
			std::shared_ptr<hilo_t> rule (new hilo_t);
			assert ((bool)rule);
			if (len > 0 && parseRule (rule_text, *rule.get())) {
				query.push_back (rule);
				DLOG(INFO) << "#" << (1 + i) << " " << rule.get()->name;
			} else {
				Tcl_SetResult (interp, "bad symbol list", TCL_STATIC);
				return TCL_ERROR;
			}
		}

		get_hilo (query, startTime, endTime);

/* Convert STL container result set into a new Tcl list. */
		Tcl_Obj* resultListPtr = Tcl_NewListObj (0, NULL);
		std::for_each (query.begin(), query.end(),
			[&](std::shared_ptr<hilo_t> shared_it)
		{
			hilo_t& it = *shared_it.get();
			DLOG(INFO) << "Into result list name=" << it.name << " high=" << it.high << " low=" << it.low;
			Tcl_Obj* elemObjPtr[] = {
				Tcl_NewStringObj (it.name.c_str(), -1),
				Tcl_NewDoubleObj (it.high),
				Tcl_NewDoubleObj (it.low),
			};
			Tcl_ListObjAppendElement (interp, resultListPtr, Tcl_NewListObj (_countof (elemObjPtr), elemObjPtr));
		});
		Tcl_SetObjResult (interp, resultListPtr);
		DLOG(INFO) << "execute complete";
		return TCL_OK;
	}
/* FlexRecord exceptions */
	catch (const vpf::PluginFrameworkException& e) {
		/* yay broken Tcl API */
		Tcl_SetResult (interp, (char*)e.what(), TCL_VOLATILE);
	}
	catch (...) {
		Tcl_SetResult (interp, "Unhandled exception", TCL_STATIC);
	}
	return TCL_ERROR;
}

void
hilo::stitch_t::processTimer (
	void*	pClosure
	)
{
	try {
		sendRefresh();
	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			"Severity: \"" << severity_string (e.getSeverity()) << "\""
			", Classification: \"" << classification_string (e.getClassification()) << "\""
			", StatusText: \"" << e.getStatus().getStatusText() << "\" }";
	}
}

/* http://en.wikipedia.org/wiki/Unix_epoch */
static const boost::posix_time::ptime kUnixEpoch (boost::gregorian::date (1970, 1, 1));

/* calculate the last reset time.
 */
void
hilo::stitch_t::get_last_reset_time (
	__time32_t&	t
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

	t = (reset_ptime - kUnixEpoch).total_seconds();
}

/* Calculate start of next interval.
 */
void
hilo::stitch_t::get_next_interval (
	FILETIME&	ft
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

/* shift is difference between 1970-Jan-01 & 1601-Jan-01 in 100-nanosecond intervals.
 */
	const uint64_t shift = 116444736000000000ULL; // (27111902 << 32) + 3577643008

	union {
		FILETIME as_file_time;
		uint64_t as_integer; // 100-nanos since 1601-Jan-01
	} caster;
	caster.as_integer = (next_ptime - kUnixEpoch).total_microseconds() * 10; // upconvert to 100-nanos
	caster.as_integer += shift; // now 100-nanos since 1601-Jan-01

	ft = caster.as_file_time;
}

/* Calculate the __time32_t of the end of the last interval, specified in
 * seconds.
 */
void
hilo::stitch_t::get_end_of_last_interval (
	__time32_t&	t
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

	t = (end_ptime - kUnixEpoch).total_seconds();
}

/* http://msdn.microsoft.com/en-us/library/4ey61ayt.aspx */
#define CTIME_LENGTH	26

bool
hilo::stitch_t::sendRefresh()
{
/* Calculate time boundary for query */
	__time32_t startTime, endTime;
	get_last_reset_time (startTime);
	get_end_of_last_interval (endTime);

	char start_str[CTIME_LENGTH], end_str[CTIME_LENGTH];
	_ctime32_s (start_str, _countof (start_str), &startTime);
	_ctime32_s (end_str, _countof (end_str), &endTime);
	start_str[CTIME_LENGTH - 2] = end_str[CTIME_LENGTH - 2] = '\0';
	LOG(INFO) << "refresh " << start_str << "-" << end_str;

	const boost::posix_time::ptime t0 (boost::posix_time::microsec_clock::universal_time());

	get_hilo (query_vector_, startTime, endTime);

/* 7.5.9.1 Create a response message (4.2.2) */
	rfa::message::RespMsg response (false);	/* reference */

/* 7.5.9.2 Set the message model type of the response. */
	response.setMsgModelType (rfa::rdm::MMT_MARKET_PRICE);
/* 7.5.9.3 Set response type. */
	response.setRespType (rfa::message::RespMsg::RefreshEnum);
	response.setIndicationMask (response.getIndicationMask() | rfa::message::RespMsg::RefreshCompleteFlag);
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
	rfa::data::FieldEntry timeact_field (false), activ_date_field (false), price_field (false);
	rfa::data::DataBuffer timeact_data (false), activ_date_data (false), price_data (false);
	rfa::data::Real64 real64;
	rfa::data::Time rfaTime;
	rfa::data::Date rfaDate;
	struct tm _tm;

/* TIMEACT */
	timeact_field.setFieldID (kRdmTimeOfUpdateId);
	_gmtime32_s (&_tm, &endTime);
	rfaTime.setHour   (_tm.tm_hour);
	rfaTime.setMinute (_tm.tm_min);
	rfaTime.setSecond (_tm.tm_sec);
	rfaTime.setMillisecond (0);
	timeact_data.setTime (rfaTime);
	timeact_field.setData (timeact_data);

/* HIGH_1, LOW_1 as PRICE field type */
	real64.setMagnitudeType (rfa::data::ExponentNeg6);
	price_data.setReal64 (real64);
	price_field.setData (price_data);

/* ACTIV_DATE */
	activ_date_field.setFieldID (kRdmActiveDateId);
	rfaDate.setDay   (/* rfa(1-31) */ _tm.tm_mday        /* tm(1-31) */);
	rfaDate.setMonth (/* rfa(1-12) */ 1 + _tm.tm_mon     /* tm(0-11) */);
	rfaDate.setYear  (/* rfa(yyyy) */ 1900 + _tm.tm_year /* tm(yyyy-1900 */);
	activ_date_data.setDate (rfaDate);
	activ_date_field.setData (activ_date_data);

	rfa::common::RespStatus status;
/* Item interaction state: Open, Closed, ClosedRecover, Redirected, NonStreaming, or Unspecified. */
	status.setStreamState (rfa::common::RespStatus::OpenEnum);
/* Data quality state: Ok, Suspect, or Unspecified. */
	status.setDataState (rfa::common::RespStatus::OkEnum);
/* Error code, e.g. NotFound, InvalidArgument, ... */
	status.setStatusCode (rfa::common::RespStatus::NoneEnum);
	response.setRespStatus (status);

	std::for_each (stream_vector_.begin(), stream_vector_.end(),
		[&](std::unique_ptr<broadcast_stream_t>& shared_stream)
	{
		broadcast_stream_t& stream = *shared_stream.get();

		attribInfo.setName (stream.rfa_name);
		it.start (fields_);
/* TIMACT */
		it.bind (timeact_field);

/* PRICE field is a rfa::Real64 value specified as <mantissa> × 10⁶.
 * Rfa deprecates setting via <double> data types so we create a mantissa from
 * source value and consider that we publish to 6 decimal places.
 */
/* HIGH_1 */
		price_field.setFieldID (kRdmTodaysHighId);
		const double high_price = stream.hilo.get()->high * 1000000.0;
		const int64_t high_mantissa = (int64_t)high_price;
		real64.setValue (high_mantissa);		
		it.bind (price_field);
/* LOW_1 */
		price_field.setFieldID (kRdmTodaysLowId);
		const double low_price = stream.hilo.get()->low * 1000000.0;
		const int64_t low_mantissa = (int64_t)low_price;
		real64.setValue (low_mantissa);
		it.bind (price_field);
/* ACTIV_DATE */
		it.bind (activ_date_field);
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
			LOG(WARNING) << "respMsg::validateMsg: { warningText: \"" << warningText << "\" }";
		} else {
			assert (rfa::message::MsgValidationOk == validation_status);
		}
#endif
		provider_->send (stream, static_cast<rfa::common::Msg&> (response));
		DLOG(INFO) << stream.rfa_name << " high=" << high_price << " low=" << low_price;
	});

	const boost::posix_time::ptime t1 (boost::posix_time::microsec_clock::universal_time());
	const boost::posix_time::time_duration td = t1 - t0;

	LOG(INFO) << "refresh complete " << td.total_milliseconds() << "ms";
	return true;
}

/* eof */
