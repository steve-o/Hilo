#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include <map>

#include "Common/Client.h"

namespace rfa {
	namespace message {
		class ReqMsg;
	}
	namespace common {
		class Handle;
		class Event;
	}
	namespace sessionLayer {
		class OMMInactiveClientSessionEvent;
		class OMMSolicitedItemEvent;
		class RequestToken;
	}
}

class ClientManager;
class StreamItem;
class ApplicationManager;

class SessionManager : public rfa::common::Client
{
public:
	SessionManager(ClientManager & clientManager, ApplicationManager & ApplicationManager);
	virtual ~SessionManager();
	
	void accept(const rfa::common::Handle & clientSessionHandle);
	void addStreamItem( rfa::sessionLayer::RequestToken& token, StreamItem& streamItem );
	void removeStreamItem( rfa::sessionLayer::RequestToken& token );
	inline bool isTokenExist( rfa::sessionLayer::RequestToken& token )
	{return _clientSessionStreams.find(&token) != _clientSessionStreams.end();}

	inline ApplicationManager & getApplicationManager() { return appManager; }

	void forceLogout();
	inline void setLoggedIn() { _loggedIn = true; }

protected:
	void processEvent( const rfa::common::Event & event );
	void processCompletionEvent( const rfa::common::Event& event );
	void processOMMSolicitedItemEvent( const rfa::sessionLayer::OMMSolicitedItemEvent &event );
	void processOMMInactiveClientSessionEvent( const rfa::sessionLayer::OMMInactiveClientSessionEvent &event );
	void processRequest( rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & msg );
	void processClose( rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & msg );

	void clearClientSessionStreams();

	ClientManager & _clientManager;
	ApplicationManager &appManager;

	rfa::common::Handle * _clientSessionHandle;

	struct STREAMITEM_STRUCT 
	{
		rfa::common::UInt8		numOfOpens; //add this for handling multiple opens for the same item in the same client session.
		StreamItem*				streamItem;
	};

	typedef std::map<rfa::sessionLayer::RequestToken *, STREAMITEM_STRUCT*> CSS_MAP;
	CSS_MAP											_clientSessionStreams;
	rfa::sessionLayer::RequestToken *				_loginToken;
	bool											_loggedIn;

private :
	SessionManager& operator=( const SessionManager& );
};

#endif
