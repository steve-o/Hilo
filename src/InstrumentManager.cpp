#include "InstrumentManager.h"

#include <assert.h>
#include <iostream>

#include "SessionLayer/RequestToken.h"
#include "Message/ReqMsg.h"
#include "Message/RespMsg.h"
#include "Common/RespStatus.h"
#include "Config/ConfigTree.h"
#include "Config/ConfigStringList.h"
#include "SessionLayer/OMMProvider.h"
#include "RDM/RDM.h"
#include "StreamItem.h"
#include "InstrumentStreamItem.h"
#include "DataGenerator.h"
#include "SessionManager.h"
#include "ApplicationManager.h"
#include "ConfigManager.h"

using namespace std;
using namespace rfa::common;
using namespace rfa::config;
using namespace rfa::message;
using namespace rfa::sessionLayer;
using namespace rfa::rdm;


InstrumentManager::InstrumentManager(ApplicationManager &appManager,
									 UInt8 messageModelType, DataGenerator& dataGen)
: DomainManager(appManager, messageModelType),_dataGen(dataGen),_isNewRequest(true),_itemCmd()
{
	_dataGen.setDomainManager(*this);
}

InstrumentManager::~InstrumentManager()
{
	// clean up _srcInstrumentSIs, it should cover the case when the provider application
	// shuts down abruptly.
	for ( SERVICE_SI_LIST::iterator it = _srcInstrumentSIs.begin(); 
		  it != _srcInstrumentSIs.end();
		  it = _srcInstrumentSIs.begin() )
	{
		INSTRUMENT_SI_LIST* pInstrumentSI = it->second;
		for ( INSTRUMENT_SI_LIST::iterator instrumentIt = pInstrumentSI->begin();
			  instrumentIt != pInstrumentSI->end();
			  instrumentIt = pInstrumentSI->begin() )
		{
			StreamItem* pStreamItem = static_cast<StreamItem*>(*instrumentIt);
			_dataGen.removeStreamItem(*pStreamItem);
			pInstrumentSI->erase( instrumentIt );
		}
		delete pInstrumentSI;
		_srcInstrumentSIs.erase( it );
	}
}

void InstrumentManager::processReReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
		processReqMsg(sessionManager, token, reqMsg);
}


void InstrumentManager::processCloseReqMsg(SessionManager & sessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
	//The token is already removed from the StreamItem when SessionManager calls processCloseReqMsg();

	const RFA_String& serviceName = (static_cast<InstrumentStreamItem&>(streamItem)).getServiceName();
	// if the streamItem is the only one for the Service, then remove it from _srcInstrumentSIs
    SERVICE_SI_LIST::iterator srcIt = _srcInstrumentSIs.find(serviceName.c_str());
	assert( srcIt != _srcInstrumentSIs.end() );

	if (srcIt == _srcInstrumentSIs.end())
		return;

	INSTRUMENT_SI_LIST *pInstrumentsSI = srcIt->second;

	INSTRUMENT_SI_LIST::iterator instrumentIt;
	for  ( instrumentIt = pInstrumentsSI->begin();
		   instrumentIt != pInstrumentsSI->end();
		   instrumentIt ++ ) 
	{
		StreamItem* pStreamItem = static_cast<StreamItem*>(*instrumentIt);
		if ( pStreamItem == &streamItem )
			break;
	}
	if ( instrumentIt == pInstrumentsSI->end() )
	{
		MsgLog(eMsgHigh, 810312, "Fail to find the original request in InstrumentManager" );
		return;
	}

	if ( streamItem.getNumOfRequestTokens() == 0 )
	{
		pInstrumentsSI->erase( instrumentIt );
		_dataGen.removeStreamItem( streamItem );
		//_srcInstrumentSIs.erase( srcIt );

		if (  pInstrumentsSI->size() == 0 )
		{
			delete pInstrumentsSI;
			_srcInstrumentSIs.erase( srcIt );
		}
	}
}

