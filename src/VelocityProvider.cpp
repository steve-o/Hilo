#include "vpf/vpf.h"
#include "RDMHeaders.h"
#include "RDMDict.h"
#include "RDMDictionaryDecoder.h"

#include "VelocityProvider.h"
#include "EventQueueDispatcher.h"

#include <windows.h>
#include "TBUserDll.h"
#include "TBPrimitives.h"
#include "TBGenericRecordVector.h"

#include "ReqRouter.h"
#include "DictionaryGenerator.h"
#include "MarketPriceGenerator.h"
#include "InstrumentStreamItem.h"

#include "RDM/RDM.h"

#include <map>
#include <set>
#include <string>
#include <strstream>
#include <iostream>

using namespace std;
using namespace vpf;
using namespace xercesc;

using namespace rfa;
using namespace rfa::common;
using namespace rfa::config;
using namespace rfa::logger;
using namespace rfa::sessionLayer;
using namespace rfa::rdm;



VelocityProvider::VelocityProvider ()
: _mDispatcher(0), _fieldDefID(0)
{
	_frEvent = NULL;
	_frBase = NULL;
	_frData = NULL;
	_quoteEvent = NULL;
	_tradeEvent = NULL;
	_publishTradeRecords = false;
	_publishQuoteRecords = false;
	_publishFlexRecords  = false;
	_rfaConfigFile = NULL;
	_appConfigFile = NULL;
}

VelocityProvider::~VelocityProvider ()
{
	//this->destroy();
}

/*!
Method Name	    : init()

Description     : Parses the XML Configuration File, Initializes 
				  the RFA Session

Parameter(s)    : config - The Plugin Configuration provided by
				  SearchEngine.

Returns         : None

Exceptions      : UserPluginException

Internals       : Caller should free the buffer.
*/

void VelocityProvider::init (const UserPluginConfig& config)
{
	AbstractEventConsumer::init (config);

	// Get FlexRecord definitions 
	FlexRecDefinitionManager* frMgr = FlexRecDefinitionManager::GetInstance(0);
	vector<string> defNames;
	frMgr->GetAllDefinitionNames(defNames);

	// Create a map of FlexRecord definitions
	for (vector<string>::const_iterator iter = defNames.begin(); iter != defNames.end(); iter++)
	{
		const string& defName = *iter;
		FlexRecData* frData = new FlexRecData(defName.c_str());
		mFlexRecDataMap.insert(make_pair(frData->getDefinitionId(), frData));
	}

	try
	{
		// Read XML configuration
		parseXMLFile();

		if (!rfa::common::Context::initialize())
			throw UserPluginException ("VelocityProvider: Failed to initialize RFA");

		if (!initConfig(_rfaConfigFile, _appConfigFile))
           throw UserPluginException ("VelocityProvider: Failed to load RFA Config. Check rfaConfigDB and appConfigDB elements in XML");

        _appManager.eventQueue = rfa::common::EventQueue::create ("VelocityQueue");
        if (!_appManager.eventQueue)
            throw UserPluginException ("VelocityProvider: Failed to create RFA event queue");

		if (!initDomainManagers())
            throw UserPluginException ("VelocityProvider: Failed to initialize Domain Managers");

		if (!initProvider())
            throw UserPluginException ("VelocityProvider: Failed to initialize Client Manager");

		// Start the event dispatcher thread
		// ---------------------------------
		_mDispatcher = new EventQueueDispatcher (_appManager.eventQueue, mConfig->getDllHandle());
		_mDispatcher->start();

        MsgLog (eMsgInfo, 810931, "* VelocityProvider: Start publishing");
	}
	catch (const rfa::common::Exception& e)
	{
		char status[512];
		switch (e.getErrorType())
		{
			case rfa::common::Exception::InvalidUsageException:
				MsgLog (eMsgHigh, 1299387, "InvalidUsageException: %s", static_cast<const InvalidUsageException&>(e).getStatus().getStatusText().c_str());
				sprintf (status, "InvalidUsageException: %s", static_cast<const InvalidUsageException&>(e).getStatus().getStatusText().c_str());
				break;
			case rfa::common::Exception::InvalidConfigurationException:
				MsgLog (eMsgHigh, 1299519, "InvalidConfigurationException: %s", static_cast<const InvalidConfigurationException&>(e).getStatus().getStatusText().c_str());
				sprintf (status, "InvalidConfigurationException: %s", static_cast<const InvalidConfigurationException&>(e).getStatus().getStatusText().c_str());
				break;
			case rfa::common::Exception::SystemException:
				MsgLog (eMsgHigh, 1299523, "SystemException: %s", static_cast<const rfa::common::SystemException&>(e).getStatus().getStatusText().c_str());
				sprintf (status, "SystemException: %s", static_cast<const rfa::common::SystemException&>(e).getStatus().getStatusText().c_str());
				break;
			default:
				strcpy (status, "unknown");
				break;
		}
		
		char msg[ERRORMESSAGELENTH];
		sprintf (msg, "VelocityProvider: RFA exception: error type %d, severity %d, class %d, status: %s", e.getErrorType(), e.getServerity(), e.getClassification(), status);
		MsgLog (eMsgHigh, 393648, "* %s", msg);
		throw UserPluginException (msg);
	}
	catch (UserPluginException& upe)
	{
		//MsgLog(eMsgHigh, 000000, "%s", "Velocity Provider Init() failed");
		MsgLog(eMsgHigh, 1299539, "%s", upe.what());
		throw upe;
	}
}

