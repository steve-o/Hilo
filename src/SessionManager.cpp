#include "SessionManager.h"
#include "vpf/vpf.h"

#include <assert.h>
#include <iostream>

#include "ClientManager.h"
#include "StreamItem.h"
#include "ReqRouter.h"
#include "ApplicationManager.h"

#include "Common/EventQueue.h"
#include "Common/Event.h"
#include "SessionLayer/RequestToken.h"
#include "SessionLayer/OMMProvider.h"
#include "SessionLayer/OMMClientSessionIntSpec.h"
#include "SessionLayer/OMMInactiveClientSessionEvent.h"
#include "SessionLayer/OMMSolicitedItemEvent.h"
#include "Message/ReqMsg.h"
#include "RDM/RDM.h"
#include "Common/CommonEnums.h"

using namespace std;
using namespace rfa::common;
using namespace rfa::sessionLayer;
using namespace rfa::rdm;
using namespace rfa::message;

SessionManager::SessionManager(ClientManager & clientManager, ApplicationManager &mApplicationManager)
: _clientManager(clientManager), appManager(mApplicationManager),_loggedIn(false), _clientSessionHandle(0),
_loginToken(0)
{

}

SessionManager::~SessionManager()
{
	appManager.getOMMProvider()->unregisterClient(_clientSessionHandle);
	_clientSessionHandle = 0;
	clearClientSessionStreams();
}

void SessionManager::accept(const Handle & clientSessionHandle)
{
	OMMClientSessionIntSpec cliSessInterestSpec;
	cliSessInterestSpec.setClientSessionHandle(&clientSessionHandle);

		// Accept the session 
	OMMProvider & ommProvider = *appManager.getOMMProvider();
	EventQueue * eventQueue = appManager.getEventQueue();
	_clientSessionHandle = ommProvider.registerClient(eventQueue, &cliSessInterestSpec, *this);
	assert( _clientSessionHandle == &clientSessionHandle); // The returned handle should be the exact same handle as the pCliSessHandle

	MsgLog(eMsgHigh, 810643, "Client Session with handle %d has been accepted", _clientSessionHandle);
}

void SessionManager::addStreamItem( RequestToken& token, StreamItem& streamItem )
{
	/*assert ( _clientSessionStreams.find(&token) == _clientSessionStreams.end() );
	_clientSessionStreams.insert(CSS_MAP::value_type(&token, &streamItem));*/
	CSS_MAP::iterator it = _clientSessionStreams.find(&token);
	if ( it == _clientSessionStreams.end() ) 
	{
		STREAMITEM_STRUCT* sItem = new STREAMITEM_STRUCT;
		sItem->streamItem = &streamItem;
		sItem->numOfOpens = 1;
		_clientSessionStreams.insert(CSS_MAP::value_type(&token, sItem));
	}
	else
		it->second->numOfOpens++;
}

void SessionManager::removeStreamItem( RequestToken& token )
{
	CSS_MAP::iterator it = _clientSessionStreams.find(&token);
	// ignore if not found
	if ( it != _clientSessionStreams.end() )
	{
		delete it->second;
        _clientSessionStreams.erase(&token);
	}
}

void SessionManager::processEvent(const rfa::common::Event & event)
{
	switch( event.getType() )
	{
		case OMMInactiveClientSessionEventEnum:
			processOMMInactiveClientSessionEvent(static_cast<const OMMInactiveClientSessionEvent&>(event));
			break;
		case OMMSolicitedItemEventEnum:
			processOMMSolicitedItemEvent(static_cast<const OMMSolicitedItemEvent&>(event));
			break;
		case ComplEventEnum:
			processCompletionEvent(event);
			break;
		default:
			MsgLog(eMsgHigh, 810742, "Unhandled event received by RDMClientManager");
			assert(0);
			break;
	}

}

void SessionManager::processCompletionEvent(const Event& event)
{
	MsgLog(eMsgHigh, 810751, "Received Completion Event for %s ", event.getHandle());
}

