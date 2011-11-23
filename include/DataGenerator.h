#ifndef _DATAGENERATOR_H
#define _DATAGENERATOR_H

namespace rfa {
	namespace data {
		class FieldList;
		class FieldListWriteIterator;
		class Map;
	}

	namespace message {
		class AttribInfo;
	}

	namespace config {
		class ConfigTree;
	}
}

using namespace rfa::data;
using namespace rfa::message;

class StreamItem;
class DomainManager;
class ApplicationManager;

class DataGenerator
{
public:
	DataGenerator(ApplicationManager &mAppManager);
	virtual ~DataGenerator();

	inline FieldList & reusableFieldList() { return *ReusableFieldList; }
	inline FieldListWriteIterator & reusableFieldListWriteIterator() { return *ReusableFieldListWriteIterator; }
	inline Map & reusableMap() { return *ReusableMap; }

	virtual StreamItem* createStreamItem(const AttribInfo& attrib) = 0;

	virtual void removeStreamItem(StreamItem & streamItem);

	virtual bool isValidRequest(const AttribInfo& attrib);
	const rfa::config::ConfigTree* getDomainConfigTree( void ) const;

	inline void setDomainManager(DomainManager & DomainManager) { domainManager = &DomainManager; }
	inline DomainManager & getDomainManager() { return *domainManager; }

protected:
	ApplicationManager &appManager;
	DomainManager *domainManager;

	static int GeneratorCount;
	static FieldList * ReusableFieldList;
	static FieldListWriteIterator * ReusableFieldListWriteIterator;
	static Map * ReusableMap;

	const rfa::config::ConfigTree * _pConfigTree;

private :
	DataGenerator& operator=( const DataGenerator& );

};

#endif
