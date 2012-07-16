/* Tcl command exports
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
#include "error.hh"
#include "rfa_logging.hh"
#include "rfaostream.hh"
#include "bnymellon.hh"

/* Feed log file FlexRecord name */
static const char* kHiloFlexRecordName = "Hilo";

/* Tcl exported API. */
static const char* kBasicFunctionName = "hilo_query";
static const char* kFeedLogFunctionName = "hilo_feedlog";
static const char* kRepublishFunctionName = "hilo_republish";

static const char* kTclApi[] = {
	kBasicFunctionName,
	kFeedLogFunctionName,
	kRepublishFunctionName
};

/* http://en.wikipedia.org/wiki/Unix_epoch */
static const boost::gregorian::date kUnixEpoch (1970, 1, 1);

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

/* Register Tcl API.
 */
bool
hilo::stitch_t::register_tcl_api (const char* id)
{
	for (size_t i = 0; i < _countof (kTclApi); ++i) {
		registerCommand (id, kTclApi[i]);
		LOG(INFO) << "Registered Tcl API \"" << kTclApi[i] << "\"";
	}
	return true;
}

/* Unregister Tcl API.
 */
bool
hilo::stitch_t::unregister_tcl_api (const char* id)
{
	for (size_t i = 0; i < _countof (kTclApi); ++i) {
		deregisterCommand (id, kTclApi[i]);
		LOG(INFO) << "Unregistered Tcl API \"" << kTclApi[i] << "\"";
	}
	return true;
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
	TCLLibPtrs* tclStubsPtr = static_cast<TCLLibPtrs*> (cmdData.mClientData);
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
		else if (0 == strcmp (command, kRepublishFunctionName))
			retval = tclRepublishQuery (cmdInfo, cmdData);
		else
			Tcl_SetResult (interp, "unknown function", TCL_STATIC);
	}
/* FlexRecord exceptions */
	catch (const vpf::PluginFrameworkException& e) {
		/* yay broken Tcl API */
		Tcl_SetResult (interp, const_cast<char*> (e.what()), TCL_VOLATILE);
	}
	catch (...) {
		Tcl_SetResult (interp, "Unhandled exception", TCL_STATIC);
	}

/* Timing */
	const boost::posix_time::ptime t1 (boost::posix_time::microsec_clock::universal_time());
	const boost::posix_time::time_duration td = t1 - t0;
	DLOG(INFO) << "execute complete " << td.total_milliseconds() << "ms";
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
	TCLLibPtrs* tclStubsPtr = static_cast<TCLLibPtrs*> (cmdData.mClientData);
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

	single_iterator::get_hilo (query, startTime, endTime);

/* Convert STL container result set into a new Tcl list. */
	Tcl_Obj* resultListPtr = Tcl_NewListObj (0, NULL);
	std::for_each (query.begin(), query.end(),
		[&](std::shared_ptr<hilo_t> it)
	{
		DLOG(INFO) << "Into result list name=" << it->name << " high=" << it->high << " low=" << it->low;

		const double high_rounded = bnymellon::round (it->high);
		const double low_rounded  = bnymellon::round (it->low);

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
		
		VHTimeProcessor::TTTimeToVH (const_cast<__time32_t*> (&timestamp), &vhtime);
		_gmtime32_s (&tm_time, &timestamp);

		stream_ << std::setfill ('0')
/* 1: timeStamp : t_string : server receipt time, fixed format: YYYYMMDDhhmmss.ttt, e.g. 20120114060928.227 */
			<< std::setw (4) << 1900 + tm_time.tm_year
			<< std::setw (2) << 1 + tm_time.tm_mon
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
	TCLLibPtrs* tclStubsPtr = static_cast<TCLLibPtrs*> (cmdData.mClientData);
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

/* create time period for bar and shift x-minutes for the specified range */
	using namespace boost::posix_time;

	time_period tp (ptime (kUnixEpoch, seconds (start_time32)), seconds (interval));

	while (true) {
		const __time32_t from = to_unix_epoch<__time32_t> (tp.begin());
		const __time32_t till = to_unix_epoch<__time32_t> (tp.end());

/* inclusive of specified end time */
		if (till > end_time32)
			break;

/* reset bars */
		std::for_each (query.begin(), query.end(), [](std::shared_ptr<hilo_t>& hilo) {
			hilo->clear();
		});

		single_iterator::get_hilo (query, from, till);
		
/* create flexrecord for each pair */
		std::for_each (query.begin(), query.end(),
			[&](std::shared_ptr<hilo_t> it)
		{
			std::ostringstream symbol_name;
			symbol_name << it->name << config_.suffix;

			const double high_rounded = bnymellon::round (it->high);
			const double low_rounded  = bnymellon::round (it->low);

			flexrecord_t fr (till, symbol_name.str().c_str(), kHiloFlexRecordName);
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

		tp.shift (seconds (interval));
	};

	return TCL_OK;
}

/* hilo_republish
 */
int
hilo::stitch_t::tclRepublishQuery (
	const vpf::CommandInfo& cmdInfo,
	vpf::TCLCommandData& cmdData
	)
{
	TCLLibPtrs* tclStubsPtr = static_cast<TCLLibPtrs*> (cmdData.mClientData);
	Tcl_Interp* interp = cmdData.mInterp;		/* Current interpreter. */
/* Refresh already running.  Note locking is handled outside query to enable
 * feedback to Tcl interface.
 */
	boost::unique_lock<boost::shared_mutex> lock (query_mutex_, boost::try_to_lock_t());
	if (!lock.owns_lock()) {
		Tcl_SetResult (interp, "query already running", TCL_STATIC);
		return TCL_ERROR;
	}

	try {
		sendRefresh();
	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			  "\"Severity\": \"" << severity_string (e.getSeverity()) << "\""
			", \"Classification\": \"" << classification_string (e.getClassification()) << "\""
			", \"StatusText\": \"" << e.getStatus().getStatusText() << "\" }";
	}
	return TCL_OK;
}

/* eof */