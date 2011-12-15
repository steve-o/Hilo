﻿/* RFA publisher plugin for Velocity Analytics Engine.
 */

#include "temp.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include "error.hh"

/* RDM Usage Guide: Section 6.5: Enterprise Platform
 * For future compatibility, the DictionaryId should be set to 1 by providers.
 * The DictionaryId for the RDMFieldDictionary is 1.
 */
static const int kDictionaryId = 1;

/* RDM: Absolutely no idea. */
static const int kFieldListId = 3;

/* FlexRecord Quote identifier. */
static const uint32_t kQuoteId = 40002;

using rfa::common::RFA_String;

temp::temp_t::temp_t() :
	is_shutdown_ (false),
	quote_flexrecord_ (nullptr),
	rfa_ (nullptr),
	event_queue_ (nullptr),
	log_ (nullptr),
	provider_ (nullptr),
	event_pump_ (nullptr),
	thread_ (nullptr),
	flexrecord_event_count_ (0),
	ignored_flexrecord_count_ (0),
	corrupt_flexrecord_count_ (0),
	discarded_event_count_ (0)
{
}

temp::temp_t::~temp_t()
{
	clear();
}

/* Plugin entry point from the Velocity Analytics Engine.
 */

void
temp::temp_t::init (
	const vpf::UserPluginConfig& vpf_config
	)
{
/* Thunk to VA consumer base class. */
	vpf::AbstractEventConsumer::init (vpf_config);

	LOG(INFO) << config_;

/* Retrieve FlexRecord dictionary. */
	if (!processFlexRecordDictionary())
		goto cleanup;

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

/* Create state for published RIC. */
		static const char* msft = "MSFT.O";
		if (!provider_->createItemStream (msft, msft_stream_))
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

	return;
cleanup:
	clear();
	is_shutdown_ = true;
	throw vpf::UserPluginException ("Init failed.");
}

