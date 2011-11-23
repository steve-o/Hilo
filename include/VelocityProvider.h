#ifndef _VELOCITYPROVIDER_H
#define _VELOCITYPROVIDER_H

#include "vpf/vpf.h"

#include <windows.h>
#include "TBUserDll.h"

#include <map>
#include <set>
#include <string>

#include "ApplicationManager.h"
#include "LoginManager.h"
#include "Authorizer.h"
#include "ClientManager.h"
#include "ConfigManager.h"
#include "ServiceDirectory.h"
#include "DirectoryManager.h"
#include "DictionaryManager.h"
#include "InstrumentManager.h"
#include "EventQueueDispatcher.h"
#include "MarketPriceGenerator.h"
#include "FlexRecDefinitions.h"
#include "FlexRecBinding.h"


//The Maximum Message Length of a FlexRecord.
#define KMAXSUBJECTLENGTH	256
#define KMAXDATALENGTH		512
#define KMAXHEADERLENGTH	512
#define KMAXTRAILERLENGTH	64
#define KMAXMESSAGELENGTH	4096-KMAXHEADERLENGTH-KMAXTRAILERLENGTH-16
#define ERRORMESSAGELENTH   1024


/*!
Class Name	    : VelocityProvider

Description	    : Publishes FlexRecord, Quote, Trade events
				  from PluginFramework to RFA.
                  
Base class(es)  : None

Exceptions      : None

API             : None

Internals       : Caller has to delete the buffer.

*/

class VelocityProvider : public vpf::AbstractEventConsumer
{
public:
	VelocityProvider();
	virtual ~VelocityProvider();
	virtual void init(const vpf::UserPluginConfig& config);
    virtual void destroy();
    virtual void processEvent(vpf::Event* event);

	bool  initConfig(const char *rfaConfigFile, const char *appConfigFile);
	bool  initDomainManagers();
	bool  initProvider();
	bool  initDictionary();
	void  populateData(std::string& subject, vpf::Event *event);

private:
	void parseXMLFile();


private:

#pragma region member Variables

	ApplicationManager		_appManager;
   	Authorizer*				_authorizer;
	LoginManager*			_loginManager;
	DictionaryManager*		_dictionaryManager;
  	ClientManager*			_clientManager;
	ServiceDirectory*		_serviceDirectory;
	DirectoryManager*		_directoryManager;	
    InstrumentManager*		_marketPriceManager;
   	DictionaryGenerator*	_dictionaryGenerator;
	MarketPriceGenerator*	_marketPriceGenerator;
    EventQueueDispatcher*	_mDispatcher;

	//Map of FlexRecord Definitions
	typedef map<U32, vpf::FlexRecData*>	INT2FRDATA_MAP;
	INT2FRDATA_MAP					mFlexRecDataMap;

	// This is the postfix for a published symbol
	std::string _mSubjectFlexRecordPostfix;
	std::string _mSubjectTradePostfix;
	std::string _mSubjectQuotePostfix;

	//The following variables are required extracting data from
	// FlexRecord, Quote, Trade events
	char						_dataBuffer[KMAXMESSAGELENGTH];
	char						_dataBuffer2[128];
	RFA_String					_dataRfaString;
	vpf::FlexRecordEvent*		_frEvent;
	const FlexRecBaseTickData*	_frBase ;
	vpf::FlexRecData*			_frData;
	const BYTE*					_frBlob;
	VarFieldsView*				_frTick;
	vpf::QuoteEvent*			_quoteEvent;
	vpf::TradeEvent*			_tradeEvent;
	std::string					_subject;
	int							_fieldDefID;
	bool						_publishTradeRecords;
	bool						_publishQuoteRecords;
	bool						_publishFlexRecords;
	char*						_rfaConfigFile;
	char*						_appConfigFile;

#pragma endregion member Variables

};
#endif