#include "DomainManager.h"

#include <assert.h>

#include "Message/RespMsg.h"
#include "Message/AttribInfo.h"
#include "SessionLayer/OMMSolicitedItemCmd.h"
#include "SessionLayer/OMMProvider.h"
#include "StreamItem.h"

#include <iostream>
//del #include "RDMUtil.h"

using namespace std;
using namespace rfa::common;
using namespace rfa::message;
using namespace rfa::sessionLayer;

DomainManager::DomainManager(ApplicationManager &mAppManager, UInt8 messageModelType)
: appManager(mAppManager), _messageModelType(messageModelType)
{
	
}

DomainManager::~DomainManager()
{

}

void DomainManager::sendCloseStatus(rfa::sessionLayer::RequestToken & requestToken,
					const AttribInfo & attribInfo)
{
	sendCloseStatus(requestToken, attribInfo, RespStatus::NoneEnum, "");
}

void DomainManager::sendCloseStatus(rfa::sessionLayer::RequestToken & requestToken,
	const AttribInfo & attribInfo, RespStatus::StatusCode code, const RFA_String & statusText)
{
	OMMSolicitedItemCmd cmd;
	RespMsg msg;
	RespStatus respStatus;

	// Create Status 
	RespStatus rStatus;
	rStatus.setStreamState(RespStatus::ClosedEnum);
	rStatus.setDataState(RespStatus::OkEnum);	
	rStatus.setStatusCode( (UInt8)code );
	rStatus.setStatusText(statusText);
	msg.setRespStatus(rStatus);
	msg.setAttribInfo(attribInfo);
	msg.setMsgModelType(_messageModelType);
	msg.setRespType(RespMsg::StatusEnum);
	cmd.setMsg(msg);
	cmd.setRequestToken(requestToken);
	OMMProvider * provider = appManager.getOMMProvider();
	provider->submit(&cmd);
}

void DomainManager::init()
{
}

void DomainManager::readConfig()
{
}

void DomainManager::cleanUp()
{
}

bool DomainManager::isValidRequest(const rfa::message::AttribInfo& attrib)
{
	return true;
}