/*!
Method Name	    : destroy()

Description     : Called when the plugin is unloaded.

Parameter(s)    : None

Returns         : None

Exceptions      : None

Internals       : None
*/

void VelocityProvider::destroy()
{
	// cleanup
	if (_clientManager)
		delete _clientManager;

    if (_loginManager)
        delete _loginManager;

    if (_directoryManager)
        delete _directoryManager;

    if (_dictionaryManager)
        delete _dictionaryManager;

    if (_marketPriceManager)
        delete _marketPriceManager;

    if (_authorizer)
        delete _authorizer;

    if (_serviceDirectory)
        delete _serviceDirectory;

    if (_dictionaryGenerator)
        delete _dictionaryGenerator;

    if (_marketPriceGenerator)
        delete _marketPriceGenerator;

    _appManager.destroy();

	if (rfa::common::Context::getInitializedCount() > 0)
	{
		if (!rfa::common::Context::uninitialize())
			MsgLog (eMsgHigh, 811088, "RFA Context fails to unitialize ... " );
	}

	_mDispatcher->stop();

	if (_mDispatcher)
        delete _mDispatcher;

	if (_rfaConfigFile)
		delete _rfaConfigFile;

	if(_appConfigFile)
		delete _appConfigFile;

	INT2FRDATA_MAP::const_iterator iter = mFlexRecDataMap.begin();
	while (iter != mFlexRecDataMap.end())
	{
		if(iter->second)
			delete(iter->second);
		iter++;
	}

    AbstractEventConsumer::destroy();
}

