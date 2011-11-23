#ifndef _CLIENT_MANAGER_H
#define _CLIENT_MANAGER_H

#include "Common/Client.h"
#include "Common/Common.h"

#include <list>

class SessionManager;
class ApplicationManager;

namespace rfa {
	namespace common {
		class Handle;
	}
	namespace sessionLayer {
		class OMMActiveClientSessionEvent;
		class OMMCmdErrorEvent;
		class ConnectionEvent;
	}
}

class ClientManager : public rfa::common::Client
{
public:
	ClientManager(ApplicationManager &mApplicationManager);
	virtual ~ClientManager();

	void cleanup();
	void removeSessionManager(SessionManager* pCSM);

protected:
	void init();

	void processEvent(const rfa::common::Event &event);
	void processCompletionEvent(const rfa::common::Event& event);
	void processOMMActiveClientSessionEvent(const rfa::sessionLayer::OMMActiveClientSessionEvent &);
	void processOMMCmdErrorEvent(const rfa::sessionLayer::OMMCmdErrorEvent&);
	void processConnectionEvent(const rfa::sessionLayer::ConnectionEvent & event);

	bool shouldAcceptClientSession();

	void readConfig();

	ApplicationManager &appManager;

	rfa::common::Handle * _errHandle;
	rfa::common::Handle * _listenConnectionHandle;
	rfa::common::Handle * _listenHandle;

	bool _connectionUp;

	typedef std::list<SessionManager*> CSM_LIST;
	CSM_LIST							_clientSessions;

	bool _wantsCompletionEvents;
	rfa::common::UInt16 _maxConnections;

private:
	ClientManager& operator=( const ClientManager& );

};


#endif
