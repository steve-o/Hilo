#ifndef _SERVICE_DIRECTORY_STREAMITEM_H
#define _SERVICE_DIRECTORY_STREAMITEM_H

#include "StreamItem.h"
#include "Common/RFA_String.h"
#include "Common/QualityOfService.h"

namespace rfa {

	namespace data{
		class Array;
		class ElementList;
		class FilterList;
		class Map;
		class MapWriteIterator;
	}

	namespace config{
		class ConfigTree;
	}
}

class ServiceDirectoryStreamItem : public StreamItem
{
public:
	ServiceDirectoryStreamItem(const rfa::message::AttribInfo& attribInfo, DataGenerator & dataGenerator);
	virtual ~ServiceDirectoryStreamItem();

	virtual bool satisfies(const rfa::message::AttribInfo & attribInfo) const;
	virtual void createStatus(rfa::common::RespStatus & status);
	virtual void createData(rfa::common::Data & data, rfa::message::RespMsg::RespType respType, rfa::common::UInt32 maxSize = 800000);
	virtual void continueData(rfa::common::Data & data, rfa::common::UInt32 maxSize = 800000);

protected:

	void encodeDirectoryArrayQoS( rfa::data::Array* pArray, const rfa::common::RFA_String& rSvcName, const rfa::config::ConfigTree* pConfigTree);
	void encodeDirectoryArray( rfa::data::Array* pArray, const rfa::common::RFA_String& rSvcName, const rfa::config::ConfigTree* pConfigTree);
	void encodeDirectoryArrayDictUsed( rfa::data::Array* pArray, const rfa::common::RFA_String& rSvcName, const rfa::config::ConfigTree* pConfigTree);
	void encodeDirectoryStateElementList(rfa::data::ElementList* pElementList, const rfa::common::RFA_String& rSvcName, const rfa::config::ConfigTree* pConfigTree);
	void encodeDirectoryInfoElementList(rfa::data::ElementList* pElementList,const rfa::common::RFA_String & rSvcName, const rfa::config::ConfigTree* pConifgTree);
	void encodeDirectoryFilterList( rfa::data::FilterList* pFilterList, const rfa::common::RFA_String& rSvcName,const rfa::config::ConfigTree* pConfigTree);
	void encodeDirectoryMapEntry( rfa::data::MapWriteIterator& mapWIt,const rfa::common::RFA_String& serviceName, const rfa::config::ConfigTree* pConfigTree );
	void encodeDirectoryMap( rfa::data::Map* pMap );

	rfa::common::RFA_String				_serviceName;
	rfa::config::ConfigTree*			_pConfigTree;

	rfa::common::QualityOfService		_reusableQoS;
	rfa::common::RFA_String				_reusableVendorStr;

	bool								_isStale; 
	rfa::common::UInt8					_requestedDataMask;

	rfa::common::RFA_String _FldDictNameUsed;
	rfa::common::RFA_String _EnumDictNameUsed;

};

#endif
