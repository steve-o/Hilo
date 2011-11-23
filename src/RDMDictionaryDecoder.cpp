
#include "RDMDictionaryDecoder.h"

#include "Data/ElementListDef.h"
#include "Data/ElementEntryDef.h"
#include "Data/ElementEntry.h"

#include "Data/Series.h"
#include "Data/SeriesReadIterator.h"
#include "Data/ArrayReadIterator.h"
#include "Data/ElementListReadIterator.h"

#include <strstream>
#include <iostream>
#include <fstream>

using namespace std;
using namespace rfa::common;
using namespace rfa::rdm;

////////////////////////////////////////////////////////////////////
/// Begin RDMFileDictionaryDecoder
////////////////////////////////////////////////////////////////////

RDMFileDictionaryDecoder::RDMFileDictionaryDecoder(RDMFieldDict & dictionary)
:_dictionary(dictionary)
{
	_enumDict = &dictionary.enumDict();
}

RDMFileDictionaryDecoder::~RDMFileDictionaryDecoder()
{}

bool RDMFileDictionaryDecoder::loadEnumTypeDef( const RFA_String& enumTypeDef, RDMEnumDict *pEnumDict )
{
	if (RDMDict::Trace & 0x1)
		cout << "File loadEnumTypeDef" << endl;

	fstream	file;
	char	read_buffer[1024];

	// open file for reading
	file.open( enumTypeDef.c_str(), ios::in);

	if(!file.is_open()){
	    cout<<"Failed opening "<<enumTypeDef.c_str()<<endl;
		return false;
	}

	bool		newEnumExpected  = true;
	RDMEnumDef *pEnumDef = 0;
	
	string s, fidName, expandedValue;

	// Read line by line
	while( file.getline( read_buffer, sizeof read_buffer - 1 ) )
	{
		if( read_buffer[0] == '!' ) // Skip comments
			continue;
		
		if( read_buffer[0] ==  0) // Skip blank lines
			continue;

		istrstream ss( read_buffer );

		if( read_buffer[0] != ' ' )
		{
			if( newEnumExpected )
			{
				if( pEnumDef ){
					pEnumDict->insertEnumDef( pEnumDef );
					if (RDMDict::Trace & 0x10)
						pEnumDef->dumpEnumDef();
				}
				
				pEnumDef = new RDMEnumDef;

				newEnumExpected = false;
			}

			// Skip the fid name
			ss >> fidName;

			// get the fid id
			Int16	fid;
			ss >> fid;
			pEnumDef->addFid( fid );
		}
		else
		{
			newEnumExpected = true;
			UInt16 enumVal;

			// Get enum val
			ss >> enumVal;

			s.assign(read_buffer);

			// Get enum string
			string::size_type index1 = s.find_first_of( "\"#", ss.tellg() );

			if (s[index1]=='#')
			{
				string::size_type index2 = s.find_first_of( "\"#", index1 + 1 );
				expandedValue.clear();
				unsigned int start = (unsigned int) (index1 + 1);
				unsigned int length = (unsigned int) (index2 - index1 - 1);
				for (unsigned int i = 0; i < length; i += 2)
				{
					string tmp = s.substr(start + i, 2);
					int hexchar = strtol(tmp.c_str(), (char**) 0, 16);
					expandedValue += (char) hexchar;
				}
			}
			else
			{
				string::size_type index2 = s.find_first_of( "\"", index1 + 1 );
				expandedValue = s.substr( index1 + 1, index2 - index1 - 1 );
			}
			pEnumDef->insertEnumVal( enumVal, expandedValue );
		}
	}

	if( pEnumDef )
	{
		pEnumDict->insertEnumDef( pEnumDef );
		if (RDMDict::Trace & 0x10)
			pEnumDef->dumpEnumDef();
	}
	file.close();
	return true;
}


void RDMFileDictionaryDecoder::associate()
{
	_dictionary.fixRipple();
	_dictionary.associateEnumDict();
}

