#include "ApplicationManager.h"
#include "ConfigManager.h"
#include "RDMHeaders.h"


using namespace rfa;
using namespace rfa::common;
// using namespace rfa::config;
//using namespace rfa::logger;
using namespace rfa::sessionLayer;
//using namespace rfa::rdm;


ApplicationManager::ApplicationManager()
{
    clear();
}

void ApplicationManager::clear()
{
    ommProvider = 0;
	eventQueue = 0;
  	session = 0;
	reqRouter = 0; 
   	appConfigManager = 0;
   	rfaConfigManager = 0;

};

void ApplicationManager::destroy()
{
    if (reqRouter)
        delete reqRouter;

    if (session)
        session->release();

    if (eventQueue)
    {
        eventQueue->deactivate();
        eventQueue->destroy();
    }

    if (appConfigManager)
        appConfigManager->cleanup();
    if (rfaConfigManager)
        rfaConfigManager->cleanup();

};