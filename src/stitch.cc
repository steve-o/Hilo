/* A basic Velocity Analytics User-Plugin to exporting a new Tcl command and
 * periodically publishing out to ADH via RFA using RDM/MarketPrice.
 */

#include "stitch.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

/* Boost Posix Time */
#include "boost/date_time/gregorian/gregorian_types.hpp"

#include "chromium/logging.hh"
#include "get_hilo.hh"
#include "snmp_agent.hh"
#include "error.hh"
#include "rfa_logging.hh"
#include "rfaostream.hh"

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

/* Feed log file FlexRecord name */
static const char* kHiloFlexRecordName = "Hilo";

/* Tcl exported API. */
static const char* kBasicFunctionName = "hilo_query";
static const char* kFeedLogFunctionName = "hilo_feedlog";

/* Default FlexRecord fields. */
static const char* kDefaultBidField = "BidPrice";
static const char* kDefaultAskField = "AskPrice";

/* http://en.wikipedia.org/wiki/Unix_epoch */
static const boost::gregorian::date kUnixEpoch (1970, 1, 1);

LONG volatile hilo::stitch_t::instance_count_ = 0;

std::list<hilo::stitch_t*> hilo::stitch_t::global_list_;
boost::shared_mutex hilo::stitch_t::global_list_lock_;

using rfa::common::RFA_String;

/* Boney M. defined: round half up the river of Babylon.
 */
static inline
double
round_half_up (double x)
{
	return floor (x + 0.5);
}

/* mantissa of 10E6
 */
static inline
int64_t
bnymellon_mantissa (double x)
{
	return (int64_t) round_half_up (x * 1000000.0);
}

/* round a double value to 6 decimal places using round half up
 */
static inline
double
bnymellon_round (double x)
{
	return (double) bnymellon_mantissa (x) / 1000000.0;
}

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
	LOG(INFO) << "{ pluginType: \"" << plugin_type_ << "\""
		", pluginId: \"" << plugin_id_ << "\""
		", instance: " << instance_ <<
		", version: \"2.0.37\""
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

/* Microsoft threadpool timer. */
		timer_.reset (CreateThreadpoolTimer (static_cast<PTP_TIMER_CALLBACK>(on_timer), this /* closure */, nullptr /* env */));
		if (!(bool)timer_)
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
	event_pump_.reset (new event_pump_t (event_queue_));
	if (!(bool)event_pump_)
		goto cleanup;

	thread_.reset (new boost::thread (*event_pump_.get()));
	if (!(bool)thread_)
		goto cleanup;

/* Spawn SNMP implant. */
	snmp_agent_.reset (new snmp_agent_t (*this));
	if (!(bool)snmp_agent_)
		goto cleanup;

/* Register Tcl API. */
	registerCommand (getId(), kBasicFunctionName);
	LOG(INFO) << "Registered Tcl API \"" << kBasicFunctionName << "\"";
	registerCommand (getId(), kFeedLogFunctionName);
	LOG(INFO) << "Registered Tcl API \"" << kFeedLogFunctionName << "\"";

/* Timer for periodic publishing.
 */
	FILETIME due_time;
	get_next_interval (due_time);
	const DWORD timer_period = std::stoul (config_.interval) * 1000;
#if 1
	SetThreadpoolTimer (timer_.get(), &due_time, timer_period, 0);
	LOG(INFO) << "Added periodic timer, interval " << timer_period << "ms";
#else
/* requires Platform SDK 7.1 */
	typedef BOOL (WINAPI *SetWaitableTimerExProc)(
		__in  HANDLE hTimer,
		__in  const LARGE_INTEGER *lpDueTime,
		__in  LONG lPeriod,
		__in  PTIMERAPCROUTINE pfnCompletionRoutine,
		__in  LPVOID lpArgToCompletionRoutine,
		__in  PREASON_CONTEXT WakeContext,
		__in  ULONG TolerableDelay
	);
	SetWaitableTimerExProc pFnSetWaitableTimerEx = nullptr;
	ULONG tolerance = std::stoul (config_.tolerable_delay);
	REASON_CONTEXT reasonContext = {0};
	reasonContext.Version = 0;
	reasonContext.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
	reasonContext.Reason.SimpleReasonString = L"HiloTimer";
	HMODULE hKernel32Module = GetModuleHandle (_T("kernel32.dll"));
	BOOL timer_status = false;
	if (nullptr != hKernel32Module)
		pFnSetWaitableTimerEx = (SetWaitableTimerExProc) ::GetProcAddress (hKernel32Module, "SetWaitableTimerEx");
	if (nullptr != pFnSetWaitableTimerEx)
		timer_status = pFnSetWaitableTimerEx (timer_.get(), &due_time, timer_period, nullptr, nullptr, &reasonContext, tolerance);
	if (timer_status) {
		LOG(INFO) << "Added periodic timer, interval " << timer_period << "ms, tolerance " << tolerance << "ms";
	} else {
		SetThreadpoolTimer (timer_.get(), &due_time, timer_period, 0);
		LOG(INFO) << "Added periodic timer, interval " << timer_period << "ms";
	}