/*!
Method Name	    : proessEvent(vpf::Event* event)

Description     : For each event in the Pluginframework, the processEvent()
				  method is called.

Parameter(s)    : vpf::Event

Returns         : None

Exceptions      : None

Internals       : None
*/
void VelocityProvider::processEvent (vpf::Event* event)
{

	auto_ptr<vpf::Event> eventP(event);

	
	//Publishing FLEX Records 
	if (event->isOfType(vpf::FlexRecordEvent::getEventType()))
	{
		if (!_publishFlexRecords)
			return;

		// figure out which event and get the BLOB
		_frEvent = static_cast<FlexRecordEvent*>(event);
		_frBase = _frEvent->getFlexRecBase();
		//const BYTE* _frBlob = _frEvent->getFlexRecBlob();
		U32 defId = _frBase->fDefinitionId;

		// Search for the FlexRecord Definition in the map(created in init())
		INT2FRDATA_MAP::const_iterator iter = mFlexRecDataMap.find(defId);
		if (iter == mFlexRecDataMap.end()) {
			MsgLog(eMsgHigh, 52169, "Error while publishing: Failed to lookup FlexRecord definition id: %u", defId);
			return;
		}

		_frData = iter->second;	

		// Append Subject with ". + definitionName + Postfix (FR)"
		_subject = (std::string) _frEvent->getSymbolName() + (string)"." + _frData->getDefinitionName() + (string) "."+ _mSubjectFlexRecordPostfix;

	} else if (event->isOfType(QuoteEvent::getEventType()))
	{
		// Publishing QUOTE Records
		if (!_publishQuoteRecords)
			return;

		_quoteEvent = static_cast<QuoteEvent*>(event);

		// Append Subject with ". Postfix (default: Q)"
		_subject = _quoteEvent->getSymbol() + _mSubjectQuotePostfix;

	} else if (event->isOfType(TradeEvent::getEventType()))
	{
		// Publishing TRADE Records

		if (!_publishTradeRecords)
			return;

		_tradeEvent = static_cast<TradeEvent*>(event);

		// Append Subject with ". Postfix (default: T)"
		_subject = _tradeEvent->getSymbol() + _mSubjectTradePostfix;

	} else 
	{
		//Unknown Record, return
		return;
	}


	rfa::common::RFA_String itemName = _subject.c_str();

	AttribInfo attrib;
	attrib.setServiceName(_mServiceName);
	attrib.setName(itemName);
	attrib.setNameType(INSTRUMENT_NAME_RIC);

	InstrumentStreamItem *pSI = _marketPriceManager->getStreamItem(attrib);
	if (pSI)
	{
		RespMsg respMsg;
		respMsg.setAttribInfo(attrib);

		// SET the Data, Data extraction is done based on the event Type
		// and the _dataBuffer buffer is set.
		populateData(_subject, event);
		pSI->setData(_dataRfaString, _fieldDefID);
	
		pSI->generateRespMsg(RespMsg::UpdateEnum, &respMsg, 0 );

		// Create a OMMSolicitedItemCmd instance
		OMMSolicitedItemCmd itemCmd;
		itemCmd.setMsg(static_cast<rfa::common::Msg&>(respMsg));
		for (TOKENS::const_iterator it = pSI->getRequestTokens()->begin();
			it != pSI->getRequestTokens()->end();
			it ++ ) 
		{
			const RequestToken *pRequestToken = *it;
			itemCmd.setRequestToken(const_cast<RequestToken&>(*pRequestToken));
			if (_appManager.getOMMProvider())
				_appManager.getOMMProvider()->submit(&itemCmd);	
		}
	}
	// delete _frEvent;
}

