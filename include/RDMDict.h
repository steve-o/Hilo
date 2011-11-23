#ifndef __SESSION_LAYER_COMMON__RDMDict__H__
#define __SESSION_LAYER_COMMON__RDMDict__H__

/**
	\class RDMDict RDMDict.h 
	\brief
	Encapsultes data dictionaries and can populate from file or network.

*/

// STL Includes
#include <map>
#include <list>
#include <vector>
#include <new>

// RFA Includes
#include "Common/DataDef.h"
#include "Data/DataBuffer.h"
#include "Data/ElementList.h"
#include "RDMDictDef.h"

// Forward declarations
class RDMFieldDict;
class RDMEnumDict;

typedef std::map<Int16, const RDMEnumDef *>	EnumTableMap;
typedef std::list<const RDMEnumDef*> EnumTables;

// Decoder
typedef std::map<UInt16, const DataDef *>  DefMap;
typedef std::vector<Int16>  FidIds;
typedef std::map< std::string, Int16 > FidMap;

using namespace std;
using namespace rfa::common;
using namespace rfa::data;

// Auxilary class to prevent copying
class __NO_COPY
{
protected:
	__NO_COPY() {}
	virtual ~__NO_COPY() {}
private:
	__NO_COPY( const __NO_COPY & );
	__NO_COPY operator=( const __NO_COPY & );
};

// Base dict class
class RDMDict : private __NO_COPY
{
	public:
		RDMDict();

		void		setDictId( Int32 dictId ) { _dictId = dictId; };
		Int32		getDictId() const { return _dictId; }

		UInt16		getDictType() const { return _dictType; }

		void		setVersion(std::string version) {_version = version;};
		std::string getVersion() const {return _version;};
		bool		isComplete() const {return _isComplete;}
		static UInt32 Trace;
protected:
		bool		_isComplete;
		Int32		_dictId;
		UInt16		_dictType;
		std::string _version;
};


// Enum Dictionary Declaration
class RDMEnumDict : public RDMDict
{
public:
		RDMEnumDict();
		virtual ~RDMEnumDict();
		void	insertEnumDef( const RDMEnumDef * enumDef );

		const	RDMEnumDef	* findEnumDef( Int16 fieldId ) const
		{
			EnumTableMap::const_iterator cit = _enumDefMap.find( fieldId );
			return cit != _enumDefMap.end() ? cit->second : 0;
		}

		const EnumTableMap & enums() const
		{
			return _enumDefMap;
		}
		const EnumTables & enumList() const
		{
			return _enumDefList;
		}

protected:
		
		EnumTableMap		_enumDefMap;
		EnumTables			_enumDefList;
};

// Field Data Dictionary Declaration
class RDMFieldDict : public RDMDict
{
public:
	RDMFieldDict();
	virtual ~RDMFieldDict();
	void putFieldDef( const RDMFieldDef *pFieldDef );

	const RDMFieldDef * getFieldDef( Int16 fieldId ) const
	{
		if (fieldId > 0)
			return (fieldId > _maxPosFieldId) ? 0 : _posFieldDefinitions[fieldId];
		else if (fieldId < 0)
			return (fieldId < _minNegFieldId) ? 0 : _negFieldDefinitions[-fieldId];
		else
			return 0;
	}

	const RDMFieldDef * getFieldDef( const std::string & fieldName ) const;

	RDMEnumDict & enumDict() { return _enumDict; }
	UInt16 maxFieldLength() const { return _maxFieldLength; }
	Int16 maxPositiveFieldId() const { return _maxPosFieldId; }
	Int16 minNegativeFieldId() const { return _minNegFieldId; }
	UInt16 count() const { return _count; }

protected:
	friend class RDMFileDictionaryDecoder;
	friend class RDMNetworkDictionaryDecoder;
	void fixRipple();
	void associateEnumDict();

	void resizePosFieldDefinitions( Int16 fieldId );
	void resizeNegFieldDefinitions( Int16 posFieldId );

	UInt16					_count;
	Int16					_maxPosFieldId;
	Int16					_minNegFieldId;
	UInt16					_posFieldDefinitionsSize;
	UInt16					_negFieldDefinitionsSize;
	const RDMFieldDef **	_posFieldDefinitions;
	const RDMFieldDef **	_negFieldDefinitions;
	RDMEnumDict 			_enumDict;
	UInt16					_maxFieldLength;
	FidMap					_fidByNameMap;

};

#endif // __SESSION_LAYER_COMMON__RDMDict__H__
