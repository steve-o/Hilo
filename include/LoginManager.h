#ifndef _LOGIN_MANAGER_H
#define _LOGIN_MANAGER_H

#include "DomainManager.h"
#include "SessionLayer/OMMSolicitedItemCmd.h"

namespace rfa {
	namespace message {
		class AttribInfo;
	}

	namespace common {
		class RespStatus;
	}
}

class UserContext;
class Authorizer;

class LoginManager : public DomainManager
{
public:
	LoginManager(ApplicationManager &applicationManager, Authorizer & authorizer);
	virtual	~LoginManager();
	virtual void processReqMsg(SessionManager & SessionManager, 
		rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & reqMsg);
	virtual void processCloseReqMsg(SessionManager & SessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
	virtual void processReReqMsg(SessionManager & SessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
protected:

	void sendLoginMsg(UserContext & userContext, rfa::sessionLayer::RequestToken & token,
							   const rfa::message::AttribInfo & rAttribInfo);

	Authorizer& _rAuthorizer;
	typedef std::map<SessionManager*, StreamItem *> CSLSI_MAP;
	CSLSI_MAP  _clientSessionLoginTokens;

	rfa::sessionLayer::OMMSolicitedItemCmd		_loginCmd;

};

#endif
