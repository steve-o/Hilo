#ifndef __SESSION_LAYER_COMMON__RDMFieldDef__H__
#define __SESSION_LAYER_COMMON__RDMFieldDef__H__


// RFA Includes
#include "Data/DataBuffer.h"

#include <string>
#include <list>
#include <map>

using namespace std;
using namespace rfa::common;
using namespace rfa::data;


typedef std::map< UInt16, string> EnumValues;

// Enum Definitiona Declaration
class RDMEnumDef
{
public:
		void	setDisplayName( char* s) 
		{
			_enumString = std::string(s);
		}

		void	setEnumVal(int val)
		{
			_enumVal = val;
		}

		void	insertEnumVal()
		{
			_enumValMap.insert( EnumValues::value_type(_enumVal, _enumString));
		}	

		void	insertEnumVal( UInt16 enumVal, const std::string& enumString )
		{
			_enumString = enumString;
			_enumValMap.insert( EnumValues::value_type( enumVal, _enumString ) );
		}

		const std::string & findEnumVal( UInt16 enumVal ) const
		{
			EnumValues::const_iterator cit = _enumValMap.find( enumVal );

			return (cit != _enumValMap.end()) ? cit->second : _emptyEnumString;
		}

		static const std::string & getEmptyEnumString() { return _emptyEnumString; }

		const EnumValues values() const
		{
			return _enumValMap;
		}

		const std::list<Int16> fids() const
		{
			return _fids;
		}

		void addFid( Int16 fid )
		{
			_fids.push_back(fid);
		}

		void dumpEnumDef() const;

protected:
		EnumValues				_enumValMap;
		std::list<Int16> _fids;


		static	std::string			_emptyEnumString;
		std::string		_enumString;
		int				_enumVal;
};


// Field Definitition Declaration
class RDMFieldDef
{
public:
		RDMFieldDef();
		RDMFieldDef( const std::string & name, 
					  DataBuffer::DataBufferEnumeration dataType,
					  const std::string & displayName);


		void	setFieldId( Int16 fieldId ) { _fieldId = fieldId; }
		const	Int16 getFieldId() const { return _fieldId; }

		void	setName( const std::string & name ) { _name = name; }
		const	std::string & getName() const { return _name; }

        void    setDisplayName( const std::string & displayName ) { _displayName = displayName; }
        const   std::string & getDisplayName() const { return _displayName; }

        void    setRipplesToFieldName( const std::string & rippleFieldName ) { _rippleFieldName = rippleFieldName; }
        void    setRipplesToFid( Int32 rippleFieldFid ) { _rippleFieldFid = rippleFieldFid; }
        const   std::string & getRipplesToFieldName() const { return _rippleFieldName; }
        const   unsigned int getRipplesToFid() const { return _rippleFieldFid; }

        void    setMFFieldTypeName( const std::string & mfFieldTypeName); 
        void    setMFFieldType( Int32 fieldType) { _mfFieldType = fieldType; }
        const   Int32	getMFFieldType() const { return _mfFieldType; }

        void    setMFFieldLength( Int32 mfFieldLength) { _mfFieldLength = mfFieldLength; }
        const   Int32  getMFFieldLength() const { return _mfFieldLength; }

        void    setMFEnumLength( Int32 mfEnumLength) { _mfEnumLength = mfEnumLength; }
        const   Int32  getMFEnumLength() const { return _mfEnumLength; }

		void	setDataType( DataBuffer::DataBufferEnumeration dataType )
		{
			_dataType = dataType;
		}

		void	resetDataType() { return; };

        UInt8	getDataType() const { return _dataType; }

		// does string to Common::DataBufferEnum translation
		void	setDataType( const std::string & dataTypeStr, int length = 0 );
		void	setDataType( UInt8 dataType ) {_dataType = dataType;};

        void    setMaxFieldLength( unsigned int  maxFieldLength) { _maxFieldLength = maxFieldLength; }
        const   int  getMaxFieldLength() const { return _maxFieldLength; }

		// EnumDef is just referenced so it's not included into "constness"
		void	 setEnumDef( const RDMEnumDef* pEnumDef ) { _pEnumDef = pEnumDef; }
		void	 setEnumDef( const RDMEnumDef* pEnumDef ) const { _pEnumDef = pEnumDef; }
		const	 RDMEnumDef* getEnumDef() const { return _pEnumDef; }		

		const std::string & getEnumString ( Int16 enumVal ) const;

		void dumpFieldDef() const;

private:
		std::string									   _name;
		Int16											_fieldId;
		UInt8											_dataType;
		std::string									   _displayName;
		std::string                                    _rippleFieldName;
		Int32										   _rippleFieldFid;
        Int32										   _mfFieldType;
        Int32                                          _mfFieldLength;
        Int32						                   _mfEnumLength;
        Int32								            _maxFieldLength;
        
		mutable const RDMEnumDef						*_pEnumDef;

};

#endif