/*!
Method Name	    : populateData

Description     : This method is responsible for extracting the data from
				  the events received and storing it in a rfa String.

Parameter(s)    : subject - the symbol for which data has to be extracted
				  event - the event passed by the PluginFramework. 

Returns         : None

Exceptions      : None

Internals       : None
*/
void VelocityProvider::populateData(std::string& subject, vpf::Event *event)
{
	if (event->isOfType(vpf::FlexRecordEvent::getEventType()))
	{
		// Fill the _dataBuffer for FlexRecord
		size_t len = sprintf(_dataBuffer, "%s,%I64d,%I64u,%I64d,%I64d,%u,%u,%u,%d,",
			subject.c_str(),
			_frBase->timeOffsetVT,
			_frBase->fSeqNum,
			_frBase->timeOffsetVT - _frBase->exchangeTimeDelta,
			_frBase->timeOffsetVT - _frBase->receiveTimeDelta,
			_frBase->timeSequenceNumber,
			0,  //_frBase->timeFieldsIndicator,
			0,  //_frBase->fFlags,
			0); //_frBase->fSubType);

		// populate string representation of user fields
		// note: toString may not work if length of all fields exceeds KMAXMESSAGELENGTH - len - 1
		_frBlob = _frEvent->getFlexRecBlob();
		_frTick = _frData->deblobToView(_frBlob);

		if (!_frTick)
		{
			MsgLog(eMsgHigh, 52177, "Error while publishing subject[%s]: Deblob failed", subject.c_str());
			return;
		}
		len += _frData->toString( &_dataBuffer[len], false );

		
		size_t len2 = sprintf(_dataBuffer2, ",%d,%s", _frData->getDefinitionId(), _frData->getDefinitionName() );
		if ( len + len2 < KMAXMESSAGELENGTH )
			strcat( _dataBuffer, _dataBuffer2 );

		//Create the RFA String
		_dataRfaString.set(_dataBuffer, strlen(_dataBuffer), false);
		_fieldDefID= 4000;

	}
	else if (event->isOfType(QuoteEvent::getEventType()))
	{
		// Fill the _dataBuffer for QuoteRecord
		_quoteEvent = static_cast<QuoteEvent*>(event);
		const TBQuoteRecord& quote = _quoteEvent->getQuoteRecord();
		U32 milliSecs;

		VHTimeProcessor::GetMilliSecPart(const_cast<VHTime*>(&quote.m_sTimeOffsetVT), &milliSecs);

		sprintf(_dataBuffer, "%s,%d,%g,%g,%I64d,%I64d,%s,%s,%d,%I64d,%s,%d,%s",
			subject.c_str(),
			quote.m_sTimeOffset,
			quote.m_fBidValue,
			quote.m_fAskValue,
			quote.bidSize,
			quote.askSize,
			quote.bidExchange,
			quote.askExchange,
			quote.quoteCond,
			quote.seqNum,
			quote.mmid,
			milliSecs,
			quote.extendedQuoteCondition);

		//Create the RFA String
		_dataRfaString.set(_dataBuffer, strlen(_dataBuffer), false);
		_fieldDefID= 4002;

	}
	else if (event->isOfType(TradeEvent::getEventType()))
	{
		// Fill the _dataBuffer for TradeRecord
		_tradeEvent = static_cast<TradeEvent*>(event);
		const TBTradeRecord& trade = _tradeEvent->getTradeRecord();
		U32 milliSecs;

		VHTimeProcessor::GetMilliSecPart(const_cast<VHTime*>(&trade.m_sTimeOffsetVT), &milliSecs);


		sprintf(_dataBuffer, "%s,%u,%g,%I64u,%g,%g,%d,%I64d,%I64d,%u,%I64d,%I64d,%s,%s,%s,%s,%d,%d,%d,%d",
			subject.c_str(),
			trade.m_sTimeOffset,
			trade.m_fPriceValue,
			trade.m_lTickVolume,
			trade.m_fBidValue,
			trade.m_fAskValue,
			trade.tickType,
			trade.seqNum,
			trade.refNum,
			trade.tickRType,
			trade.bidSize,
			trade.askSize,
			trade.tradeExchange,
			trade.bidExchange,
			trade.askExchange,
			trade.salesCondition,
			trade.openClosedIndicator,
			trade.hiLoIndicator,
			trade.mds127G,
			milliSecs);

		//Create the RFA String
		_dataRfaString.set(_dataBuffer, strlen(_dataBuffer), false);
		_fieldDefID= 4001;
	} else 
	{
		// Unknown Event. complete the record
		return;
	}
}

/*!
Method Name	    : initConfig()

Description     : Initializes the RFA Session based on the config files
				  (for ex: providerRFA.cfg and providerAPP.cfg) which are passed
				  in the XML File.

Parameter(s)    : rfaConfigFile - the configuration file for RFA Session
				  appConfigFile - the Application config file for RFA Session

Returns         : false - if RFA is unable to load from Configuration files.
				  true  - if RFA successfully loads from Configuration files.

Exceptions      : None

Internals       : None
*/

bool  VelocityProvider::initConfig(const char *rfaConfigFile,const char *appConfigFile)
{	
    _appManager.rfaConfigManager = new ConfigManager(RFA_String("RFA"));
    if (!_appManager.rfaConfigManager->initFile(rfaConfigFile))
        return false;
	
	//_appManager.rfaConfigManager->populateRFAConfig();
		
    _appManager.appConfigManager = new ConfigManager(RFA_String("VelocityProviderApp"));
    if (!_appManager.appConfigManager->initFile(appConfigFile))
        return false;

	//_appManager.appConfigManager->populateAPPConfig();

    return true;
}

/*!
Method Name	    : initDomainManagers

Description     : Initializes helper classes like RequestRouter, Authorizer, LoginManger,
					  DirectoryManger, etc.

Parameter(s)    : None

Returns         : false - if RFA is unable to create any of the helper Classes.
				  true  - if RFA successfully initialized all the helper Classes.

Exceptions      : None

Internals       : None
*/