#endif	
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
	if (timer_)
		SetThreadpoolTimer (timer_.get(), nullptr, 0, 0);
	timer_.reset();

/* Close SNMP agent. */
	snmp_agent_.reset();

/* Signal message pump thread to exit. */
	if ((bool)event_queue_)
		event_queue_->deactivate();
/* Drain and close event queue. */
	if ((bool)thread_)
		thread_->join();

/* Release everything with an RFA dependency. */
	thread_.reset();
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
	deregisterCommand (getId(), kFeedLogFunctionName);
	LOG(INFO) << "Unregistered Tcl API \"" << kFeedLogFunctionName << "\"";
	deregisterCommand (getId(), kBasicFunctionName);
	LOG(INFO) << "Unregistered Tcl API \"" << kBasicFunctionName << "\"";
	clear();
	LOG(INFO) << "Runtime summary: {"
		    " tclQueryReceived: " << cumulative_stats_[STITCH_PC_TCL_QUERY_RECEIVED] <<
		   ", timerQueryReceived: " << cumulative_stats_[STITCH_PC_TIMER_QUERY_RECEIVED] <<
		" }";
	LOG(INFO) << "Instance closed.";
	vpf::AbstractUserPlugin::destroy();
}

/* Tcl boilerplate.
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
	int retval = TCL_ERROR;
	TCLLibPtrs* tclStubsPtr = (TCLLibPtrs*)cmdData.mClientData;
	Tcl_Interp* interp = cmdData.mInterp;		/* Current interpreter. */
	const boost::posix_time::ptime t0 (boost::posix_time::microsec_clock::universal_time());
	last_activity_ = t0;

	cumulative_stats_[STITCH_PC_TCL_QUERY_RECEIVED]++;

	try {
		const char* command = cmdInfo.getCommandName();
		if (0 == strcmp (command, kBasicFunctionName))
			retval = tclHiloQuery (cmdInfo, cmdData);
		else if (0 == strcmp (command, kFeedLogFunctionName))
			retval = tclFeedLogQuery (cmdInfo, cmdData);
		else
			Tcl_SetResult (interp, "unknown function", TCL_STATIC);
	}
/* FlexRecord exceptions */
	catch (const vpf::PluginFrameworkException& e) {
		/* yay broken Tcl API */
		Tcl_SetResult (interp, (char*)e.what(), TCL_VOLATILE);
	}
	catch (...) {
		Tcl_SetResult (interp, "Unhandled exception", TCL_STATIC);
	}

/* Timing */
	const boost::posix_time::ptime t1 (boost::posix_time::microsec_clock::universal_time());
	const boost::posix_time::time_duration td = t1 - t0;
	DLOG(INFO) << "execute complete" << td.total_milliseconds() << "ms";
	if (td < min_tcl_time_) min_tcl_time_ = td;
	if (td > max_tcl_time_) max_tcl_time_ = td;
	total_tcl_time_ += td;

	return retval;
}

/* hilo_query <symbol-list> [startTime] [endTime]
 */
int
hilo::stitch_t::tclHiloQuery (
	const vpf::CommandInfo& cmdInfo,
	vpf::TCLCommandData& cmdData
	)
{
	TCLLibPtrs* tclStubsPtr = (TCLLibPtrs*)cmdData.mClientData;
	Tcl_Interp* interp = cmdData.mInterp;		/* Current interpreter. */
	int objc = cmdData.mObjc;			/* Number of arguments. */
	Tcl_Obj** CONST objv = cmdData.mObjv;		/* Argument strings. */

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
		auto rule = std::make_shared<hilo_t> ();
		assert ((bool)rule);
		if (len > 0 && parseRule (rule_text, *rule.get())) {
			query.push_back (rule);
			DLOG(INFO) << "#" << (1 + i) << " " << rule->name;
		} else {
			Tcl_SetResult (interp, "bad symbol list", TCL_STATIC);
			return TCL_ERROR;
		}
	}

	get_hilo (query, startTime, endTime);

