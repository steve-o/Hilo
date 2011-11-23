#include "Authorizer.h"

#include "Config/ConfigTree.h"
#include "Message/AttribInfo.h"
#include <assert.h>
#include "ApplicationManager.h"
#include "ConfigManager.h"
#include "UserContext.h"

using namespace rfa::config;


RFA_String Authorizer::_domainName = "Login";
RFA_String Authorizer::_userNamesListName = "userNames";

Authorizer::Authorizer(ApplicationManager &mAppManager)
: DataGenerator(mAppManager)
{
    _pConfigTree = const_cast<rfa::config::ConfigTree*>(appManager.getAppConfigManager()->getTree(_domainName));
	assert( _pConfigTree );
}

Authorizer::~Authorizer()
{
}

bool Authorizer::isValidRequest(const rfa::message::AttribInfo& attrib)
{
    const RFA_String& userName = attrib.getName();
	const StringList* pStringList = _pConfigTree->getChildAsStringList(_userNamesListName);

	if (!pStringList || (pStringList->size() == 0)) {
		if ( pStringList ) delete pStringList;
		return true;  // default is to allow everyone
	}

	unsigned int i;

	for ( i = 0; i < pStringList->size(); i ++ )
	{
		const RFA_String& tmpUserName = ((*pStringList)[i]);
	
		if ( userName == tmpUserName ) {
			delete pStringList;
			return true;
		}
	}

	delete pStringList;

	return false;
}

StreamItem* Authorizer::createStreamItem(const rfa::message::AttribInfo& attrib)
{
	if (!isValidRequest(attrib))
		return 0;

	return new UserContext(attrib, *this);
}
