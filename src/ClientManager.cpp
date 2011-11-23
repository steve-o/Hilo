#include "ClientManager.h"

#include "vpf/vpf.h"


#include <assert.h>
#include <iostream>

#include "SessionManager.h"
#include "ReqRouter.h"
#include "ApplicationManager.h"
#include "ConfigManager.h"

#include "SessionLayer/ConnectionEvent.h"
#include "SessionLayer/Session.h"
#include "SessionLayer/OMMProvider.h"
#include "SessionLayer/OMMListenerConnectionIntSpec.h"
#include "SessionLayer/OMMClientSessionListenerIntSpec.h"
#include "SessionLayer/OMMErrorIntSpec.h"
#include "SessionLayer/OMMActiveClientSessionEvent.h"
#include "SessionLayer/OMMCmdErrorEvent.h"
#include "SessionLayer/ClientSessionStatus.h"
#include "SessionLayer/OMMClientSessionCmd.h"
#include "Common/CommonEnums.h"
//#include "SessionLayer/OMMClientSessionIntSpec.h"


using namespace std;
using namespace rfa::common;
using namespace rfa::sessionLayer;


ClientManager::ClientManager(ApplicationManager &mApplicationManager)
: appManager(mApplicationManager)
{
	init();
}

ClientManager::~ClientManager()
{
	cleanup();
}

void ClientManager::readConfig()
{
	_wantsCompletionEvents = appManager.getAppConfigManager()->getBool("wantsCompletionEvents", false);
	_maxConnections = (UInt16) appManager.getAppConfigManager()->getLong("maxConnections", 10);
}

void ClientManager::init()
{
 	readConfig();

 	OMMProvider * ommProvider  = appManager.getSession()->createOMMProvider("OMMProvider", _wantsCompletionEvents);
	assert( ommProvider  );
	appManager.setOMMProvider(ommProvider);
	

	// Register for Listener Connection events (these are event related to the clientManager listen port)
	OMMListenerConnectionIntSpec intConnSpec;
	_listenConnectionHandle = ommProvider->registerClient(appManager.getEventQueue(), &intConnSpec, *this);
	assert(_listenConnectionHandle);

	// Register to receive new client session connection event (inbound clients)
	OMMClientSessionListenerIntSpec intListenerSpec;
	_listenHandle = ommProvider->registerClient(appManager.getEventQueue(), &intListenerSpec, *this);
	assert( _listenHandle );

	// Register for Cmd Error events (These events are sent back if the submit() call fails)
	OMMErrorIntSpec intErrSpec;
	_errHandle = ommProvider->registerClient(appManager.getEventQueue(), &intErrSpec, *this);
}

void ClientManager::cleanup()
{

	OMMProvider * pOmmProvider = appManager.getOMMProvider();
	// if case pOmmProvider has been destroyed, all PrvClientManager member objects should already have been 
	// cleaned up because both ClientManager and OmmProvider are singleton in the app.
	if( !pOmmProvider ) return;

	// cleanup provider
	if( pOmmProvider )
	{
		if ( _listenHandle )
			pOmmProvider->unregisterClient(_listenHandle);
		if ( _listenConnectionHandle )
			pOmmProvider->unregisterClient(_listenConnectionHandle);
		if ( _errHandle )
			pOmmProvider->unregisterClient(_errHandle);

		
		// clean out the list of SessionManager
		for( CSM_LIST::iterator csm_iter = _clientSessions.begin();
			csm_iter != _clientSessions.end(); 
			csm_iter = _clientSessions.begin() )
		{
			SessionManager *pCSM = (*csm_iter);
			_clientSessions.erase(  csm_iter );
			delete pCSM;		
		}

		pOmmProvider->destroy();
		pOmmProvider = 0;
		appManager.setOMMProvider(0);
	}
}

