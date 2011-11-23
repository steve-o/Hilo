#ifndef _AUTHORIZER_H
#define _AUTHORIZER_H

#include "DataGenerator.h"
#include "Common/RFA_String.h"

using namespace rfa::common;
using namespace rfa::message;

class ApplicationManager;

class Authorizer : public DataGenerator
{
public:
	Authorizer(ApplicationManager &mAppManager);
	virtual ~Authorizer();

	virtual StreamItem* createStreamItem(const AttribInfo& attrib);
	virtual bool isValidRequest(const AttribInfo& attrib);
	
protected:
	static RFA_String		_domainName;
	static RFA_String		_userNamesListName;
};

#endif
