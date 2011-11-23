#ifndef _REQROUTER_H
#define _REQROUTER_H

#include <vector>
#include "Common/Common.h"

namespace rfa {
	namespace sessionLayer {
		class RequestToken;
		class OMMProvider;
	}
	namespace message {
		class ReqMsg;
		class AttribInfo;
	}
}

class DomainManager;
class SessionManager;
class StreamItem;

class ReqRouter
{
public:
	ReqRouter();
	virtual ~ReqRouter() {};

	void addDomainMgr(DomainManager &);
	void processReqMsg(SessionManager & sessionManager, rfa::sessionLayer::RequestToken &, const rfa::message::ReqMsg & msg);
	void processCloseReqMsg(SessionManager & sessionManager, rfa::sessionLayer::RequestToken &, StreamItem & streamItem, const rfa::message::ReqMsg & msg);
	void processReReqMsg(SessionManager & sessionManager, rfa::sessionLayer::RequestToken &, StreamItem & streamItem, const rfa::message::ReqMsg & msg);

protected:
	void sendCloseStatus(rfa::sessionLayer::OMMProvider & provider, 
		rfa::sessionLayer::RequestToken & requestToken, rfa::common::UInt8 messageModelType,
		const rfa::message::AttribInfo & attribInfo);

	std::vector<DomainManager *> _domainMgrs;
};

#endif