// Read appendinx_a file
bool RDMFileDictionaryDecoder::loadAppendix_A( const RFA_String& appendix_a )
{
	if (RDMDict::Trace & 0x1)
		cout << "File loadAppendix_A" << endl;

	fstream	file;
	char	read_buffer[1024];

    file.open( appendix_a.c_str(), ios::in);
	if(!file.is_open())
	{
        cout<<"Failed opening "<<appendix_a.c_str()<<endl;
        return false;
    }

	string s;
	string::size_type index1, index2, dataTypeIndex1, dataTypeIndex2;

	// Read line by line
	while( file.getline( read_buffer, sizeof read_buffer - 1 ))
	{
		if( read_buffer[0] == '!' ) // Skip comments
			continue;

		if( read_buffer[0] ==  0) // Skip blank lines
			continue;

		// Start with a fresh Field Def
		RDMFieldDef *pFieldDef = new RDMFieldDef();

		s.assign(read_buffer);
		istrstream ss( read_buffer );


		// Parse name 1st
		index1 = s.find_first_not_of(" ");
		index2 = s.find_first_of( " ", index1 + 1 );
		pFieldDef->setName( s.substr( index1, index2 - index1 ) );

		// Parse display name
		index1 = s.find_first_of( "\"", index2 + 1 );
		index2 = s.find_first_of( "\"", index1 + 1 );
		pFieldDef->setDisplayName( s.substr( index1 + 1, index2 - index1 - 1 ) );

		// Get the Field Id, used as key
		index1 = s.find_first_not_of( " ", index2 + 1 );
		int fieldId;
		ss.seekg( index1 ); //, ios::beg);
		ss >> fieldId;
		pFieldDef->setFieldId(fieldId);

		// Get Ripple To Field
		index1 = s.find_first_not_of(" ", ss.tellg() );
		index2 = s.find_first_of( " ", index1 + 1 );
		pFieldDef->setRipplesToFieldName( s.substr( index1, index2 - index1 ) );

		// Get MF Field Type
		index1 = s.find_first_not_of(" ", index2 + 1 );
		index2 = s.find_first_of( " ", index1 + 1 );
		pFieldDef->setMFFieldTypeName( s.substr( index1, index2 - index1 ) );

		// Get MF Field Length
        index1 = s.find_first_not_of( " ", index2 + 1 );
        int mfFLen;
        ss.seekg((unsigned int)index1, ios::beg);
        ss >> mfFLen;
		pFieldDef->setMFFieldLength( mfFLen );
		index2 = index1 = ss.tellg(); 

		// Get MF enum length
		index1 = s.find_first_of("(", index1 );
			// "(" not found, MF enum length does not exist for this entry
		if( index1 == string::npos )
			index1 = index2;
		else{
	        index1 = s.find_first_not_of( " ", index1 + 1 );
            int mfELen;
            ss.seekg((unsigned int)index1, ios::beg);
            ss >> mfELen;
			pFieldDef->setMFEnumLength( mfELen );
			index1 = s.find_first_of( ")", ss.tellg() );
        }

		// Get BufferType
		index1 = s.find_first_not_of( " ", index1 + 1 );
		index2 = s.find_first_of( " ", index1 + 1 );
		dataTypeIndex1 = index1;
		dataTypeIndex2 = index2;

		// Get RWF length
        index1 = s.find_first_not_of( " ", index2 + 1 );
        int rwfLen;
        ss.seekg((unsigned int)index1, ios::beg);
        ss >> rwfLen;
        pFieldDef->setMaxFieldLength( rwfLen );

		// setting data type requires knowledge of data type name and length
		// e.g. REAL 5 => REAL32
		pFieldDef->setDataType( s.substr( dataTypeIndex1, dataTypeIndex2 - dataTypeIndex1 ), rwfLen );

		// Skip the rest since it's not used
		_dictionary.putFieldDef( pFieldDef );
		if (RDMDict::Trace & 0x10)
			pFieldDef->dumpFieldDef( );
	}

	file.close();
	return true;
}

bool RDMFileDictionaryDecoder::load( const RFA_String& appendix_a, 
									 const RFA_String& enumTypeDef )
{
	//check to see if valid strings
	if( enumTypeDef.empty() || appendix_a.empty())
	        return false;

	// load appendix_a and enumTypeDef
	if(!loadAppendix_A( appendix_a ) || !loadEnumTypeDef( enumTypeDef, _enumDict ))
		return false;

	associate();
	return true;
}
////////////////////////////////////////////////////////////////////
/// End RDMFileDictionaryDecoder
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
/// Begin RDMNetworkDictionaryDecoder
////////////////////////////////////////////////////////////////////
RDMNetworkDictionaryDecoder::RDMNetworkDictionaryDecoder(RDMFieldDict & dictionary, UInt32 dictVerbosityType)
:	_dictionary(dictionary),
	_defId(0),
	_def(0),
	_defTemp(0),
	_enumDict(0),
	pEnumDef(0),
	appendixALoaded(false),
	enumTypeDefLoaded(false),
	_first(true),
	_dictVerbosityType(dictVerbosityType)
{
	_enumDict = &dictionary.enumDict();
}

