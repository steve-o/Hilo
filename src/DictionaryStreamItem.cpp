#include "DictionaryStreamItem.h"

#include "Common/Data.h"
#include "Message/AttribInfo.h"

#include "RDMDict.h"
#include "RDMDictionaryEncoder.h"

DictionaryStreamItem::DictionaryStreamItem(const AttribInfo& attribInfo,
										   DataGenerator & dataGenerator,
										   const RFA_String & dictionaryName,
										   UInt8 verbosity)
					:StreamItem(attribInfo,dataGenerator),
					_dictionaryName(dictionaryName),
					_verbosity(verbosity)
{
}

bool DictionaryStreamItem::satisfies(const AttribInfo & attribInfo) const
{
	if (!	((attribInfo.getHintMask() & AttribInfo::NameFlag) &&
			 (attribInfo.getHintMask() & AttribInfo::DataMaskFlag))	)
		 return false;
	return attribInfo.getName() == _dictionaryName && _verbosity == attribInfo.getDataMask();
}

void DictionaryStreamItem::createStatus(RespStatus & status)
{
}

FieldDictionaryStreamItem::FieldDictionaryStreamItem(const AttribInfo& attribInfo,
													 DataGenerator & dataGenerator,
													 const RFA_String & dictionaryName, UInt8 verbosity,
													 const RDMFieldDict & fieldDict)
							: DictionaryStreamItem(attribInfo, dataGenerator, dictionaryName, verbosity), _rdmFieldDict(fieldDict)
{
	_encoder = new RDMDictionaryEncoder();
}

FieldDictionaryStreamItem::~FieldDictionaryStreamItem()
{
	delete _encoder;
}

void FieldDictionaryStreamItem::createData( Data & data, RespMsg::RespType respType, UInt32 maxSize )
{
	_encoder->encodeFieldDictionary(static_cast<Series&>(data),_rdmFieldDict, _verbosity, RDMDictionaryEncoder::Speed, maxSize);
}

void FieldDictionaryStreamItem::continueData( Data & data, UInt32 maxSize)
{
	_encoder->continueEncoding(static_cast<Series&>(data));
}

EnumDictionaryStreamItem::EnumDictionaryStreamItem(const AttribInfo& attribInfo,
												   DataGenerator & dataGenerator,
												   const RFA_String & dictionaryName,
												   UInt8 verbosity,
										           const RDMEnumDict & enumDict)
: DictionaryStreamItem( attribInfo, dataGenerator, dictionaryName, verbosity), _rdmEnumDict(enumDict)
{
	_encoder = new RDMDictionaryEncoder();
}

EnumDictionaryStreamItem::~EnumDictionaryStreamItem()
{
}

void EnumDictionaryStreamItem::createData( Data & data, RespMsg::RespType respType, UInt32 maxSize)
{
	_encoder->encodeEnumDictionary(static_cast<Series&>(data), _rdmEnumDict, _verbosity, RDMDictionaryEncoder::Speed, maxSize);
}

void EnumDictionaryStreamItem::continueData( Data & data, UInt32 maxSize )
{
	_encoder->continueEncoding(static_cast<Series&>(data));
}
