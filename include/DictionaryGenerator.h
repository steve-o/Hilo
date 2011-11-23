#ifndef _DICTIONARY_GENERATOR_H
#define _DICTIONARY_GENERATOR_H

#include "DataGenerator.h"
#include "Data/Series.h"
#include "RDMDict.h"

#include <map>
#include <string>

namespace rfa {
	namespace data {

	}
	namespace common {
		class RFA_String;
	}
	namespace sessionLayer {
		class RequestToken;
	}
}


typedef std::map<std::string, const RDMDict*> MapDict;

class RDMDict;

class DictionaryGenerator : public DataGenerator
{
public:
	DictionaryGenerator(ApplicationManager &appManager);
	virtual ~DictionaryGenerator();

	void addDictionary(const rfa::common::RFA_String & dictionaryName, const RDMDict & dictionary);
	virtual StreamItem* createStreamItem(const rfa::message::AttribInfo& attrib);
	
	inline rfa::data::Series & reusableSeries() { return _reusableSeries; }

	virtual void	removeStreamItem(StreamItem & streamItem);

protected:
	MapDict _dictionaries;
	rfa::data::Series _reusableSeries;
};

#endif