RDMNetworkDictionaryDecoder::~RDMNetworkDictionaryDecoder()
{
}

UInt32	RDMNetworkDictionaryDecoder::getDictVerbosityType()
{ return _dictVerbosityType;}


void RDMNetworkDictionaryDecoder::loadAppendix_A(const rfa::common::Data &data, bool moreFragments)
{
	if (RDMDict::Trace & 0x1)
		cout << "Network loadAppendix_A " << (moreFragments ? "" : "last") << endl;

	// start the decoding process with recursion level of 0.
	decodeFieldDefDictionary(data);
	appendixALoaded = !moreFragments;
	if (appendixALoaded)
		_dictionary.fixRipple();
    if (appendixALoaded && enumTypeDefLoaded)
		associate();
}

void RDMNetworkDictionaryDecoder::loadEnumTypeDef(const rfa::common::Data &data, bool moreFragments)
{
	if (RDMDict::Trace & 0x1)
		cout << "Network loadEnumTypeDef " << (moreFragments ? "" : "last") << endl;

	// start the decoding process with recursion level of 0.
	decodeEnumDefTables(data);
	enumTypeDefLoaded = !moreFragments;
	if (appendixALoaded && enumTypeDefLoaded)
		associate();
}

void RDMNetworkDictionaryDecoder::associate()
{
	_dictionary.associateEnumDict();
	
	// _enumDict is now owned by the RDMFieldDict superclass, do not hold on to a reference
	_enumDict = 0; 

	// all series are completed.  No need to keep the _defTemp around.
	delete _defTemp;
}

void RDMNetworkDictionaryDecoder::decodeFieldDefDictionary(const Data& data, int recursionLevel)//, ConcreteDecFunction pFunc)
{
	ElementListReadIterator elReadIter;

	const Series& series = static_cast<const Series&>(data);

	if (series.getIndicationMask() & Series::SummaryDataFlag)
	{
		// summary data should be a elementlist
		decodeFieldDefDictionarySummaryData(static_cast<const ElementList&>(series.getSummaryData()), elReadIter);
	}

	if(_dictVerbosityType != rfa::rdm::DICTIONARY_INFO) // INFO only has summary data
	{
		SeriesReadIterator it;
		it.start(series);
		while(!it.off())
		{
			//decode Dictionary Row  - each row should be an elementList
			decodeFieldDefDictionaryRow(static_cast<const ElementList&> (it.value().getData()), elReadIter);
			it.forth();
		}
	}
}



void RDMNetworkDictionaryDecoder::decodeFieldDefDictionarySummaryData(const ElementList& elementList, ElementListReadIterator& it)
{
	it.start(elementList);

	while(!it.off())
	{
		const ElementEntry& element = it.value();

		// The "Type" element is assumed to be DICTIONARY_FIELD_DEFINITIONS (1) - See RDM.h
		if (element.getName() == "DictionaryId") 
			_dictionary.setDictId(static_cast<const DataBuffer&>(element.getData()).getInt32());
		else if (element.getName() == "Version")
			_dictionary.setVersion(static_cast<const DataBuffer&>(element.getData()).getAsString().c_str());

		it.forth();
	}
	if (RDMDict::Trace & 0x2)
	{
		cout << "type: " << _dictionary.getDictType() << " dictId: " << _dictionary.getDictId() 
			<< " ver: " << _dictionary.getVersion() << endl;
	}
}

void RDMNetworkDictionaryDecoder::decodeFieldDefDictionaryRow(const ElementList& elementList, ElementListReadIterator& it)
{
	RDMFieldDef* pFieldDef = new RDMFieldDef();
	
	bool success;
	if (_first)
	{
		_first = false;
		success = decodeFirstFieldDef(elementList, it, pFieldDef);
	}
	else
	{
		success = decodeOtherFieldDef(elementList, it, pFieldDef);
	}
	if (!success)
	{
		cout << "Dictionary not encoded correctly" << endl;
		delete pFieldDef;
		return;
	}
	// if off we should have a complete fieldDef to put into the dictionary
	_dictionary.putFieldDef(pFieldDef);
	if (RDMDict::Trace & 0x20)
		pFieldDef->dumpFieldDef();
}

