#ifndef _DICTIONARY_MANAGER_H
#define _DICTIONARY_MANAGER_H

#include "DomainManager.h"
#include "SessionLayer/OMMSolicitedItemCmd.h"
#include "Message/RespMsg.h"
#include <list>

class DictionaryGenerator;
class DictionaryStreamItem;
class StreamItem;

class DictionaryManager : public DomainManager
{
public:
	DictionaryManager(ApplicationManager & appManager, DictionaryGenerator& generator);
	virtual ~DictionaryManager();

	virtual void processReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & msg);
	virtual void processCloseReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
	virtual void processReReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);

protected:
	void sendRefresh( rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & reqMsg, DictionaryStreamItem & dictionaryStreamItem );

	void encodeMsg(rfa::message::RespMsg* respMsg, const rfa::message::AttribInfo & rAttribInfo,
		rfa::common::RespStatus & rRStatus);

	void cleanupStreamItem(DictionaryStreamItem * dstream);
	DictionaryStreamItem * findStream(const rfa::message::AttribInfo & attribInfo);

	std::list<DictionaryStreamItem *>			_dictionaryStreams;
	DictionaryGenerator&						_dictionaryGenerator;
	rfa::sessionLayer::OMMSolicitedItemCmd		_cmd;
	rfa::message::RespMsg						_respMsg;

private:
	DictionaryManager& operator=( const DictionaryManager& );
};

#endif

