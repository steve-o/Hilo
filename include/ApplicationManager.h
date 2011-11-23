#ifndef _APPLICATION_MANAGER_H
#define _APPLICATION_MANAGER_H

#include "Common/EventQueue.h"
#include "SessionLayer/Session.h"
#include "SessionLayer/OMMProvider.h"

class ReqRouter;
class ConfigManager;

#include "RDMDict.h"

class ApplicationManager
{
public:
    ApplicationManager();
    
    void clear();
    void destroy();

   	inline ReqRouter * getReqRouter() { return reqRouter; } 
	inline rfa::common::EventQueue * getEventQueue() { return eventQueue; }
	inline rfa::sessionLayer::Session * getSession() { return session; }
	inline ConfigManager *getAppConfigManager() { return appConfigManager; }
	inline ConfigManager *getRFAConfigManager() { return rfaConfigManager; }
    inline rfa::sessionLayer::OMMProvider *getOMMProvider() { return ommProvider; }

   	inline void setReqRouter(ReqRouter *mReqRouter) {reqRouter = mReqRouter; } 
	inline void setAppConfigManager(ConfigManager *mAppConfigManager) { appConfigManager = mAppConfigManager; }
    inline void setOMMProvider(rfa::sessionLayer::OMMProvider *mOMMProvider) { ommProvider = mOMMProvider; }

    friend class VelocityProvider;
    friend class DictionaryGenerator;

protected:
    rfa::sessionLayer::OMMProvider *ommProvider;
	rfa::common::EventQueue *eventQueue;
  	rfa::sessionLayer::Session *session;
	ReqRouter *reqRouter;
   	ConfigManager *appConfigManager;
   	ConfigManager *rfaConfigManager;
    RDMFieldDict *rdmFieldDict;
};

#endif