bool RDMNetworkDictionaryDecoder::decodeFirstFieldDef(const ElementList& elementList, ElementListReadIterator& it, RDMFieldDef * pFieldDef)
{
	it.start(elementList);

	// will assume the minimal set of of elements defined by DICTIONARY_MINUMAL ('NAME', 'TYPE', 'FID', 'LENGTH', 'RWFTYPE',
	// 'RWFLEN', 'RIPPLETO') but also check for the other elements depending on the dictionary mask

	const ElementEntry& element = it.value();
	if (element.getName() == "NAME") 
		pFieldDef->setName(static_cast<const DataBuffer&>(element.getData()).getAsString().c_str());
	else
		return false;

	it.forth();
	const ElementEntry& element3 = it.value();
	if (element3.getName() == "FID")
		pFieldDef->setFieldId((Int16)(static_cast<const DataBuffer&>(element3.getData())).getInt32());
	else
		return false;

	it.forth();
	const ElementEntry& element4 = it.value();
	if (element4.getName() == "RIPPLETO")
		pFieldDef->setRipplesToFid(static_cast<const DataBuffer&>(element4.getData()).getInt32());
	else
		return false;

	it.forth();
	const ElementEntry& element5 = it.value();
	if (element5.getName() == "TYPE")
		pFieldDef->setMFFieldType(static_cast<const DataBuffer&>(element5.getData()).getInt32());
	else
		return false;

	it.forth();
	const ElementEntry& element6 = it.value();
	if (element6.getName() == "LENGTH")
		pFieldDef->setMFFieldLength(static_cast<const DataBuffer&>(element6.getData()).getUInt32());
	else
		return false;
	
	it.forth();
	const ElementEntry& element8 = it.value();
	if (element8.getName() == "RWFTYPE")
		pFieldDef->setDataType(static_cast<UInt8>(static_cast<const DataBuffer&>(element8.getData()).getUInt32()));  // explicit cast to UInt8 for DataType
	else
		return false;
	
	it.forth();
	const ElementEntry& element9 = it.value();
	if (element9.getName() == "RWFLEN")
		pFieldDef->setMaxFieldLength(static_cast<const DataBuffer&>(element9.getData()).getUInt32());
	else
		return false;

	if(_dictVerbosityType == rfa::rdm::DICTIONARY_NORMAL || _dictVerbosityType == rfa::rdm::DICTIONARY_VERBOSE) // DICTIONARY_NORMAL contains these elements
	{
		it.forth();
		const ElementEntry& element7 = it.value();
		if (element7.getName() == "ENUMLENGTH")
			pFieldDef->setMFEnumLength(static_cast<const DataBuffer&>(element7.getData()).getUInt32());
		else
			return false;

		it.forth();
		const ElementEntry& element2 = it.value();
		if (element2.getName() ==	"LONGNAME")
			pFieldDef->setDisplayName(static_cast<const DataBuffer&>(element2.getData()).getAsString().c_str());
		else
			return false;
	}

	it.forth();
	return it.off();
}

bool RDMNetworkDictionaryDecoder::decodeOtherFieldDef(const ElementList& elementList, ElementListReadIterator& it, RDMFieldDef * pFieldDef)
{
	it.start(elementList);

    pFieldDef->setName(static_cast<const DataBuffer&>(it.value().getData()).getAsString().c_str());

	it.forth();
	pFieldDef->setFieldId((Int16)(static_cast<const DataBuffer&>(it.value().getData())).getInt32());

	it.forth();
	pFieldDef->setRipplesToFid(static_cast<const DataBuffer&>(it.value().getData()).getInt32());

	it.forth();
	pFieldDef->setMFFieldType(static_cast<const DataBuffer&>(it.value().getData()).getInt32());

	it.forth();
	pFieldDef->setMFFieldLength(static_cast<const DataBuffer&>(it.value().getData()).getUInt32());
	
	it.forth(); 
	pFieldDef->setDataType(static_cast<UInt8>(static_cast<const DataBuffer&>(it.value().getData()).getUInt32()));  // explicit cast to UInt8 for DataType 
	
	it.forth();
	pFieldDef->setMaxFieldLength(static_cast<const DataBuffer&>(it.value().getData()).getUInt32());

	if(_dictVerbosityType == rfa::rdm::DICTIONARY_NORMAL || _dictVerbosityType == rfa::rdm::DICTIONARY_VERBOSE) // DICTIONARY_NORMAL contains these elements
	{
		it.forth();
		pFieldDef->setMFEnumLength(static_cast<const DataBuffer&>(it.value().getData()).getUInt32());

		it.forth();
        pFieldDef->setDisplayName(static_cast<const DataBuffer&>(it.value().getData()).getAsString().c_str());
	}

	it.forth();

	return it.off();
}