void
temp::temp_t::clear()
{
/* Signal message pump thread to exit. */
	if (nullptr != event_queue_)
		event_queue_->deactivate();
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
temp::temp_t::destroy()
{
	clear();
	LOG(INFO) << "Runtime summary: { "
		"FlexRecordEvents: " << flexrecord_event_count_ <<
		", DiscardedEvents: " << discarded_event_count_ << " }";
	vpf::AbstractEventConsumer::destroy();
}

/* Analytic Engine event handler with RAII.
 */

void
temp::temp_t::processEvent (
	vpf::Event*	vpf_event_
	)
{
/* Ignore all events if startup failed, or significant runtime error occurred. */
	if (is_shutdown_) return;

	CHECK (nullptr != vpf_event_);
	std::unique_ptr<vpf::Event> vpf_event (vpf_event_);

	if (!vpf_event->isOfType (vpf::FlexRecordEvent::getEventType())) {
		discarded_event_count_++;
		return;
	}

/* move to derived class */
	std::unique_ptr<vpf::FlexRecordEvent> fr_event (static_cast<vpf::FlexRecordEvent*> (vpf_event.release()));

	processFlexRecord (std::move (fr_event));
	flexrecord_event_count_++;
}

/* Incoming FlexRecord formatted update.
 */

void
temp::temp_t::processFlexRecord (
	std::unique_ptr<vpf::FlexRecordEvent> fr_event
	)
{
//	LOG(INFO) << "Event: { "
//		"SymbolName: \"" << fr_event->getSymbolName() << "\" }";

/* Verify record id is a Quote FlexRecord.
 * FlexRecBaseTickData cannot be nullptr by implementation.
 */
	if (kQuoteId != fr_event->getFlexRecBase()->fDefinitionId) {
		ignored_flexrecord_count_++;
		discarded_event_count_++;
		return;
	}

/* Decompress the content using the FlexRecord definition.
 * FlexRecBlob will assumed not be nullptr but subject to ctor.
 */
	VarFieldsView* view = quote_flexrecord_->deblobToView (fr_event->getFlexRecBlob());
	if (nullptr == view) {
		LOG(WARNING) << "FlexRecord unpack failed for symbol name '" << fr_event->getSymbolName() << "'";
		corrupt_flexrecord_count_++;
		discarded_event_count_++;
		return;
	}

/* 7.5.9.1 Create a response message (4.2.2) */
	rfa::message::RespMsg response;

/* 7.5.9.2 Set the message model type of the response. */
	response.setMsgModelType (rfa::rdm::MMT_MARKET_PRICE);
/* 7.5.9.3 Set response type. */
	response.setRespType (rfa::message::RespMsg::RefreshEnum);
	response.setIndicationMask (response.getIndicationMask() | rfa::message::RespMsg::RefreshCompleteFlag);
/* 7.5.9.4 Set the response type enumation. */
	response.setRespTypeNum (rfa::rdm::REFRESH_UNSOLICITED);

/* 7.5.9.5 Create or re-use a request attribute object (4.2.4) */
	rfa::message::AttribInfo attribInfo;
	attribInfo.setNameType (rfa::rdm::INSTRUMENT_NAME_RIC);
	RFA_String service_name (config_.service_name.c_str(), 0, false);
	attribInfo.setName (msft_stream_.name);
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
	{
// not std::map :(  derived from rfa::common::Data
		fields_.setAssociatedMetaInfo (provider_->getRwfMajorVersion(), provider_->getRwfMinorVersion());
		fields_.setInfo (kDictionaryId, kFieldListId);

		rfa::data::FieldListWriteIterator it;
		it.start (fields_);

		static const int kFlexBidPriceId = 24, kRdmBidPriceId = 22;
		static const int kFlexAskPriceId = 30, kRdmAskPriceId = 25;

static const int kRdmRdnDisplayId = 2;		/* RDNDISPLAY */
static const int kRdmTradePriceId = 6;		/* TRDPRC_1 */

		rfa::data::FieldEntry field;
		rfa::data::DataBuffer dataBuffer;
		rfa::data::Real64 real64;

		const int fieldCount = quote_flexrecord_->getFieldCount();

		for (int i = kFRFixedFields, j = 2 /* copy only two fields */;
			i < fieldCount;
			++i)
		{
			if ((0 == view[i].length) || (view[i].theType != VarFieldsEnums::t_double))
				continue;

			switch (view[i].fieldId) {
			case kFlexBidPriceId:	field.setFieldID (kRdmBidPriceId); break;
			case kFlexAskPriceId:	field.setFieldID (kRdmAskPriceId); break;
/* Ignore all other fields. */
			default: continue;
			}

/* PRICE field is a rfa::Real64 value specified as <mantissa> × 10⁴.
 * Rfa deprecates setting via <double> data types so we create a mantissa from
 * source value and consider that we publish to 4 decimal places.
 */
			const int64_t mantissa = (int64_t)(*(const double*)view[i].data * 1000.0);
			real64.setValue (mantissa);
			real64.setMagnitudeType (rfa::data::ExponentNeg4);
			dataBuffer.setReal64 (real64);
			field.setData (dataBuffer);
			it.bind (field);
			if (0 == --j) break;
/* FieldEntry::clear() is optional. */
/* DataBuffer::clear() is optional. */
		}

{
		field.setFieldID (kRdmRdnDisplayId);
		dataBuffer.setUInt32 (100);
		field.setData (dataBuffer);
		it.bind (field);

		field.setFieldID (kRdmTradePriceId);
		real64.setValue (++msft_stream_.count);
		real64.setMagnitudeType (rfa::data::ExponentNeg2);
		dataBuffer.setReal64 (real64);
		it.bind (field);
}

		it.complete();
		response.setPayload (fields_);
	}

	rfa::common::RespStatus status;
/* Item interaction state: Open, Closed, ClosedRecover, Redirected, NonStreaming, or Unspecified. */
	status.setStreamState (rfa::common::RespStatus::OpenEnum);
/* Data quality state: Ok, Suspect, or Unspecified. */
	status.setDataState (rfa::common::RespStatus::OkEnum);
/* Error code, e.g. NotFound, InvalidArgument, ... */
	status.setStatusCode (rfa::common::RespStatus::NoneEnum);
	response.setRespStatus (status);

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

	provider_->send (msft_stream_, static_cast<rfa::common::Msg&> (response));
//	LOG(INFO) << "sent";
}

/* Enumerate the FlexRecord dictionary from the Analytic Engine process.
 */

bool
temp::temp_t::processFlexRecordDictionary()
{
	FlexRecDefinitionManager* manager = FlexRecDefinitionManager::GetInstance (nullptr);
	std::vector<std::string> names;

	manager->GetAllDefinitionNames (names);

	for (auto it = names.begin();
	     it != names.end();
	     ++it)
	{
		std::unique_ptr<vpf::FlexRecData> data (new vpf::FlexRecData (it->c_str()));
		flexrecord_map_.insert (make_pair (data->getDefinitionId(), std::move (data)));
	}
	LOG(INFO) << "FlexRecord dictionary with " << flexrecord_map_.size() << " entries";

	CHECK(!flexrecord_map_.empty());

/* Verify we have the FlexRecord Quote record. */
	const auto it = flexrecord_map_.find (kQuoteId);
	CHECK(it != flexrecord_map_.end());

/* FlexRecord definition from the dictionary. */
	quote_flexrecord_ = it->second.get();
	CHECK(nullptr != quote_flexrecord_);
	return true;
}

/* eof */
