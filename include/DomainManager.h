#ifndef _DOMAIN_MANAGER_H
#define _DOMAIN_MANAGER_H

#pragma warning( disable : 4512 )
#include "Common/Common.h"
#include "Common/RespStatus.h"

#include "ApplicationManager.h"

#include <map>

namespace rfa {
	namespace common {
		class RFA_String;
	}
	namespace sessionLayer {
		class RequestToken;
	}
	namespace message {
		class ReqMsg;
		class RespMsg;
		class AttribInfo;
	}
}

class StreamItem;
class SessionManager;

class DomainManager
{
public:
	DomainManager(ApplicationManager &appManager, rfa::common::UInt8 messageModelType);
	virtual ~DomainManager();

	inline rfa::common::UInt8 messageModelType() { return _messageModelType; }

	virtual void processReqMsg(SessionManager & SessionManager, 
		rfa::sessionLayer::RequestToken &, const rfa::message::ReqMsg & msg) = 0;
	virtual void processCloseReqMsg(SessionManager & SessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg) = 0;
	virtual void processReReqMsg(SessionManager & SessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg) = 0;
	virtual bool	isValidRequest(const rfa::message::AttribInfo& attrib);

protected:
	void sendCloseStatus(rfa::sessionLayer::RequestToken & requestToken,
		const rfa::message::AttribInfo & attribInfo);

	void sendCloseStatus(rfa::sessionLayer::RequestToken & requestToken,
		const rfa::message::AttribInfo & attribInfo, rfa::common::RespStatus::StatusCode code,
		const rfa::common::RFA_String & statusText);

	virtual void init();
	virtual void readConfig();
	virtual void cleanUp();

    ApplicationManager &appManager;
	rfa::common::UInt8 _messageModelType;
};

#endif