/* Convert STL container result set into a new Tcl list. */
	Tcl_Obj* resultListPtr = Tcl_NewListObj (0, NULL);
	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t> it)
	{
		DLOG(INFO) << "Into result list name=" << it->name << " high=" << it->high << " low=" << it->low;

		const double high_rounded = bnymellon_round (it->high);
		const double low_rounded  = bnymellon_round (it->low);

		Tcl_Obj* elemObjPtr[] = {
			Tcl_NewStringObj (it->name.c_str(), -1),
			Tcl_NewDoubleObj (high_rounded),
			Tcl_NewDoubleObj (low_rounded),
		};
		Tcl_ListObjAppendElement (interp, resultListPtr, Tcl_NewListObj (_countof (elemObjPtr), elemObjPtr));
	});
	Tcl_SetObjResult (interp, resultListPtr);

	return TCL_OK;
}

class flexrecord_t {
public:
	flexrecord_t (const __time32_t& timestamp, const char* symbol, const char* record)
	{
		VHTime vhtime;
		struct tm tm_time = { 0 };
		
		VHTimeProcessor::TTTimeToVH ((__time32_t*)&timestamp, &vhtime);
		_gmtime32_s (&tm_time, &timestamp);

		stream_ << std::setfill ('0')
/* 1: timeStamp : t_string : server receipt time, fixed format: YYYYMMDDhhmmss.ttt, e.g. 20120114060928.227 */
			<< std::setw (4) << 1900 + tm_time.tm_year
			<< std::setw (2) << tm_time.tm_mon
			<< std::setw (2) << tm_time.tm_mday
			<< std::setw (2) << tm_time.tm_hour
			<< std::setw (2) << tm_time.tm_min
			<< std::setw (2) << tm_time.tm_sec
			<< '.'
			<< std::setw (3) << 0
/* 2: eyeCatcher : t_string : @@a */
			<< ",@@a"
/* 3: recordType : t_string : FR */
			   ",FR"
/* 4: symbol : t_string : e.g. MSFT */
			   ","
			<< symbol
/* 5: defName : t_string : FlexRecord name, e.g. Quote */
			<< ',' << record
/* 6: sourceName : t_string : FlexRecord name of base derived record. */
			<< ","
/* 7: sequenceID : t_u64 : Sequence number. */
			   "," << sequence_++
/* 8: exchTimeStamp : t_VHTime : exchange timestamp */
			<< ",V" << vhtime
/* 9: subType : t_s32 : record subtype */
			<< ","
/* 10..497: user-defined data fields */
			   ",";
	}

	std::string str() { return stream_.str(); }
	std::ostream& stream() { return stream_; }
private:
	std::ostringstream stream_;
	static uint64_t sequence_;
};

uint64_t flexrecord_t::sequence_ = 0;

/* hilo_feedlog <feedlog-file> <symbol-list> <interval> [startTime] [endTime]
 */
