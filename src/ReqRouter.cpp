#include "ReqRouter.h"

#include "DomainManager.h"
#include "SessionManager.h"
#include "ApplicationManager.h"

#include "RDM/RDM.h"
#include "Common/RespStatus.h"
#include "Message/ReqMsg.h"
#include "Message/RespMsg.h"
#include "SessionLayer/OMMSolicitedItemCmd.h"
#include "SessionLayer/OMMProvider.h"

#include <iostream>

using namespace rfa::sessionLayer;
using namespace rfa::message;
using namespace rfa::common;
using namespace rfa::rdm;

ReqRouter::ReqRouter() : _domainMgrs(MMT_MAX_VALUE)
{
}

void ReqRouter::addDomainMgr(DomainManager & mgr)
{
	_domainMgrs[mgr.messageModelType()] = &mgr;
}

void ReqRouter::processReqMsg(SessionManager & sessionManager, RequestToken & token, const ReqMsg & msg)
{
	DomainManager * domainManager = _domainMgrs[msg.getMsgModelType()];
	if (domainManager == 0)
	{
		OMMProvider * provider = sessionManager.getApplicationManager().getOMMProvider();
		sendCloseStatus(*provider, token, msg.getMsgModelType(), msg.getAttribInfo());
	}
	else
		domainManager->processReqMsg(sessionManager, token, msg);
}

void ReqRouter::processReReqMsg(SessionManager & sessionManager, RequestToken & token, StreamItem & streamItem, const ReqMsg & msg)
{
	DomainManager * domainManager = _domainMgrs[msg.getMsgModelType()];
	if (domainManager)
		domainManager->processReReqMsg(sessionManager, token, streamItem, msg);
	// else ignore
}

void ReqRouter::processCloseReqMsg(SessionManager & sessionManager, RequestToken & token, StreamItem & streamItem, const ReqMsg & msg)
{
	DomainManager * domainManager = _domainMgrs[msg.getMsgModelType()];
	if (domainManager)
		domainManager->processCloseReqMsg(sessionManager, token, streamItem, msg);
	// else ignore
}

void ReqRouter::sendCloseStatus(OMMProvider & provider, 
									RequestToken & requestToken,
									UInt8 messageModelType,
									const AttribInfo & attribInfo)
{
	OMMSolicitedItemCmd cmd;
	RespMsg msg;
	RespStatus respStatus;

	// Create Status 
	RespStatus rStatus;
	rStatus.setStreamState(RespStatus::ClosedEnum);
	rStatus.setDataState(RespStatus::OkEnum);	
	rStatus.setStatusCode(RespStatus::NotFoundEnum);
	rStatus.setStatusText("Unsupported message model type");
	msg.setRespStatus(rStatus);

	msg.setMsgModelType(messageModelType);
	msg.setRespType(RespMsg::StatusEnum);
	cmd.setMsg(msg);
	cmd.setRequestToken(requestToken);
	provider.submit(&cmd);
}
