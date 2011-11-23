#ifdef WIN32
#pragma warning(disable:  4786)
#endif

#include "RDMDict.h"
#include "RDM/RDM.h"
#include "Data/Series.h"
#include "Data/Array.h"

#include <iostream>
#include <stdlib.h>
#include <new>

using namespace rfa::rdm;


RDMDict::RDMDict() :
	_dictId(0), _isComplete(false)
{
}

RDMFieldDict::RDMFieldDict() :
	_posFieldDefinitionsSize(0),
	_negFieldDefinitionsSize(0),
	_posFieldDefinitions(0),
	_negFieldDefinitions(0),
	_maxPosFieldId(0),
	_minNegFieldId(0),
	_maxFieldLength(0),
	_count(0)
{
	resizePosFieldDefinitions(4000);
	_dictType = DICTIONARY_FIELD_DEFINITIONS;
}

RDMFieldDict::~RDMFieldDict()
{
	for( UInt16 i = 1; i < _posFieldDefinitionsSize; i++)
		delete _posFieldDefinitions[i];

	for( UInt16 j = 1; j < _negFieldDefinitionsSize; j++)
		delete _negFieldDefinitions[j];

	_fidByNameMap.clear();

	delete [] _posFieldDefinitions;
	delete [] _negFieldDefinitions;
}

void RDMFieldDict::putFieldDef( const RDMFieldDef *pFieldDef )
{
	Int16 fieldId = pFieldDef->getFieldId();
	
	// map name to fieldId
	_fidByNameMap.insert( FidMap::value_type( pFieldDef->getName(), fieldId ) );

	if (fieldId > 0)
	{
		// need to resize the array
		if ( fieldId >= _posFieldDefinitionsSize ) resizePosFieldDefinitions( fieldId );

		// keep track of the largest "positive" fieldId
		if ( _maxPosFieldId < fieldId )	_maxPosFieldId = fieldId;

		// count number of unique field ids
		if ( _posFieldDefinitions[fieldId] == 0 ) _count++;

		_posFieldDefinitions[fieldId] = pFieldDef;
	}
	else
	{
		int posFieldId = -fieldId;

		if ( posFieldId >= _negFieldDefinitionsSize )
		{
			resizeNegFieldDefinitions( posFieldId );
		}

		// keep track of the largest "negative" fieldId
		if ( _minNegFieldId > fieldId ) _minNegFieldId = fieldId;

		// count number of unique field ids
		if ( _negFieldDefinitions[posFieldId] == 0 ) _count++;

		_negFieldDefinitions[posFieldId] = pFieldDef;
	}
	if ( _maxFieldLength < pFieldDef->getMaxFieldLength() )
        _maxFieldLength = pFieldDef->getMaxFieldLength();
}

void RDMFieldDict::resizePosFieldDefinitions(Int16 fieldId)
{
	int newDefSize = max(fieldId + 1, _posFieldDefinitionsSize*2);
	const RDMFieldDef ** newDefs = new const RDMFieldDef*[newDefSize];
	int i = 1;
	for (; i < _posFieldDefinitionsSize; i++)
		newDefs[i] = _posFieldDefinitions[i];
	for (; i < newDefSize; i++)
		newDefs[i] = 0;
	newDefs[0] = 0;
	delete [] _posFieldDefinitions;
	_posFieldDefinitions = newDefs;
	_posFieldDefinitionsSize = newDefSize;
}

void RDMFieldDict::resizeNegFieldDefinitions(Int16 posFieldId)
{
	int newDefSize = max(posFieldId + 1, _negFieldDefinitionsSize*2);
	const RDMFieldDef ** newDefs = new const RDMFieldDef*[newDefSize];
	int i = 1;
	for (; i < _negFieldDefinitionsSize; i++)
		newDefs[i] = _negFieldDefinitions[i];
	for (; i < newDefSize; i++)
		newDefs[i] = 0;
	newDefs[0] = 0;
	delete [] _negFieldDefinitions;
	_negFieldDefinitions = newDefs;
	_negFieldDefinitionsSize = newDefSize;
}

