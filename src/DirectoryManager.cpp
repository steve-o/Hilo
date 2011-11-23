#include "DirectoryManager.h"


#include "RDM/RDM.h"
#include <assert.h>
#include <iostream>

#include "RDM/RDM.h"
#include "Config/ConfigTree.h"
#include "Common/Msg.h"
#include "Data/Map.h"
#include "Message/RespMsg.h"
#include "Message/ReqMsg.h"
#include "Message/Manifest.h"
#include "Message/AttribInfo.h"
#include "SessionLayer/RequestToken.h"
#include "SessionLayer/OMMSolicitedItemCmd.h"
#include "SessionLayer/OMMProvider.h"

#include "ConfigManager.h"
#include "SessionManager.h"
#include "ApplicationManager.h"
#include "ServiceDirectory.h"
#include "ServiceDirectoryStreamItem.h"

using namespace rfa::rdm;
using namespace rfa::message;
using namespace rfa::common;
using namespace rfa::config;
using namespace rfa::data;
using namespace rfa::sessionLayer;


using namespace rfa::rdm;

DirectoryManager::DirectoryManager(ApplicationManager &mAppManager, ServiceDirectory& serviceDirectory)
: DomainManager(mAppManager, MMT_DIRECTORY),_srcDirGen(serviceDirectory)
{
    _pConfigTree = mAppManager.getAppConfigManager()->getTree("Services");
	assert( _pConfigTree );
	_srcDirGen.setDomainManager(*this);
}

DirectoryManager::~DirectoryManager()
{
	for ( SRC_DIR_SI_LIST::iterator it = _streams.begin(); 
		  it != _streams.end(); it = _streams.begin()  )
	{
		StreamItem *pStreamItem = *it;
		_srcDirGen.removeStreamItem(*pStreamItem);
		_streams.erase( it );
	}
	_streams.clear();
}

void DirectoryManager::processReReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
    UInt8 interactionType = reqMsg.getInteractionType();
    if (!(interactionType & ReqMsg::InitialImageFlag ) && (interactionType & ReqMsg::InterestAfterRefreshFlag))
        return;

	// for existig request, send a refresh again
	if (streamItem.satisfies(reqMsg.getAttribInfo()))
	{
		sendRefresh(token, reqMsg, static_cast<ServiceDirectoryStreamItem&>(streamItem));
	}
	else // request parameters changed
	{
			// close the old stream from the perspective of the sessionManager
		streamItem.removeRequestToken(token);
		sessionManager.removeStreamItem(token);
		// if the streamItem is not needed, destroy it

		// search for/create new stream item - same as processReqMsg()
		ServiceDirectoryStreamItem *pSrcDirStreamItem = getStreamItem(reqMsg.getAttribInfo());
		pSrcDirStreamItem->addRequestToken( token );

		sessionManager.addStreamItem( token, *pSrcDirStreamItem );
		sendRefresh(token, reqMsg, *pSrcDirStreamItem);
	
		// cleanup old stream - same as processCloseReqMsg()
		if ( streamItem.getNumOfRequestTokens() == 0 )
			cleanupStreamItem(static_cast<ServiceDirectoryStreamItem*>(&streamItem));
	}
}


void DirectoryManager::processCloseReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token, 
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
	if ( streamItem.getNumOfRequestTokens() == 0 )
		cleanupStreamItem(static_cast<ServiceDirectoryStreamItem*>(&streamItem));
}

void DirectoryManager::processReqMsg(SessionManager & sessionManager, 
									 RequestToken & token,  
									 const ReqMsg & reqMsg)
{
	ServiceDirectoryStreamItem* pSrcDirStreamItem = 0;

	if ( isValidRequest(reqMsg.getAttribInfo()) )
	{
		if ( (reqMsg.getInteractionType() & ReqMsg::InitialImageFlag) && 
			(reqMsg.getInteractionType() & ReqMsg::InterestAfterRefreshFlag))
		{
			// only create StreamItem for stream requests
			pSrcDirStreamItem = getStreamItem(reqMsg.getAttribInfo());
			pSrcDirStreamItem->addRequestToken(token);
				
			// add to _clientSesionStreams in SessionManager, if request is valid
			sessionManager.addStreamItem( token, *pSrcDirStreamItem );

			sendRefresh(token,reqMsg,*pSrcDirStreamItem);
		}
	}
	else
		sendCloseStatus(token,reqMsg.getAttribInfo());
}

