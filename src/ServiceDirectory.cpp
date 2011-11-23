
#include "ServiceDirectory.h"
#include "StreamItem.h"
#include "ConfigManager.h"
#include "ApplicationManager.h"
#include "ServiceDirectoryStreamItem.h"
#include "Message/AttribInfo.h"
#include "Common/RFA_String.h"
#include "Config/ConfigTree.h"


#include <assert.h>

rfa::common::RFA_String ServiceDirectory::_domainName = "Services";

ServiceDirectory::ServiceDirectory(ApplicationManager &mAppManager)
: DataGenerator(mAppManager)
{
	_pConfigTree = mAppManager.getAppConfigManager()->getTree(_domainName);
	assert( _pConfigTree );
}

ServiceDirectory::~ServiceDirectory()
{
	
}


StreamItem* ServiceDirectory::createStreamItem(const rfa::message::AttribInfo& attrib)
{
	return new ServiceDirectoryStreamItem(attrib, *this);
}

void ServiceDirectory::removeStreamItem(StreamItem & streamItem)
{
	assert(streamItem.getNumOfRequestTokens() == 0);
	delete &streamItem;
}

const rfa::config::ConfigTree* ServiceDirectory::getServiceConfigTree(const rfa::common::RFA_String& serviceName)
{
	if ( serviceName.length() )
		//  find the particular sub configTree for the service
		return _pConfigTree->getChildAsTree(serviceName);
	else
		// otherwise for all services request, return the configTree
		return _pConfigTree;		
	
}
