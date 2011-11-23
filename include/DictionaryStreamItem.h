#ifndef _DICTIONARY_STREAMITEM_H
#define _DICTIONARY_STREAMITEM_H

#include "StreamItem.h"
#include "Common/Common.h"
#include "Common/RFA_String.h"

class RDMFieldDict;
class RDMEnumDict;
class RDMDictionaryEncoder;

using namespace rfa::common;
using namespace rfa::message;

class DictionaryStreamItem : public StreamItem
{
public:
	DictionaryStreamItem(const AttribInfo & attribInfo,
						 DataGenerator & dataGenerator,
						 const RFA_String & dictionaryName, 
						 rfa::common::UInt8 verbosity);

	virtual ~DictionaryStreamItem() {}

	bool satisfies(const AttribInfo & attribInfo) const;
	void createStatus(RespStatus & status);

protected:
	const RFA_String _dictionaryName;
	const rfa::common::UInt8 _verbosity;
};

class FieldDictionaryStreamItem : public DictionaryStreamItem
{
public:
	FieldDictionaryStreamItem(const AttribInfo& attribInfo,
							  DataGenerator & dataGenerator,
							  const RFA_String & dictionaryName, 
							  UInt8 verbosity, 
							  const RDMFieldDict &);
	virtual ~FieldDictionaryStreamItem();

	void createData(Data & data, RespMsg::RespType respType, UInt32 maxSize = 800000);
	void continueData(Data & data, UInt32 maxSize = 800000);

protected:
	RDMDictionaryEncoder * _encoder;
	const RDMFieldDict & _rdmFieldDict;
};

class EnumDictionaryStreamItem : public DictionaryStreamItem
{
public:
	EnumDictionaryStreamItem(const AttribInfo& attribInfo,
							 DataGenerator & dataGenerator,
							 const RFA_String & dictionaryName,
							 UInt8 verbosity,
							 const RDMEnumDict &);
	virtual ~EnumDictionaryStreamItem();

	void createData(Data & data, RespMsg::RespType respType, UInt32 maxSize = 800000);
	void continueData(Data & data, UInt32 maxSize = 800000);

protected:
	RDMDictionaryEncoder * _encoder;
	const RDMEnumDict & _rdmEnumDict;
};

#endif
