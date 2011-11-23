#include "RDMDictDef.h"

#include <iostream>

void RDMEnumDef::dumpEnumDef() const
{
	std::list<Int16> _fidslist = fids();
	list<Int16>::const_iterator it;

	cout<<"Enum fid list: ";
	for(it=_fidslist.begin(); it!=_fidslist.end(); ++it)
	{
		cout << *it <<","; 
	} 
	cout<<endl;

	EnumValues _valsMap = values();
	EnumValues::const_iterator vIt;
	cout<<"Enum value pairs: ";
	for(vIt=_valsMap.begin(); vIt!=_valsMap.end(); ++vIt)
	{
		cout << vIt->first << "," <<vIt->second<<" "; // each element on a separate line
	} 
	cout<<endl;

}


RDMFieldDef::RDMFieldDef() : 
		_dataType( DataBuffer::NoDataBufferEnum ),
		_mfFieldLength(0),
		_mfEnumLength(0),
		_maxFieldLength(0),
		_pEnumDef(0),
		_rippleFieldFid(0)

{
}


RDMFieldDef::RDMFieldDef( const string & name, 
			  rfa::data::DataBuffer::DataBufferEnumeration dataType,
			  const string & displayName) :
			_name(name),
			_dataType(dataType),
			_displayName(displayName),
            _mfFieldLength(0),
            _mfEnumLength(0),
	        _maxFieldLength(0),
	        _pEnumDef(0)

{
}


const string & RDMFieldDef::getEnumString ( rfa::common::Int16 enumVal ) const 
{
	return _pEnumDef ? _pEnumDef->findEnumVal( enumVal ) : RDMEnumDef::getEmptyEnumString(); 
}

#define TRY_FIELD_TYPE_AND_RETURN_EX(VAL,ENUM) \
		if( mfFieldTypeName == #VAL ) { _mfFieldType = ENUM; return; } else ((void)0)

void RDMFieldDef::setMFFieldTypeName(const std::string& mfFieldTypeName)
{
	TRY_FIELD_TYPE_AND_RETURN_EX(TIME_SECONDS, 0);
	TRY_FIELD_TYPE_AND_RETURN_EX(INTEGER, 1);
	TRY_FIELD_TYPE_AND_RETURN_EX(NUMERIC, 2);
	TRY_FIELD_TYPE_AND_RETURN_EX(DATE, 3);
	TRY_FIELD_TYPE_AND_RETURN_EX(PRICE, 4);
	TRY_FIELD_TYPE_AND_RETURN_EX(ALPHANUMERIC, 5);
	TRY_FIELD_TYPE_AND_RETURN_EX(ENUMERATED, 6);
	TRY_FIELD_TYPE_AND_RETURN_EX(TIME, 7);
	TRY_FIELD_TYPE_AND_RETURN_EX(BINARY, 8);
	TRY_FIELD_TYPE_AND_RETURN_EX(LONG_ALPHANUMERIC, 9);

	_mfFieldType = -1; // Unknown
}

#define TRY_DATA_TYPE_AND_RETURN_EX(VAL,ENUM) \
		if( dataTypeStr == #VAL ) { _dataType = DataBuffer:: ENUM##Enum; return; } else ((void)0)
#define TRY_DATA_FORMAT_AND_RETURN_EX(VAL,ENUM) \
		if( dataTypeStr == #VAL ) { _dataType =  ENUM##Enum; return; } else ((void)0)
#define	TRY_DATA_TYPE_AND_RETURN(VAL)  \
		if( dataTypeStr == #VAL ) { _dataType = DataBuffer:: VAL##Enum; return; } else ((void)0)



//maps from RWF string found in appendix_a to RFA DataBufferEnums
void	RDMFieldDef::setDataType( const std::string & dataTypeStr, int length )
{

	//map RWF types in 0.3 of appendix_a to RFA types
	TRY_DATA_TYPE_AND_RETURN_EX(INT32,Int32);
	TRY_DATA_TYPE_AND_RETURN_EX(UINT32,UInt32);
	TRY_DATA_TYPE_AND_RETURN_EX(INT64,Int64);
	TRY_DATA_TYPE_AND_RETURN_EX(UINT64,UInt64);

	// try new data type UINT, INT with length
	// 4 bytes or less means Int32 or UInt32
	if ( length <= 4 ) {
		TRY_DATA_TYPE_AND_RETURN_EX(INT,Int32);
	}
	else {
		TRY_DATA_TYPE_AND_RETURN_EX(INT,Int64);
	}
	if ( length <= 4 ) {
		TRY_DATA_TYPE_AND_RETURN_EX(UINT,UInt32);
	}
	else {
		TRY_DATA_TYPE_AND_RETURN_EX(UINT,UInt64);
	}

	TRY_DATA_TYPE_AND_RETURN_EX(ENUM,Enumeration);

	TRY_DATA_TYPE_AND_RETURN_EX(TIME,Time);

	TRY_DATA_TYPE_AND_RETURN_EX(FLOAT,Float);
	TRY_DATA_TYPE_AND_RETURN_EX(DOUBLE,Double);
	TRY_DATA_TYPE_AND_RETURN_EX(STATE,RespStatus);
	TRY_DATA_TYPE_AND_RETURN_EX(QOS, QualityOfServiceInfo);
	TRY_DATA_TYPE_AND_RETURN_EX(ANSI_PAGE,ANSI_Page);

	TRY_DATA_TYPE_AND_RETURN_EX(REAL32 ,Real32);
	TRY_DATA_TYPE_AND_RETURN_EX(REAL64 ,Real64);

	// try new data type REAL with length
	// 5 bytes or less means Real32
	if ( length <= 5 ) {
		TRY_DATA_TYPE_AND_RETURN_EX(REAL,Real32);
	}
	else {
		TRY_DATA_TYPE_AND_RETURN_EX(REAL,Real64);
	}

	TRY_DATA_TYPE_AND_RETURN_EX(DATE,Date);
	TRY_DATA_TYPE_AND_RETURN_EX(DATETIME,DateTime);

	TRY_DATA_TYPE_AND_RETURN_EX(BUFFER, Buffer);

	TRY_DATA_TYPE_AND_RETURN_EX(ASCII_STRING, StringAscii);
	TRY_DATA_TYPE_AND_RETURN_EX(RMTES_STRING, StringRMTES);
	
	TRY_DATA_TYPE_AND_RETURN_EX(UTF8_STRING, StringUTF8);

	TRY_DATA_FORMAT_AND_RETURN_EX(VECTOR, Vector);
	TRY_DATA_FORMAT_AND_RETURN_EX(FILTER_LIST, FilterList);
	TRY_DATA_FORMAT_AND_RETURN_EX(ELEMENT_LIST, ElementList);
	TRY_DATA_FORMAT_AND_RETURN_EX(FIELD_LST, FieldList);
	TRY_DATA_FORMAT_AND_RETURN_EX(MAP, Map);
	TRY_DATA_FORMAT_AND_RETURN_EX(SERIES, Series);
	TRY_DATA_FORMAT_AND_RETURN_EX(ARRAY, Array);

	//no match
	_dataType =  DataBuffer::UnknownDataBufferEnum;
};

void RDMFieldDef::dumpFieldDef() const
{
	cout<<getFieldId()<<"	"
		<<getName()<<"	"
		<<getDisplayName()<<"	"
		<<getRipplesToFieldName()<<"	"
		<<getMFFieldType()<<"	"
		<<getMFFieldLength()<<"	"
		<<getMFEnumLength()<<"	"
		<<getMaxFieldLength()<<"	"
		<<getDataType()<<"	"
		<<endl;

}
