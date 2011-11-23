#include "LoginManager.h"
#include "vpf/vpf.h"

#include <assert.h>

#include "RDM/RDM.h"
#include "Common/Msg.h"
#include "Data/ElementList.h"
#include "Message/RespMsg.h"
#include "Message/ReqMsg.h"
#include "Message/Manifest.h"
#include "Message/AttribInfo.h"
#include "SessionLayer/RequestToken.h"
#include "SessionLayer/OMMProvider.h"
#include "SessionManager.h"
#include "ApplicationManager.h"
#include "Authorizer.h"
#include "UserContext.h"

#include <iostream>
#include "Data/ElementList.h"
#include "Data/ElementListReadIterator.h"

using namespace rfa::rdm;
using namespace rfa::data;
using namespace rfa::message;
using namespace rfa::common;
using namespace rfa::sessionLayer;


LoginManager::LoginManager(ApplicationManager &applicationManager, Authorizer & authorizer)
: DomainManager(applicationManager, MMT_LOGIN), _rAuthorizer(authorizer), _loginCmd()
{
	_rAuthorizer.setDomainManager(*this);
}

LoginManager::~LoginManager()
{
}

void LoginManager::processReReqMsg(SessionManager & SessionManager, 
									  rfa::sessionLayer::RequestToken & token,  
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
	CSLSI_MAP::iterator it = _clientSessionLoginTokens.find(&SessionManager);
	UserContext * userContext = static_cast<UserContext*>(it->second);
	if (userContext->satisfies(reqMsg.getAttribInfo()))
	{
		sendLoginMsg(*userContext, token, reqMsg.getAttribInfo());
	}
	else
	{
		// TODO cleanup
		_clientSessionLoginTokens.erase( it );
		SessionManager.forceLogout();
		sendCloseStatus(token, reqMsg.getAttribInfo(), RespStatus::AlreadyOpenEnum, "Already Logged In");

	}
}

void LoginManager::processCloseReqMsg(SessionManager & SessionManager, 
									  rfa::sessionLayer::RequestToken & token, 
									  StreamItem & streamItem,
									  const rfa::message::ReqMsg & reqMsg)
{
	CSLSI_MAP::iterator it = _clientSessionLoginTokens.find(&SessionManager);
	assert( it != _clientSessionLoginTokens.end() );
	_clientSessionLoginTokens.erase( it );
}

void LoginManager::processReqMsg(SessionManager& SessionManager, 
								 RequestToken& token, 
								 const ReqMsg & reqMsg)
{
	if (_clientSessionLoginTokens.find(&SessionManager) != _clientSessionLoginTokens.end())
	{
		// already logged in - this case should be prevented by RFA
		sendCloseStatus(token, reqMsg.getAttribInfo(), RespStatus::AlreadyOpenEnum, "Already Logged In");
		SessionManager.forceLogout();
		return;
	}

	StreamItem * userContext = _rAuthorizer.createStreamItem(reqMsg.getAttribInfo());
	if ( !userContext )
	{
		// Authorizer did not allow login
		sendCloseStatus(token, reqMsg.getAttribInfo(),
			RespStatus::NotAuthorizedEnum, "Not authorized");
		SessionManager.forceLogout();
		return;
	}
	userContext->addRequestToken(token);

	SessionManager.setLoggedIn();

	// Keep a reference to the SessionManager
	_clientSessionLoginTokens.insert(CSLSI_MAP::value_type(&SessionManager, userContext));
	sendLoginMsg(static_cast<UserContext&>(*userContext), token, reqMsg.getAttribInfo());

	// add to _clientSesionStreams in SessionManager, if request is valid
	SessionManager.addStreamItem( token, *userContext );
}

void LoginManager::sendLoginMsg(UserContext & userContext, RequestToken & token, const AttribInfo & rAttribInfo)
{
	RespMsg respMsg;
	AttribInfo attribInfo;
	respMsg.setMsgModelType(MMT_LOGIN);	
	RespStatus rStatus;
	rStatus.setDataState(RespStatus::OkEnum);
	rStatus.setStreamState(RespStatus::OpenEnum);
	rStatus.setStatusCode(RespStatus::NoneEnum);
	RFA_String  tmpStr;
	tmpStr.set( "RequestCompleted", 16, false );
	rStatus.setStatusText( tmpStr );
	respMsg.setRespStatus(rStatus);
	respMsg.setIndicationMask(RespMsg::RefreshCompleteFlag);

	respMsg.setRespType(RespMsg::RefreshEnum);
	respMsg.setRespTypeNum(REFRESH_SOLICITED);

		// TODO check attrib
	attribInfo.setName(rAttribInfo.getName());
	attribInfo.setNameType(rAttribInfo.getNameType());
	ElementList attrib;
	userContext.createData(attrib, RespMsg::RefreshEnum);
	attribInfo.setAttrib(attrib);
	respMsg.setAttribInfo(attribInfo);

    MsgLog (eMsgHigh, 810444, "Provided Login Accept Msg for %s: ", rAttribInfo.getName().c_str());

	if (rAttribInfo.getHintMask() & AttribInfo::AttribFlag)
	{
		assert(rAttribInfo.getAttrib().getDataType() == ElementListEnum);

		ElementListReadIterator readIter;
		readIter.start(static_cast<const ElementList &>(rAttribInfo.getAttrib()));
		while (!readIter.off())
		{
			const ElementEntry & entry = readIter.value();
			if (entry.getName() == "ApplicationId" || entry.getName() == "Position" )
                MsgLog (eMsgHigh, 810588, " %s : %s ", entry.getName().c_str() 
                , static_cast<const DataBuffer &>(entry.getData()).getAsString().c_str()) ;
			readIter.forth();
		}
	}

	_loginCmd.setMsg( static_cast<rfa::common::Msg&>(respMsg) );

	_loginCmd.setRequestToken( const_cast<RequestToken&>( token ) );  //  This request token should be held onto for future possible logouts
	appManager.getOMMProvider()->submit( &_loginCmd );
}
