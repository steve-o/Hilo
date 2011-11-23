#ifndef _INSTRUMENT_MANAGER_H
#define _INSTRUMENT_MANAGER_H

#include "Common/RFA_String.h"
#include "DomainManager.h"
#include "SessionLayer/OMMSolicitedItemCmd.h"
#include <list>
#include <map>

class DataGenerator;
class InstrumentStreamItem;
class ApplicationManager;

class InstrumentManager : public DomainManager
{
public:
	InstrumentManager(ApplicationManager & context, rfa::common::UInt8 messageModelType, DataGenerator& dataGenerator);
	virtual ~InstrumentManager();

	virtual void processReqMsg(SessionManager & sessionManager, 
		rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & msg);
	virtual void processCloseReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
	virtual void processReReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
	
	bool				  isValidRequest(const rfa::message::AttribInfo& attrib);
	InstrumentStreamItem* getStreamItem(const rfa::message::AttribInfo& attrib);
protected:
	DataGenerator&				_dataGen;

	InstrumentStreamItem*	isNewRequest(const rfa::message::AttribInfo&  attrib);
	InstrumentStreamItem*	getSnapshotItem(const rfa::message::AttribInfo& attrib);

	typedef	std::list<InstrumentStreamItem*>				INSTRUMENT_SI_LIST;
	typedef std::map<std::string,INSTRUMENT_SI_LIST*>		SERVICE_SI_LIST;
	SERVICE_SI_LIST										_srcInstrumentSIs;

	bool												_isNewRequest;
	rfa::sessionLayer::OMMSolicitedItemCmd				_itemCmd;
};

#endif