void ClientManager::processEvent(const Event & event)
{
	switch( event.getType() )
	{
		case OMMActiveClientSessionEventEnum:
			processOMMActiveClientSessionEvent(static_cast<const OMMActiveClientSessionEvent&>(event));
			break;
		case OMMCmdErrorEventEnum:
			processOMMCmdErrorEvent(static_cast<const OMMCmdErrorEvent&>(event));
			break;
		case ConnectionEventEnum:
			processConnectionEvent( static_cast<const ConnectionEvent &>(event) );
			break;
		case ComplEventEnum:
			processCompletionEvent(event);
			break;
		default:
			MsgLog (eMsgHigh, 810122, "Unhandled event received by ClientManager" );
			assert(0);
			break;
	}
}

void ClientManager::processCompletionEvent(const Event& event)
{
     MsgLog (eMsgHigh, 810257, "Received Completion Event for ", event.getHandle());
}

void ClientManager::processConnectionEvent( const ConnectionEvent & event )
{
	// output status of the listening connections
	const RFA_String & connectionName = event.getConnectionName();
	const ConnectionStatus& status = event.getStatus();

	if ( status.getState() == ConnectionStatus::Up && (!_connectionUp) ) 
	{
         MsgLog (eMsgHigh, 810268, "Listen Connection %s - UP" , connectionName.c_str());
		_connectionUp = true;
	}
	else  if ( status.getState() == ConnectionStatus::Down && _connectionUp )
	{
         MsgLog (eMsgHigh, 810273, "Listen Connection %s - DOWN" , connectionName.c_str());
		_connectionUp = false;
	}
    else
    {
        MsgLog (eMsgHigh, 810278, "Listen Connection %s - No Change" , connectionName.c_str());
    }

    MsgLog (eMsgHigh, 810281, "Connection Status : %s", status.getStatusText().c_str());
}

void ClientManager::processOMMActiveClientSessionEvent( const OMMActiveClientSessionEvent &event )
{
	const rfa::common::Handle* clientSessionHandle = event.getClientSessionHandle();

	if ( shouldAcceptClientSession() )
	{
		SessionManager * csm = new SessionManager(*this, appManager);
		csm->accept(*clientSessionHandle);
		_clientSessions.push_back(csm);
	}
	else
	{
		// Reject the client session 
		OMMProvider * ommProvider = appManager.getOMMProvider();
		OMMClientSessionCmd mCliSessRejectCmd;
		mCliSessRejectCmd.setClientSessionHandle( clientSessionHandle );
		ClientSessionStatus mCliSessRejectStatus;
		mCliSessRejectStatus.setState(ClientSessionStatus::Inactive);
		mCliSessRejectStatus.setStatusCode(ClientSessionStatus::Reject);
		mCliSessRejectCmd.setStatus(mCliSessRejectStatus);
		ommProvider->submit(&mCliSessRejectCmd);	
        MsgLog (eMsgHigh, 810305, "Client Session %d has been rejected", clientSessionHandle);
	}
}


void	ClientManager::processOMMCmdErrorEvent( const OMMCmdErrorEvent &event )
{
    
   MsgLog (eMsgHigh, 810313, "OMMCmdErrorEvent %s ", event.getStatus().getStatusText().c_str());
//       event.getCmdID()
  //      ,event.getSubmitClosure(),event.getStatus().getState(),event.getStatus().getStatusCode());
}


bool ClientManager::shouldAcceptClientSession()
{
	return (appManager.getAppConfigManager()->getBool("AcceptClientSessions",true)) && (_clientSessions.size() < _maxConnections);
}

void ClientManager::removeSessionManager(SessionManager* pCSM)
{
	CSM_LIST::iterator csm_it;
	for ( csm_it = _clientSessions.begin(); 
		  csm_it != _clientSessions.end() && *csm_it != pCSM;
		  csm_it ++ );
	if ( csm_it == _clientSessions.end() ) assert( 0 );

	_clientSessions.erase( csm_it );

}