void InstrumentManager::processReqMsg(SessionManager & sessionManager, 
									  RequestToken & token,  const ReqMsg & reqMsg)
{
	assert( reqMsg.getHintMask() & ReqMsg::AttribInfoFlag );
	const AttribInfo& attribInfo = reqMsg.getAttribInfo();
	if (( !isValidRequest(attribInfo) ) || !(_dataGen.isValidRequest(attribInfo)))  // Check if valid for the InstrumentManager & the DataGenerator
	{
		// close the item
		sendCloseStatus(token,reqMsg.getAttribInfo());
		return;
	}

	bool isSnapShot = false;
	StreamItem* pStreamItem = 0;
	RespMsg respMsg;
	RespStatus respStatus;
	const RFA_String& serviceName = reqMsg.getAttribInfo().getServiceName();
	respMsg.setAttribInfo(reqMsg.getAttribInfo());

    // only create StreamItem for stream request
    if ( reqMsg.getInteractionType() & ReqMsg::InterestAfterRefreshFlag ) 
    {
        // if it is the first request for the service, create a Service + StreamItems entry in _srcInstrumentSIs
        SERVICE_SI_LIST::iterator it = _srcInstrumentSIs.find(serviceName.c_str());
        if (it == _srcInstrumentSIs.end() )    // Do only if it is a new service request 
        {
            INSTRUMENT_SI_LIST *pInstrumentsSI = new INSTRUMENT_SI_LIST;
            _srcInstrumentSIs.insert(SERVICE_SI_LIST::value_type(serviceName.c_str(),pInstrumentsSI));
        }

        // QoS evaluation can be done here.
		// rfa::common::QualityOfServiceRequest reqQoS = reqMsg.getRequestedQualityOfService();

		respStatus.setStreamState(rfa::common::RespStatus::OpenEnum);
		// get new or old item from generator
		pStreamItem = getStreamItem(reqMsg.getAttribInfo());
		if (!pStreamItem)
		{
			sendCloseStatus(token, reqMsg.getAttribInfo());
			return;
		}

		if ( !sessionManager.isTokenExist(token) )
			pStreamItem->addRequestToken(token);
			
		// add to _clientSesionStreams in SessionManager, if request is valid
		sessionManager.addStreamItem( token, *pStreamItem );
	}
	else
	{
		isSnapShot = true;
		respStatus.setStreamState(rfa::common::RespStatus::NonStreamingEnum);
		// for snapshot request, create the StreamItem but do not record it
		pStreamItem = getSnapshotItem( reqMsg.getAttribInfo() );
		if (!pStreamItem)
		{
			sendCloseStatus(token, reqMsg.getAttribInfo());
			return;
		}
	}

    // Create a OMMSolicitedItemCmd instance
	while ( !(respMsg.getIndicationMask() & RespMsg::RefreshCompleteFlag) )
	{
		pStreamItem->generateRespMsg( RespMsg::RefreshEnum, &respMsg, &respStatus );

		_itemCmd.setMsg( static_cast<rfa::common::Msg&>(respMsg) );
		_itemCmd.setRequestToken( const_cast<RequestToken&>(token) );  


		RFA_String refreshTypeString;

		if ( !(respMsg.getIndicationMask() & RespMsg::RefreshCompleteFlag) ) {
			refreshTypeString.set( "(part of multipart)", 19, false );
		} else {
			refreshTypeString.set( "Complete", 8, false );
		}

		appManager.getOMMProvider()->submit( &_itemCmd );
	}
    
	if ( isSnapShot && _isNewRequest )
		_dataGen.removeStreamItem( *pStreamItem );
	_isNewRequest = true;
}


bool InstrumentManager::isValidRequest(const AttribInfo& attrib)
{
	// validate if the correct request parameters exist
	if (attrib.getHintMask() != (AttribInfo::ServiceNameFlag|AttribInfo::NameFlag|AttribInfo::NameTypeFlag))
		return false;

	// then validate if the request exceeds the max # of requests of the Service
	
	const RFA_String& serviceName = attrib.getServiceName();
	const RFA_String& itemName = attrib.getName();
	
	const ConfigTree* pConfigTree = appManager.getAppConfigManager()->getTree( RFA_String( "Services", 0, false ) );
	assert( pConfigTree );
	pConfigTree = pConfigTree->getChildAsTree(serviceName);
	if ( !pConfigTree )
		return false;
	
	// check if item already exists
    SERVICE_SI_LIST::iterator it = _srcInstrumentSIs.find(serviceName.c_str());
	if ( it == _srcInstrumentSIs.end() )
		return true;

	//	// check if max number of requests will be exceeded for this service
	//unsigned int maxNumRequests = pConfigTree->getChildAsLong( RFA_String( "MaxNumRequests", 0, false ) );
	//INSTRUMENT_SI_LIST *pInstrumentsSI = it->second;
	//if ( pInstrumentsSI->size() >= maxNumRequests )
	//	return false;
	
	return true;
}

InstrumentStreamItem* InstrumentManager::getStreamItem(const AttribInfo& attrib)
{
	InstrumentStreamItem* pSI = isNewRequest(attrib);

	if (!pSI)
    {
        SERVICE_SI_LIST::iterator it = _srcInstrumentSIs.find(attrib.getServiceName().c_str());
		if ( it != _srcInstrumentSIs.end() )
		{
         INSTRUMENT_SI_LIST *pInstrumentSIs = it->second;
         pSI = static_cast<InstrumentStreamItem*>(_dataGen.createStreamItem(attrib));
         if (pSI)
             pInstrumentSIs->push_back( pSI );
		}
    }

	return pSI;
}

InstrumentStreamItem* InstrumentManager::getSnapshotItem(const AttribInfo& attrib)
{
	InstrumentStreamItem* pSI = isNewRequest(attrib);

	if (!pSI)
		pSI = static_cast<InstrumentStreamItem*>(_dataGen.createStreamItem(attrib));

	return pSI;
}


InstrumentStreamItem* InstrumentManager::isNewRequest(const AttribInfo&  attrib)
{
	InstrumentStreamItem* pSI = 0;

    SERVICE_SI_LIST::iterator it = _srcInstrumentSIs.find(attrib.getServiceName().c_str());
	if ( it == _srcInstrumentSIs.end() ) 
	{
		_isNewRequest = true;
		return pSI;
	}

	INSTRUMENT_SI_LIST *pInstrumentSIs = it->second;
    
	// find the StreamItem with the same AttribInfo first, if not exist, then create a new one
	for (INSTRUMENT_SI_LIST::iterator it = pInstrumentSIs->begin();
		it != pInstrumentSIs->end(); it++ ) 
	{
		pSI = *it;
		if (pSI->satisfies(attrib))
		{
			_isNewRequest = false;
			return pSI;
		}
	}

	_isNewRequest = true;
	return 0;
}