int
hilo::stitch_t::tclFeedLogQuery (
	const vpf::CommandInfo& cmdInfo,
	vpf::TCLCommandData& cmdData
	)
{
	TCLLibPtrs* tclStubsPtr = (TCLLibPtrs*)cmdData.mClientData;
	Tcl_Interp* interp = cmdData.mInterp;		/* Current interpreter. */
	int objc = cmdData.mObjc;			/* Number of arguments. */
	Tcl_Obj** CONST objv = cmdData.mObjv;		/* Argument strings. */

	if (objc < 2 || objc > 7) {
		Tcl_WrongNumArgs (interp, 1, objv, "feedLogFile symbolList interval ?startTime? ?endTime?");
		return TCL_ERROR;
	}

/* feedLogFile */
	int len = 0;
	char* feedlog_file = Tcl_GetStringFromObj (objv[1], &len);
	if (0 == len) {
		Tcl_SetResult (interp, "bad feedlog file", TCL_STATIC);
		return TCL_ERROR;
	}

	ms::handle file (CreateFile (feedlog_file,
				     GENERIC_WRITE,
				     0,
				     NULL,
				     CREATE_ALWAYS,
				     0,
				     NULL));
	if (!file) {
		LOG(WARNING) << "Failed to create file " << feedlog_file << " error code=" << GetLastError();
		return TCL_ERROR;
	}

	DLOG(INFO) << "feedLogFile=" << feedlog_file;

/* interval period */
	long interval;
	Tcl_GetLongFromObj (interp, objv[3], &interval);

	DLOG(INFO) << "interval=" << interval;

/* startTime if not provided is market open. */
	__time32_t start_time32;
	if (objc >= 4)
		Tcl_GetLongFromObj (interp, objv[4], &start_time32);
	else
		start_time32 = TBPrimitives::GetOpeningTime();

/* endTime if not provided is now. */
	__time32_t end_time32;
	if (objc >= 5)
		Tcl_GetLongFromObj (interp, objv[5], &end_time32);
	else
		end_time32 = TBPrimitives::GetCurrentTime();

/* Time must be ascending. */
	if (end_time32 <= start_time32) {
		Tcl_SetResult (interp, "endTime must be after startTime", TCL_STATIC);
		return TCL_ERROR;
	}

	DLOG(INFO) << "startTime=" << start_time32 << ", endTime=" << end_time32;

/* symbolList must be a list object.
 * NB: VA 7.0 does not export Tcl_ListObjGetElements()
 */
	int listLen, result = Tcl_ListObjLength (interp, objv[2], &listLen);
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

		Tcl_ListObjIndex (interp, objv[2], i, &objPtr);

		int len = 0;
		char* rule_text = Tcl_GetStringFromObj (objPtr, &len);
		auto rule = std::make_shared<hilo_t> ();
		assert ((bool)rule);
		if (len > 0 && parseRule (rule_text, *rule.get())) {
			query.push_back (rule);
			DLOG(INFO) << "#" << (1 + i) << " " << rule.get()->name;
		} else {
			Tcl_SetResult (interp, "bad symbol list", TCL_STATIC);
			return TCL_ERROR;
		}
	}

/* create timer iterator and walk through specified period dumping flexrecords to the feedlog */
	using namespace boost::posix_time;
	const ptime start (kUnixEpoch, seconds (start_time32));
	const ptime end (kUnixEpoch, seconds (end_time32));
	time_iterator time_it (start, seconds (interval));
	__time32_t interval_start_time32 (start_time32);
	while (++time_it <= end) {
		__time32_t interval_end_time32 = (*time_it - ptime (kUnixEpoch)).total_seconds();

		get_hilo (query, interval_start_time32, interval_end_time32);
		interval_start_time32 = interval_end_time32;
		
/* create flexrecord for each pair */
		std::for_each (query.begin(), query.end(),
			[&](std::shared_ptr<hilo_t> it)
		{
			std::ostringstream symbol_name;
			symbol_name << it->name << config_.suffix;

			const double high_rounded = bnymellon_round (it->high);
			const double low_rounded  = bnymellon_round (it->low);

			flexrecord_t fr (interval_end_time32, symbol_name.str().c_str(), kHiloFlexRecordName);
			fr.stream() << high_rounded
				    << ','
				    << low_rounded;

			DWORD written;
			std::string line (fr.str());
			line.append ("\r\n");
			const BOOL result = WriteFile (file.get(), line.c_str(), (DWORD)line.length(), &written, nullptr);
			if (!result || written != line.length()) {
				LOG(WARNING) << "Writing file " << feedlog_file << " failed, error code=" << GetLastError();
			}

/* do not reset analytic result set so next query extends previous result set */
		});
	}

	return TCL_OK;
}

void
hilo::stitch_t::processTimer (
	void*	pClosure
	)
{
	cumulative_stats_[STITCH_PC_TIMER_QUERY_RECEIVED]++;
	try {
		sendRefresh();
	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			"Severity: \"" << severity_string (e.getSeverity()) << "\""
			", Classification: \"" << classification_string (e.getClassification()) << "\""
			", StatusText: \"" << e.getStatus().getStatusText() << "\" }";
	}
}

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

	t = (reset_ptime - ptime (kUnixEpoch)).total_seconds();
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
	caster.as_integer = (next_ptime - ptime (kUnixEpoch)).total_microseconds() * 10; // upconvert to 100-nanos
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

	t = (end_ptime - ptime (kUnixEpoch)).total_seconds();
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
	__time32_t start_time32, end_time32;
	get_last_reset_time (start_time32);
	get_end_of_last_interval (end_time32);

	char start_str[CTIME_LENGTH], end_str[CTIME_LENGTH];
	_ctime32_s (start_str, _countof (start_str), &start_time32);
	_ctime32_s (end_str, _countof (end_str), &end_time32);
	start_str[CTIME_LENGTH - 2] = end_str[CTIME_LENGTH - 2] = '\0';
	LOG(INFO) << "refresh " << start_str << "-" << end_str;

	get_hilo (query_vector_, start_time32, end_time32);

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
	rfa::data::FieldEntry timeact_field (false), activ_date_field (false), price_field (false);
	rfa::data::DataBuffer timeact_data (false), activ_date_data (false), price_data (false);
	rfa::data::Real64 real64;
	rfa::data::Time rfaTime;
	rfa::data::Date rfaDate;
	struct tm _tm;