bool  VelocityProvider::initDomainManagers()
{	
    _appManager.reqRouter = new ReqRouter();
	_authorizer = new Authorizer(_appManager);
	_loginManager = new LoginManager(_appManager, *_authorizer);
	_appManager.reqRouter->addDomainMgr(*_loginManager);

    _serviceDirectory = new ServiceDirectory(_appManager);
	_directoryManager = new DirectoryManager(_appManager, *_serviceDirectory);
	_appManager.reqRouter->addDomainMgr(*_directoryManager);

    if (!initDictionary())
		return false;

	_dictionaryGenerator = new DictionaryGenerator(_appManager);

	_dictionaryManager = new DictionaryManager(_appManager, *_dictionaryGenerator);
	_appManager.reqRouter->addDomainMgr(*_dictionaryManager);

    _marketPriceGenerator = new MarketPriceGenerator(_appManager);
    _marketPriceManager = new InstrumentManager(_appManager, MMT_MARKET_PRICE, *_marketPriceGenerator);
	_appManager.reqRouter->addDomainMgr(*_marketPriceManager);

    return true;
}

/*!
Method Name	    : initDictionary()

Description     : Initializes the RFA Dictionary from the file provided from the
				  configuration file (providerAPP.cfg)

Parameter(s)    : None

Returns         : false - if RFA is unable to load the dictionary file
				  true  - if RFA successfully loads the dictionary file

Exceptions      : None

Internals       : None
*/
bool  VelocityProvider::initDictionary()
{
 
	_appManager.rdmFieldDict = new RDMFieldDict();
	RDMFileDictionaryDecoder decoder(*(_appManager.rdmFieldDict));
	_mServiceName = _appManager.getAppConfigManager()->getString("ServiceName", "VTA");
    RFA_String fieldDictionaryFilename = _appManager.getAppConfigManager()->getString("FieldDictionaryFilename", "./SampleRDMFieldDictionary");
	RFA_String enumDictionaryFilename = _appManager.getAppConfigManager()->getString("EnumDictionaryFilename", "./SampleRDMEnumType.def");

	MsgLog (eMsgInfo, 1299952, "Velocity Provider: service name %s", _mServiceName.c_str());
	MsgLog (eMsgInfo, 1299953, "Velocity Provider: field Dictionary File is %s", fieldDictionaryFilename.c_str());
	MsgLog (eMsgInfo, 1299954, "Velocity Provider: enum Dictionary File is %s", enumDictionaryFilename.c_str());

	if( decoder.load(fieldDictionaryFilename, enumDictionaryFilename) )
	{
		_appManager.rdmFieldDict->setVersion("1.1");
		_appManager.rdmFieldDict->setDictId(1);
		_appManager.rdmFieldDict->enumDict().setVersion("1.1");
		_appManager.rdmFieldDict->enumDict().setDictId(1);
		MsgLog (eMsgInfo, 1299962, "Velocity Provider: Successfully Loaded Dictionary" );
		return true;
	}
	else
	{
		throw UserPluginException ("VelocityProvider: Unable to load the Dictionary File");
		MsgLog (eMsgHigh, 811231, "VelocityProvider: Failed to load dictionary" );

		return false;
	}
}

/*!
Method Name	    : initProvider()

Description     : The wrapper class which acquires RFA session and RFA client

Parameter(s)    : None

Returns         : false - if RFA is unable to acquire Session or if RFA is unable
				  to create the RFA Client.

				  true  - if RFA successfully acquires RFA Session and the RFA Client.

Exceptions      : None

Internals       : None
*/
bool VelocityProvider::initProvider()
{
	// Acquire a session
    RFA_String sessionName = _appManager.appConfigManager->getString("Session", "VelocitySession");
    _appManager.session = Session::acquire(sessionName);
	assert (_appManager.session);

	// create OMM managed provider
	_clientManager = new ClientManager(_appManager);
	assert (_clientManager);

	return _clientManager? true : false;
}