void SessionManager::processOMMSolicitedItemEvent( const OMMSolicitedItemEvent &event )
{
	const ReqMsg& reqMsg = static_cast<const ReqMsg&>(event.getMsg());
	RequestToken & token = event.getRequestToken();
	
	if (!_loggedIn)
	{
		if ( reqMsg.getMsgModelType() != MMT_LOGIN )
		{
			// TODO reject - not logged in yet
		}
		else
		{
			_loginToken = &token;
		}
	}

	UInt8 interactionType = reqMsg.getInteractionType();
    if (!(interactionType & ReqMsg::InitialImageFlag) && !(interactionType & ReqMsg::InterestAfterRefreshFlag))
		processClose(token, reqMsg);
	else
		processRequest(token, reqMsg);	
}

void SessionManager::processClose( RequestToken & token, const ReqMsg & reqMsg )
{
	if ( reqMsg.getMsgModelType() == MMT_LOGIN )
	{
		CSS_MAP::iterator css_it = _clientSessionStreams.find( &token );

		if ( css_it != _clientSessionStreams.end() )
		{
			StreamItem * streamItem = css_it->second->streamItem;
			clearClientSessionStreams();
			appManager.getReqRouter()->processCloseReqMsg(*this, token, *streamItem, reqMsg);
			MsgLog(eMsgHigh, 810789, "Processed Logout Request Message for Client Session Handle: %d ", _clientSessionHandle );
		}
		else
		{
			MsgLog(eMsgHigh, 810793, "Ignored Logout Request Message for Client Session Handle: %d ", _clientSessionHandle );
		}
		_loggedIn = false;
		_loginToken = 0;
		return;
	}

	CSS_MAP::iterator css_it = _clientSessionStreams.find( &token );
		// for none existing token, we ignore it
	if ( css_it != _clientSessionStreams.end() )
	{
		if ( css_it->second->numOfOpens == 1 )
		{
			MsgLog(eMsgHigh, 810806, "Received Close Request of %d for Client Session Handle", _clientSessionHandle);

			StreamItem * streamItem = css_it->second->streamItem;
			streamItem->removeRequestToken( token );
			delete css_it->second; 
			_clientSessionStreams.erase( css_it );
			appManager.getReqRouter()->processCloseReqMsg(*this, token, *streamItem, reqMsg);
		}
		else //still has opens for the same item.
			css_it->second->numOfOpens--;
	}
}

void SessionManager::processRequest(RequestToken & token, const ReqMsg & reqMsg)
{
	CSS_MAP::iterator css_it = _clientSessionStreams.find( &token );
	if ( css_it == _clientSessionStreams.end() )
		appManager.getReqRouter()->processReqMsg(*this, token, reqMsg);
	else
	{
		StreamItem * streamItem = css_it->second->streamItem;
		appManager.getReqRouter()->processReReqMsg(*this, token, *streamItem, reqMsg);
	}
}

void SessionManager::processOMMInactiveClientSessionEvent( const OMMInactiveClientSessionEvent &event )
{
	// Get client session handle
	rfa::common::Handle * clientSessionHandle = event.getHandle();
	assert( _clientSessionHandle == clientSessionHandle );

	MsgLog(eMsgHigh, 810837, "Client Session %d has been closed via OMMInactiveClientSessionEvent", _clientSessionHandle);

	if (_loginToken)  
	{
		ReqMsg reqMsg; // ReqMsg is ignored
		reqMsg.setMsgModelType(MMT_LOGIN);
		_loggedIn = false;
		CSS_MAP::iterator css_it = _clientSessionStreams.find( _loginToken );
		if ( css_it != _clientSessionStreams.end() )
		{
			StreamItem * streamItem = css_it->second->streamItem;
			appManager.getReqRouter()->processCloseReqMsg(*this, *_loginToken, *streamItem, reqMsg);
		}
		_loginToken = 0;
	}

	// remove itself from ClientManager, 
	_clientManager.removeSessionManager(this);

	delete this;
}

void SessionManager::clearClientSessionStreams()
{
	// Cleanup _clientSessionStreams 
	for ( CSS_MAP::iterator css_it = _clientSessionStreams.begin();
			css_it != _clientSessionStreams.end(); 
			css_it ++ )
	{
		StreamItem* pStreamItem = (*css_it).second->streamItem;
		pStreamItem->removeRequestToken(*((*css_it).first));
		delete (*css_it).second;
	}
	_clientSessionStreams.clear();
}

void SessionManager::forceLogout()
{
	_loggedIn = false;
	_loginToken = 0;
	clearClientSessionStreams();
}
