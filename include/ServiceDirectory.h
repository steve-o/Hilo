#ifndef _SERVICE_DIRECTORY_H
#define _SERVICE_DIRECTORY_H

#include "DataGenerator.h"
#include "Common/RFA_String.h"

class ServiceDirectoryStreamItem;

class ServiceDirectory : public DataGenerator
{
public:
	ServiceDirectory(ApplicationManager & appManager);
	virtual ~ServiceDirectory();

	virtual StreamItem* createStreamItem(const rfa::message::AttribInfo& attrib);
	virtual void removeStreamItem(StreamItem & streamItem);

	const rfa::config::ConfigTree* getServiceConfigTree(const rfa::common::RFA_String& serviceName);

protected:
	static rfa::common::RFA_String		_domainName;
};

#endif