/*!
Method Name	    : parseXMLFile()

Description     : Parses the XML Configuration data. This data is available to the
				  plugin by the PluginFrameWork. This method also determines the 
				  subject PostFix passed by the user and the types of the records
				  to be published.

Parameter(s)    : None

Returns         : None

Exceptions      : None

Internals       : None
*/
void VelocityProvider::parseXMLFile()
{
	vpf::XMLStringPool xmlStr;
	const DOMNodeList* nodeList;
	const DOMElement* configElem = mConfig->getXmlConfigData ();

	nodeList = configElem->getElementsByTagName (xmlStr.transcode ("Config"));
	if (nodeList->getLength() < 1)
		throw UserPluginException ("VelocityProvider: Expecting one XML configuration element 'Config'");

	DOMElement* myCfg = static_cast<DOMElement*>(nodeList->item(0));
	
	const char* rfaConfigFile = xmlStr.transcode (myCfg->getAttribute (xmlStr.transcode ("rfaConfigDB")));
	const char* appConfigFile = xmlStr.transcode (myCfg->getAttribute (xmlStr.transcode ("appConfigDB")));

	if (rfaConfigFile && strlen(rfaConfigFile) != 0)
		_rfaConfigFile = new char[strlen(rfaConfigFile) + 1];
	else
		throw UserPluginException ("VelocityProvider: Expecting one XML configuration attribute in element 'Config': rfaConfigDB");


	if (appConfigFile && strlen(appConfigFile) != 0)
		_appConfigFile = new char [strlen(appConfigFile) + 1];
	else
		throw UserPluginException ("VelocityProvider: Expecting one XML configuration attribute in element 'Config': appConfigDB");

	strncpy( _rfaConfigFile, rfaConfigFile, strlen(rfaConfigFile)+1);
	strncpy( _appConfigFile, appConfigFile, strlen(appConfigFile)+1);

	// Get the postfix for the TradeConsumer if specified
	nodeList = configElem->getElementsByTagName (xmlStr.transcode ("TradeConsumer"));
	const char* tradePostFix= "T";
	if (nodeList->getLength() < 1)
	{
		//TradeConsumer Element not found. So we are not publishing Trade Records
		_publishTradeRecords = false;

	} else 
	{
		_publishTradeRecords = true;

		DOMElement* myTradeConsumerElem = static_cast<DOMElement*>(nodeList->item(0));
		tradePostFix =  xmlStr.transcode(myTradeConsumerElem->getTextContent());
		if (tradePostFix != NULL || StrCmp(tradePostFix, ""))
			_mSubjectTradePostfix = (std::string)"." + (std::string)tradePostFix; //_subject postfix from XML
		else
			_mSubjectTradePostfix = (std::string)"." + (std::string) "T"; // default is T
	}

	// Get the postfix for the QuoteConsumer if specified
	nodeList = configElem->getElementsByTagName (xmlStr.transcode ("QuoteConsumer"));
	const char* quotePostFix= "Q";
	if (nodeList->getLength() < 1)
	{
		//QuoteConsumer Element not found. So we are not publishing Quote Records
		_publishQuoteRecords = false;

	} else 
	{
		_publishQuoteRecords = true;

		DOMElement* myQuoteConsumerElem = static_cast<DOMElement*>(nodeList->item(0));
		quotePostFix =  xmlStr.transcode(myQuoteConsumerElem->getTextContent());
		if (quotePostFix != NULL || StrCmp(quotePostFix, ""))
			_mSubjectQuotePostfix = (std::string)"." + (std::string)quotePostFix; //_subject postfix from XML
		else
			_mSubjectQuotePostfix = (std::string)"." + (std::string) "Q"; // default is Q
	}


	nodeList = configElem->getElementsByTagName (xmlStr.transcode ("FlexRecordConsumer"));
	if (nodeList->getLength() < 1)
	{
		//FlexRecordConsumer Element not found. So we are not publishing  Flex Records
		_publishFlexRecords = false;

	} else 
	{
		_publishFlexRecords = true;
	}

	_mSubjectFlexRecordPostfix = "FR"; 

}


class VelocityProviderFactory : public ObjectFactory
{
public:
	VelocityProviderFactory() {
		registerType("VelocityProvider");
	}
    virtual void * newInstance(const char* type) {
		return new VelocityProvider();
	}
};

static VelocityProviderFactory sFactoryInstance;
