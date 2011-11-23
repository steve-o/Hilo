#include "DataGenerator.h"

#include "Config/ConfigTree.h"
#include "Data/FieldList.h"
#include "Data/FieldListWriteIterator.h"
#include "Data/Map.h"

DataGenerator::DataGenerator(ApplicationManager &mAppManager)
: appManager(mAppManager),_pConfigTree(0)
{
	GeneratorCount++;
	if (GeneratorCount == 1)
	{
		ReusableFieldList = new FieldList();
		ReusableFieldListWriteIterator = new FieldListWriteIterator();
		ReusableMap = new Map();
	}
}

DataGenerator::~DataGenerator()
{
	GeneratorCount--;
	if (GeneratorCount == 0)
	{
		delete ReusableFieldList;
		ReusableFieldList = 0;
		delete ReusableFieldListWriteIterator;
		ReusableFieldListWriteIterator = 0;
		delete ReusableMap;
		ReusableMap = 0;
	}
}

bool DataGenerator::isValidRequest(const rfa::message::AttribInfo& attrib)
{
	return true;
}

void DataGenerator::removeStreamItem(StreamItem & streamItem)
{
}

const rfa::config::ConfigTree* DataGenerator::getDomainConfigTree( void ) const
{
	return _pConfigTree;
}

int DataGenerator::GeneratorCount = 0;
FieldList * DataGenerator::ReusableFieldList = 0;
FieldListWriteIterator * DataGenerator::ReusableFieldListWriteIterator = 0;
Map * DataGenerator::ReusableMap = 0;