/* TIMEACT */
	timeact_field.setFieldID (kRdmTimeOfUpdateId);
	_gmtime32_s (&_tm, &end_time32);
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
		[&](std::shared_ptr<broadcast_stream_t>& stream)
	{
		attribInfo.setName (stream->rfa_name);
		it.start (fields_);
/* TIMACT */
		it.bind (timeact_field);

/* PRICE field is a rfa::Real64 value specified as <mantissa> × 10⁶.
 * Rfa deprecates setting via <double> data types so we create a mantissa from
 * source value and consider that we publish to 6 decimal places.
 */
/* HIGH_1 */
		price_field.setFieldID (kRdmTodaysHighId);
		const int64_t high_mantissa = bnymellon_mantissa (stream->hilo->high);
		real64.setValue (high_mantissa);		
		it.bind (price_field);
/* LOW_1 */
		price_field.setFieldID (kRdmTodaysLowId);
		const int64_t low_mantissa = bnymellon_mantissa (stream->hilo->low);
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
		provider_->send (*stream.get(), static_cast<rfa::common::Msg&> (response));
		DLOG(INFO) << stream->rfa_name << " high=" << stream->hilo->high << " low=" << stream->hilo->low;

/* reset analytic result set so next query starts from a blank state */
		stream->hilo->clear();
	});

/* create timer iterator and walk through specified period dumping flexrecords to the feedlog */
	const ptime start (kUnixEpoch, seconds (start_time32));
	const ptime end (kUnixEpoch, seconds (end_time32));
	const int interval_seconds = std::stoi (config_.interval);
	__time32_t interval_start_time32 (start_time32);
	for (time_iterator time_it (start, seconds (interval_seconds)); ++time_it <= end;)
	{
		__time32_t interval_end_time32 = (*time_it - ptime (kUnixEpoch)).total_seconds();

		get_hilo (query_vector_, interval_start_time32, interval_end_time32);
		interval_start_time32 = interval_end_time32;
		
/* create flexrecord for each pair */
		const time_duration interval_end_tod = time_it->time_of_day();
		std::ostringstream ss;
		ss << std::setfill ('0')
		   << '.'
		   << std::setw (2) << interval_end_tod.hours()
		   << std::setw (2) << interval_end_tod.minutes();

/* TIMEACT */
		_gmtime32_s (&_tm, &interval_end_time32);
		rfaTime.setHour   (_tm.tm_hour);
		rfaTime.setMinute (_tm.tm_min);
		rfaTime.setSecond (_tm.tm_sec);
		rfaTime.setMillisecond (0);

/* ACTIV_DATE */
		rfaDate.setDay   (/* rfa(1-31) */ _tm.tm_mday        /* tm(1-31) */);
		rfaDate.setMonth (/* rfa(1-12) */ 1 + _tm.tm_mon     /* tm(0-11) */);
		rfaDate.setYear  (/* rfa(yyyy) */ 1900 + _tm.tm_year /* tm(yyyy-1900 */);

		std::for_each (stream_vector_.begin(), stream_vector_.end(),
			[&](std::shared_ptr<broadcast_stream_t>& stream)
		{
			rfa::common::RFA_String rfa_name (stream->rfa_name);
			rfa_name.append (ss.str().c_str());
			attribInfo.setName (rfa_name);
			it.start (fields_);
/* TIMACT */
			it.bind (timeact_field);

/* PRICE field is a rfa::Real64 value specified as <mantissa> × 10⁶.
 * Rfa deprecates setting via <double> data types so we create a mantissa from
 * source value and consider that we publish to 6 decimal places.
 */
/* HIGH_1 */
			price_field.setFieldID (kRdmTodaysHighId);
			const int64_t high_mantissa = bnymellon_mantissa (stream->hilo->high);
			real64.setValue (high_mantissa);		
			it.bind (price_field);
/* LOW_1 */
			price_field.setFieldID (kRdmTodaysLowId);
			const int64_t low_mantissa = bnymellon_mantissa (stream->hilo->low);
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
			const std::string key (rfa_name.c_str());
			provider_->send (*stream->historical[key].get(), static_cast<rfa::common::Msg&> (response));
			DLOG(INFO) << rfa_name << " high=" << stream->hilo->high << " low=" << stream->hilo->low;

/* do not reset analytic result set so next query extends previous result set */

		});
	}