void DirectoryManager::sendRefresh( RequestToken & token, const ReqMsg & reqMsg, ServiceDirectoryStreamItem & srcDirStreamItem )
{
	RespMsg respMsg;
	RespStatus respStatus;
	// set MsgModelType
	respMsg.setMsgModelType(rfa::rdm::MMT_DIRECTORY);		
	// Set attribute 
	respMsg.setAttribInfo(reqMsg.getAttribInfo());
	// Set RespTypeNum
	respMsg.setRespTypeNum(rfa::rdm::REFRESH_SOLICITED);
	respMsg.setIndicationMask(RespMsg::RefreshCompleteFlag);
	respStatus.setStreamState(RespStatus::OpenEnum);
	respStatus.setDataState(RespStatus::OkEnum);	
	respStatus.setStatusCode(RespStatus::NoneEnum);
	RFA_String  tmpStr("RequestCompleted");
	respStatus.setStatusText(tmpStr.c_str());

	// Set response Type
	rfa::message::RespMsg::RespType  respType= RespMsg::RefreshEnum;
	respMsg.setRespType(respType);
	_srcDirGen.reusableMap().clear(); 
	srcDirStreamItem.createData(static_cast<Data&>(_srcDirGen.reusableMap()), respType);
	respMsg.setPayload(static_cast<Data&>(_srcDirGen.reusableMap()));
		
	// Set respStatus 
	respMsg.setRespStatus(respStatus);

	// Create a OMMSolicitedItemCmd instance
	OMMSolicitedItemCmd itemCmd;
	itemCmd.setMsg(static_cast<rfa::common::Msg&>(respMsg));
    itemCmd.setRequestToken(const_cast<RequestToken&>(token));     
	appManager.getOMMProvider()->submit(&itemCmd);

}

bool  DirectoryManager::isValidRequest(const rfa::message::AttribInfo& attrib)
{
	bool bIsValidRequest = true;
	if ( attrib.getHintMask() & rfa::message::AttribInfo::ServiceNameFlag )
	{
		if( attrib.getServiceName() == "")
			return bIsValidRequest;

		if (!_pConfigTree->getChildAsTree(attrib.getServiceName()) )
			bIsValidRequest = false;
	}
	return bIsValidRequest;
}

void DirectoryManager::cleanupStreamItem(ServiceDirectoryStreamItem *  pSrcDirSI)
{
	ServiceDirectoryStreamItem* pStreamItem = 0;
	SRC_DIR_SI_LIST::iterator it;
	for ( it = _streams.begin(); it != _streams.end(); it++ ) 
	{
		pStreamItem = *it;
		if ( pStreamItem == pSrcDirSI ) break;
	}

	if ( it == _streams.end() )
	{
		return;
	}

	_streams.erase( it );
	_srcDirGen.removeStreamItem( *pSrcDirSI );
}

ServiceDirectoryStreamItem* DirectoryManager::getStreamItem(const AttribInfo& attrib)
{
	ServiceDirectoryStreamItem* pSrcDirSI = 0;
	// find the StreamItem with the same AttribInfo first, if not exist, then create a new one
	for (SRC_DIR_SI_LIST::iterator it = _streams.begin();	it != _streams.end(); it++ ) 
	{
		pSrcDirSI = *it;
		if (pSrcDirSI->satisfies(attrib))
			return pSrcDirSI;
	}

	pSrcDirSI = static_cast<ServiceDirectoryStreamItem*>(_srcDirGen.createStreamItem(attrib));
	_streams.push_back( pSrcDirSI );

	return pSrcDirSI;
}

