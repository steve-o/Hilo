#ifndef _RFA_EXAMPLE_RDMDictionaryEncoder_h
#define _RFA_EXAMPLE_RDMDictionaryEncoder_h

#include "RDMDict.h"
#include "Data/SeriesWriteIterator.h"
#include "Data/ElementList.h"
#include "Data/ElementListDef.h"

class RDMDictionaryEncoder
{
public:

	enum OptimizeEnum
	{
		Speed = 0,
		Size = 1,
		Fragment = 2
	};
	
	RDMDictionaryEncoder();
	virtual ~RDMDictionaryEncoder();

	void encodeFieldDictionary(Series & series, const RDMFieldDict & dictionary,
		UInt32 verbosity, OptimizeEnum optimize = Size, int maxFragmentSize = 200000);
	void encodeEnumDictionary(Series & series, const RDMEnumDict & dictionary, 
		UInt32 verbosity, OptimizeEnum optimize = Size, int maxFragmentSize = 200000);
	
	void continueEncoding(Series & series);

	bool isComplete() const { return _isComplete; }

protected:

	const RDMFieldDict * _fieldDict;
	const RDMEnumDict * _enumDict;

	// state used for fragmentation
	ElementList * _elementList;
	ElementListDef * _elementListDef;
	EnumTables::const_iterator _enumIter;
	bool _isComplete;
	UInt32 _verbosity;
	UInt32 _type;
	Int16 _fieldId;
	UInt16 _tableIndex;

	int _maxFragmentSize;
	int _maxSeriesEntrySize;
	OptimizeEnum _optimize;

	void encodeFieldDictionary(const RDMFieldDict & dictionary, UInt32 verbosity, Series & series);
	void encodeFieldDictionaryDataDef(DataBuffer::DataBufferEncodedEnumeration flen,
			UInt32 verbosity, ElementListDef & eld, Series & series);
	void encodeFieldDictionaryDataDef(const ElementListDef & eld, Series & series);
	void encodeFieldDictionarySummaryData(const RDMFieldDict & dictionary, Series & series);
	void encodeFieldDictionaryFields(Int16 startFid,
			SeriesWriteIterator & switer, 
			SeriesEntry & sentry, 
			const RDMFieldDict & dictionary, 
			UInt32 verbosity,
			ElementList & elementList, 
			const ElementListDef & eld, 
			Series & series);
	void encodeFieldDictionaryFidDef(DataBuffer::DataBufferEncodedEnumeration flen,
			UInt32 verbosity, const ElementListDef & eld, 
			const RDMFieldDef *, ElementList & fidDef);

	void encodeEnumDictionary(const RDMEnumDict & dictionary, UInt32 verbosity, Series & series);
	void encodeEnumDictionaryDataDef(ElementListDef & eld, Series & series);
	void encodeEnumDictionaryDataDef(const ElementListDef & eld, Series & series);
	void encodeEnumDictionaryTables(EnumTables::const_iterator & iter, 
			SeriesWriteIterator & switer, 
			SeriesEntry & sentry, 
			const RDMEnumDict & dictionary,
			ElementList & elementList, 
			const ElementListDef & eld,
			Series & series);
	void encodeEnumDictionaryEnumTable(const EnumTables::const_iterator & iter, 
			const ElementListDef & eld,	ElementList & enumTable);

	void encodeDictionarySummaryData(const RDMDict & dictionary, 
		UInt32 type, Series & series, ElementList & list);


private:

};

#endif
