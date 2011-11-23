#include "DictionaryGenerator.h"

#include "Common/RFA_String.h"
#include "Message/AttribInfo.h"
#include "RDM/RDM.h"
#include "RDMDict.h"
#include "DictionaryStreamItem.h"
#include <assert.h>

#include "ApplicationManager.h"

using namespace std;
using namespace rfa::common;
using namespace rfa::rdm;

DictionaryGenerator::DictionaryGenerator(ApplicationManager & appManager)
: DataGenerator(appManager)
{
}

DictionaryGenerator::~DictionaryGenerator()
{

}


void DictionaryGenerator::addDictionary(const RFA_String & dictionaryName, const RDMDict & dictionary)
{
    _dictionaries.insert( MapDict::value_type( dictionaryName.c_str(), &dictionary ) );
}

StreamItem* DictionaryGenerator::createStreamItem( const rfa::message::AttribInfo& attribInfo )
{
    if (attribInfo.getName() == "RWFFld")
        return new FieldDictionaryStreamItem(attribInfo, *this, "RWFFld", (UInt8) attribInfo.getDataMask(), *appManager.rdmFieldDict);
    else // assumes ENUM
        return new EnumDictionaryStreamItem(attribInfo, *this, "RWFEnum", (UInt8) attribInfo.getDataMask(), appManager.rdmFieldDict->enumDict());

}

void DictionaryGenerator::removeStreamItem(StreamItem & streamItem)
{
	assert( streamItem.getNumOfRequestTokens() == 0 );
	delete &streamItem;
}