void RDMNetworkDictionaryDecoder::decodeEnumDefTables(const Data& data, int recursionLevel)//, ConcreteDecFunction pFunc)
{
	ElementListReadIterator elReadIter1;
	ElementListReadIterator elReadIter2;

	const Series& series = static_cast<const Series&>(data);

	if (series.getIndicationMask() & Series::SummaryDataFlag)
	{
		decodeEnumDefTablesSummaryData(static_cast<const ElementList&>(series.getSummaryData()), elReadIter1);
	}

	SeriesReadIterator it;
	it.start(series);
	int i = 0;
	while(!it.off())
	{
		//cout << "decoding table " << i++ << endl;
		//decode Enum table  - each row should be an elementList
		decodeEnumDefTable(static_cast<const ElementList&> (it.value().getData()), elReadIter1, elReadIter2);
		it.forth();
	}
}

void RDMNetworkDictionaryDecoder::decodeEnumDefTablesSummaryData(const ElementList& elementList, ElementListReadIterator& it)
{
	it.start(elementList);

	while(!it.off())
	{
		const ElementEntry& element = it.value();

		// The "Type" element is assumed to be DICTIONARY_ENUM_TABLES (2) - See RDM.h
		if (element.getName() == "DictionaryId") 
			_enumDict->setDictId(static_cast<const DataBuffer&>(element.getData()).getInt32());
		else if (element.getName() == "Version")
			_enumDict->setVersion(static_cast<const DataBuffer&>(element.getData()).getAsString().c_str());

		it.forth();
	}
	if (RDMDict::Trace & 0x2)
	{
		cout << "type: " << _enumDict->getDictType() << " dictId: " << _enumDict->getDictId() 
			<< " ver: " << _enumDict->getVersion() << endl;
	}
}

void RDMNetworkDictionaryDecoder::decodeEnumDefTable(const ElementList& elementList, ElementListReadIterator& elIt1, ElementListReadIterator& elIt2)
{
	// create two iterators so we can do depth traversal on the 2nd 
	// and 3rd elementEntries at the same time.

	RDMEnumDef* enumDef = new RDMEnumDef();
	elIt1.start(elementList);
	elIt2.start(elementList);

	// The first element in the elementList is an Array that contains all of the fids.
	// Decode and add all of the fids to the RDMEnumDef
	const ElementEntry& fidsElement = elIt1.value();
	const Array& fidArray = static_cast<const Array&> (fidsElement.getData());
	ArrayReadIterator fidArrIt;
	fidArrIt.start(fidArray);
	while(!fidArrIt.off())
	{
		const DataBuffer& fidBuffer = static_cast<const DataBuffer&> (fidArrIt.value().getData());
		enumDef->addFid(static_cast<Int16> (fidBuffer.getInt32()));
		fidArrIt.forth();
    }

	// next get the second element("VALUES") and the third element("DISPLAYS")
	//	and decode the contained Arrays at the same time, inserting them into the RDMEnumDef
	//  Value-Display are always 1-to-1 (ie. the contained Arrays always have the same count
	elIt1.forth();
	const ElementEntry& valuesElement = elIt1.value();
	const Array& valueArray = static_cast<const Array&> (valuesElement.getData());

	ArrayReadIterator valArrIt;
	valArrIt.start(valueArray);

	elIt2.forth();
	elIt2.forth();
	const ElementEntry& displaysElement = elIt2.value();
	const Array& displayArray = static_cast<const Array&> (displaysElement.getData());
	ArrayReadIterator disArrIt;
	disArrIt.start(displayArray);

	while(!valArrIt.off() && !disArrIt.off())
	{
		const DataBuffer& valueBuffer = static_cast<const DataBuffer&> (valArrIt.value().getData());
		const DataBuffer& displayBuffer = static_cast<const DataBuffer&> (disArrIt.value().getData());

		const char * display = displayBuffer.getAsString().c_str();

		UInt16 enumeration = valueBuffer.getEnumeration();
		enumDef->insertEnumVal(enumeration, display );

		valArrIt.forth();
		disArrIt.forth();
	}
	_enumDict->insertEnumDef(enumDef);
	if (RDMDict::Trace & 0x10)
		enumDef->dumpEnumDef();
}
////////////////////////////////////////////////////////////////////
/// End RDMNetworkDictionaryDecoder
////////////////////////////////////////////////////////////////////

