#include "DictionaryManager.h"

#include <iostream>

#include "RDM/RDM.h"
#include "Common/RespStatus.h"
#include "Message/ReqMsg.h"
#include "SessionLayer/RequestToken.h"
#include "SessionLayer/OMMProvider.h"

#include "RDMDict.h"
#include "RDMDictionaryDecoder.h"
#include "RDMDictionaryEncoder.h"
#include "DictionaryStreamItem.h"
#include "DictionaryGenerator.h"
#include "ApplicationManager.h"
#include "SessionManager.h"
// #include "RDMUtil.h"

using namespace std;
using namespace rfa::common;
using namespace rfa::message;
using namespace rfa::sessionLayer;
using namespace rfa::rdm;


DictionaryManager::DictionaryManager(ApplicationManager & appManager, DictionaryGenerator& generator)
: DomainManager(appManager, MMT_DICTIONARY),_dictionaryGenerator(generator),
_respMsg(),
_cmd()
{
	_dictionaryGenerator.setDomainManager(*this);
}

DictionaryManager::~DictionaryManager()
{
}

void DictionaryManager::processCloseReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
	if (streamItem.getNumOfRequestTokens() == 0)
		cleanupStreamItem(static_cast<DictionaryStreamItem*>(&streamItem));
}

void DictionaryManager::processReReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  StreamItem & streamItem,
									  const ReqMsg & reqMsg)
{
    UInt8 interactionType = reqMsg.getInteractionType();
	if (!(interactionType & ReqMsg::InitialImageFlag ) && (interactionType & ReqMsg::InterestAfterRefreshFlag))
	{
		// Dictionaries are not pre-empted, ignore priority changes
		return;
	}
	// else wants new image
	
	if (streamItem.satisfies(reqMsg.getAttribInfo()))
	{
		sendRefresh(token, reqMsg, static_cast<DictionaryStreamItem&>(streamItem));
	}
	else // request parameters changed
	{
			// close the old stream from the perspective of the sessionManager
		streamItem.removeRequestToken(token);
		sessionManager.removeStreamItem(token);
		
		// search for/create new stream item - same as processReqMsg()
		DictionaryStreamItem * dictionaryStreamItem = findStream(reqMsg.getAttribInfo());
		if (dictionaryStreamItem)
		{
			dictionaryStreamItem->addRequestToken( token );
			sessionManager.addStreamItem( token, *dictionaryStreamItem );
			sendRefresh(token, reqMsg, *dictionaryStreamItem);
		}
		else
			sendCloseStatus(token, reqMsg.getAttribInfo());
	
		// cleanup old stream - same as processCloseReqMsg()
		if (streamItem.getNumOfRequestTokens() == 0)
			cleanupStreamItem(static_cast<DictionaryStreamItem*>(&streamItem));
	}
}

void DictionaryManager::processReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  const rfa::message::ReqMsg & reqMsg)
{
	DictionaryStreamItem * dictionaryStreamItem = findStream(reqMsg.getAttribInfo());
	if (dictionaryStreamItem)
	{
		dictionaryStreamItem->addRequestToken( token );
		sessionManager.addStreamItem( token, *dictionaryStreamItem );
		sendRefresh(token, reqMsg, *dictionaryStreamItem);
	}
	else
		sendCloseStatus(token, reqMsg.getAttribInfo());
}

void DictionaryManager::sendRefresh( RequestToken & token, const ReqMsg & reqMsg, DictionaryStreamItem & dictionaryStreamItem )
{
	_respMsg.clear();

	_respMsg.setMsgModelType( MMT_DICTIONARY );	
	rfa::message::RespMsg::RespType  respType= RespMsg::RefreshEnum;

	_respMsg.setRespType( respType );
	_respMsg.setIndicationMask( RespMsg::RefreshCompleteFlag );
	_respMsg.setRespTypeNum( REFRESH_SOLICITED );
	_respMsg.setAttribInfo( reqMsg.getAttribInfo() );
	
	RespStatus rStatus;
	rStatus.setStreamState( RespStatus::OpenEnum );
	rStatus.setDataState( RespStatus::OkEnum );	
	rStatus.setStatusCode( RespStatus::NoneEnum );
	
	_respMsg.setIndicationMask( (UInt8)(_respMsg.getIndicationMask() | RespMsg::RefreshCompleteFlag) );    
	RFA_String  tmpStr;
	tmpStr.set( "RequestCompleted", 16, false );
	rStatus.setStatusText( tmpStr );
	_respMsg.setRespStatus( rStatus );
	
	Series & series = _dictionaryGenerator.reusableSeries();
	series.clear();
	dictionaryStreamItem.createData( series, respType );
	_respMsg.setPayload( series );
	_cmd.setMsg( _respMsg );
	_cmd.setRequestToken( token );

	appManager.getOMMProvider()->submit( &_cmd );
}

void DictionaryManager::cleanupStreamItem(DictionaryStreamItem * dstream)
{
	list<DictionaryStreamItem *>::iterator iter = _dictionaryStreams.begin();
	for (;iter != _dictionaryStreams.end(); iter++)
	{
		if (*iter != dstream) 
			break;
	}
	_dictionaryStreams.erase(iter);
	_dictionaryGenerator.removeStreamItem(*dstream);
}

DictionaryStreamItem * DictionaryManager::findStream(const AttribInfo & attribInfo)
{
	for (list<DictionaryStreamItem *>::const_iterator iter = _dictionaryStreams.begin();
		iter != _dictionaryStreams.end(); iter++)
	{
		DictionaryStreamItem * item = *iter;
		if (item->satisfies(attribInfo))
			return item;
	}
	StreamItem * streamItem = _dictionaryGenerator.createStreamItem(attribInfo);
	if (streamItem)
	{
		DictionaryStreamItem * dictionaryStreamItem = static_cast<DictionaryStreamItem *>(streamItem);
		_dictionaryStreams.push_back(dictionaryStreamItem);
		return dictionaryStreamItem;
	}
	else
		return 0;
}