void	RDMFieldDict::associateEnumDict(  )
{
	UInt16 i;

	for( i = 1; i <= _maxPosFieldId; i++)
	{
		const RDMFieldDef * def = _posFieldDefinitions[i];
		if (!def)
			continue;
		if (def->getDataType() == DataBuffer::EnumerationEnum )
			def->setEnumDef( _enumDict.findEnumDef( i ) );
	}

	Int16 j;

	for( j = -1; j > _minNegFieldId; j--)
	{
		const RDMFieldDef * def = _negFieldDefinitions[-j];
		if (!def)
			continue;
		if (def->getDataType() == DataBuffer::EnumerationEnum )
			def->setEnumDef( _enumDict.findEnumDef( j ) );
	}

	if (Trace & 0x2)
		cout << "enumeration tables associated" << endl;
	_isComplete = true;
}

const RDMFieldDef * RDMFieldDict::getFieldDef( const std::string & fieldName ) const
{
	FidMap::const_iterator cit = _fidByNameMap.find( fieldName );

	Int16 fieldId = ( cit != _fidByNameMap.end() ? cit->second : 0 );

	if ( fieldId < 0 ) {
		const RDMFieldDef * fieldDef = _negFieldDefinitions[ -fieldId ];
		return fieldDef;
	}
	else {
		const RDMFieldDef * fieldDef = _posFieldDefinitions[ fieldId ];
		return fieldDef;
	}

}

void	RDMFieldDict::fixRipple() 
{
	RDMFieldDef* def;

	for( UInt16 i = 1; i <= _maxPosFieldId; i++)
    {
		def = const_cast<RDMFieldDef*>(_posFieldDefinitions[i]);
		if (!def)
			continue;
		if (def->getRipplesToFid() == 0)
		{
			if (def->getRipplesToFieldName() == "")
			{
				continue;
			}
			else if (def->getRipplesToFieldName()=="NULL")
			{
				def->setRipplesToFid(0);
				def->setRipplesToFieldName("");
			}
			else
			{
				const RDMFieldDef * rippleDef = getFieldDef(def->getRipplesToFieldName());
				if (rippleDef)
					def->setRipplesToFid(rippleDef->getFieldId());
			}
		}
		else
		{
			const RDMFieldDef * rippleDef = getFieldDef(def->getRipplesToFid());
			if (rippleDef)
				def->setRipplesToFieldName(rippleDef->getName());
		}
	}

	for( Int16 i = -1; i >= _minNegFieldId; i--)
    {
		def = const_cast<RDMFieldDef*>(_negFieldDefinitions[-i]);
		if (!def)
			continue;
		if (def->getRipplesToFid() == 0)
		{
			if (def->getRipplesToFieldName() == "")
			{
				continue;
			}
			else if (def->getRipplesToFieldName()=="NULL")
			{
				def->setRipplesToFid(0);
				def->setRipplesToFieldName("");
			}
			else
			{
				const RDMFieldDef * rippleDef = getFieldDef(def->getRipplesToFieldName());
				if (rippleDef)
					def->setRipplesToFid(rippleDef->getFieldId());
			}
		}
		else
		{
			const RDMFieldDef * rippleDef = getFieldDef(def->getRipplesToFid());
			if (rippleDef)
				def->setRipplesToFieldName(rippleDef->getName());
		}
	}
	if (Trace & 0x2)
		cout << "ripples associated" << endl;
}

RDMEnumDict::RDMEnumDict()
{
	_dictType = DICTIONARY_ENUM_TABLES;
}

RDMEnumDict::~RDMEnumDict() 
{
	for( EnumTables::iterator it = _enumDefList.begin();
		 it != _enumDefList.end();
		 delete *it, ++it );

}

void	RDMEnumDict::insertEnumDef( const RDMEnumDef * enumDef )
{		
	const list<rfa::common::Int16> & fidIds = enumDef->fids();
	for( list<rfa::common::Int16>::const_iterator cit = fidIds.begin();
		 cit != fidIds.end(); ++cit )
		 _enumDefMap.insert( EnumTableMap::value_type( *cit, enumDef ) );
	
	_enumDefList.push_back( enumDef );
}

string RDMEnumDef::_emptyEnumString = "Not provided";
UInt32 RDMDict::Trace = 0;