/* Publish a symbol list aka chain of all published historical item streams. */
	std::vector<std::string> link_times;
	link_times.reserve ((24 * 60 * 60) / interval_seconds);
	for (time_iterator time_it (start, seconds (interval_seconds)); ++time_it <= end;)
	{
		const time_duration interval_end_tod = time_it->time_of_day();
		std::ostringstream ss;
		ss << std::setfill ('0')
		   << '.'
		   << std::setw (2) << interval_end_tod.hours()
		   << std::setw (2) << interval_end_tod.minutes();
		link_times.push_back (ss.str());
	}

	assert (link_times.size() > 0);

	rfa::data::FieldEntry ref_count_field (false), link_field (false);
	rfa::data::DataBuffer ref_count_data (false), link_data (false);
	const size_t chain_index_max = link_times.size() / _countof (kRdmLinkId);

	ref_count_field.setFieldID (kRdmReferenceCountId);
	ref_count_field.setData (ref_count_data);
	link_field.setData (link_data);

	std::for_each (stream_vector_.begin(), stream_vector_.end(),
		[&](std::shared_ptr<broadcast_stream_t>& stream)
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
			const uint32_t ref_count = min (_countof (kRdmLinkId), (uint32_t)(link_times.size() - i));
			ref_count_data.setUInt32 (ref_count);
			it.bind (ref_count_field);
/* LONGLINKx */
			for (unsigned k = 0;
			     k < _countof (kRdmLinkId);
			     k++, i++)
			{
				link_field.setFieldID (kRdmLinkId[k]);
				if (i < link_times.size()) {
					RFA_String link_name (stream->rfa_name);
					link_name.append (link_times[i].c_str());
					link_data.setFromString (link_name, rfa::data::DataBuffer::StringRMTESEnum);
					it.bind (link_field);
				} else {
					RFA_String null ("", 0, false);
					link_data.setFromString (null, rfa::data::DataBuffer::StringRMTESEnum);
					it.bind (link_field);
				}
			} 
/* LONGPREVLR */
			link_field.setFieldID (kRdmPreviousLinkId);
			if (j > 0) {
				std::ostringstream ss;
				ss << (j - 1)
				   << '#'
				   << stream->rfa_name.c_str();
				RFA_String previous_name (ss.str().c_str(), (int)ss.str().length(), true);
				link_data.setFromString (previous_name, rfa::data::DataBuffer::StringRMTESEnum);
				it.bind (link_field);
			} else {
				RFA_String null ("", 0, false);
				link_data.setFromString (null, rfa::data::DataBuffer::StringRMTESEnum);
				it.bind (link_field);
			}
/* LONGNEXTLR */
			link_field.setFieldID (kRdmNextLinkId);
			if ((j + 1) < chain_index_max) {
				std::ostringstream ss;
				ss << (j + 1)
				   << '#'
				   << stream->rfa_name.c_str();
				RFA_String next_name (ss.str().c_str(), (int)ss.str().length(), true);
				link_data.setFromString (next_name, rfa::data::DataBuffer::StringRMTESEnum);
				it.bind (link_field);
			} else {
				RFA_String null ("", 0, false);
				link_data.setFromString (null, rfa::data::DataBuffer::StringRMTESEnum);
				it.bind (link_field);
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
				LOG(WARNING) << "respMsg::validateMsg: { warningText: \"" << warningText << "\" }";
			} else {
				assert (rfa::message::MsgValidationOk == validation_status);
			}
#endif
			const std::string key (rfa_name.c_str());
			provider_->send (*stream->chain[key].get(), static_cast<rfa::common::Msg&> (response));
		}

/* now reset result set */
		stream->hilo->clear();
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
