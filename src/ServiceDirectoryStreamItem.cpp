#include "ServiceDirectoryStreamItem.h"
#include "ServiceDirectory.h"

#include <assert.h>
#include "RDM/RDM.h"
#include "Common/RespStatus.h"
#include "Message/AttribInfo.h"
#include "Data/Map.h"
#include "Data/MapEntry.h"
#include "Data/MapWriteIterator.h"
#include "Data/FilterList.h"
#include "Data/FilterEntry.h"
#include "Data/FilterListWriteIterator.h"
#include "Data/ElementList.h"
#include "Data/ElementEntry.h"
#include "Data/ElementListWriteIterator.h"
#include "Data/Array.h"
#include "Data/ArrayEntry.h"
#include "Data/ArrayWriteIterator.h"

#include "Config/ConfigTree.h"
#include "Config/ConfigNode.h"
#include "Config/ConfigNodeIterator.h"
#include "RDM/RDM.h"

using namespace rfa::config;
using namespace rfa::common;
using namespace rfa::message;
using namespace rfa::data;


ServiceDirectoryStreamItem::ServiceDirectoryStreamItem(const AttribInfo& attribInfo, DataGenerator & pDataGen)
							:StreamItem(attribInfo,pDataGen),_isStale(false),_FldDictNameUsed("RWFFld"), _EnumDictNameUsed("RWFEnum")
{
	if ( attribInfo.getHintMask() & AttribInfo::ServiceNameFlag )
	{
		_serviceName = attribInfo.getServiceName();
	}
	_pConfigTree = const_cast<ConfigTree*>((static_cast<ServiceDirectory&>(_dataGenerator)).getServiceConfigTree(_serviceName));
	assert ( _pConfigTree );

	_requestedDataMask =  (UInt8) attribInfo.getDataMask();
}

ServiceDirectoryStreamItem::~ServiceDirectoryStreamItem()
{
	
}

bool ServiceDirectoryStreamItem::satisfies(const AttribInfo & attribInfo) const
{
	RFA_String reqServiceName;
	if ( attribInfo.getHintMask() & AttribInfo::ServiceNameFlag )
		reqServiceName = attribInfo.getServiceName();
	return ( _serviceName == reqServiceName && _requestedDataMask == attribInfo.getDataMask() )? true : false;
	
}

void ServiceDirectoryStreamItem::createStatus(RespStatus & rStatus)
{
	
	if ( !_isStale )
	{
		_isStale = true;
		// generate a stale status
	}
	else
	{
		_isStale = false;
		// generate a OK status
	}
}

void ServiceDirectoryStreamItem::createData(Data & data, rfa::message::RespMsg::RespType respType, UInt32 maxSize)
{
	encodeDirectoryMap(static_cast<Map*>(&data));
}

void ServiceDirectoryStreamItem::continueData(rfa::common::Data & data, rfa::common::UInt32 maxSize)
{

}


void ServiceDirectoryStreamItem::encodeDirectoryMap( Map* pMap )
{
	assert(pMap);
	MapWriteIterator mapWIt;

	pMap->setIndicationMask(Map::EntriesFlag);
	mapWIt.start(*pMap);
	pMap->setKeyDataType( DataBuffer::StringAsciiEnum);   

	// Provides a hint to the consuming side of how many map entries are to be provided. 
	if ( _serviceName.length() )
	{
		// only one particular service should be published
		pMap->setTotalCountHint(1); 
		encodeDirectoryMapEntry( mapWIt,_serviceName,_pConfigTree );
	}
	else	
	{
		// all services should be published
		pMap->setTotalCountHint(_pConfigTree->getChildAsLong("TotalSerivces")); 
	
		ConfigNodeIterator *pTreeIt = _pConfigTree->createIterator();
		assert( pTreeIt );

		const ConfigNode *pNode = 0;
		for ( pTreeIt->start();!pTreeIt->off();pTreeIt->forth() )
		{
			  pNode = pTreeIt->value();
			  if( pNode->getType() == treeNode )
			  {
				 const ConfigTree* pServiceTree = static_cast<const ConfigTree*>(pNode);
				 encodeDirectoryMapEntry( mapWIt, pServiceTree->getNodename(), pServiceTree );
			  }
		}

		pTreeIt->destroy();
	
	}
	mapWIt.complete();
}

void ServiceDirectoryStreamItem::encodeDirectoryMapEntry( MapWriteIterator& mapWIt,const RFA_String& serviceName,const ConfigTree* pConfigTree )
{

	assert( pConfigTree );
		//********* Begin Encode MapEntry **********
	MapEntry mapEntry;
	mapEntry.setAction(MapEntry::Add);

 	// Set ServiceName as key for MapEntry
	DataBuffer keyDataBuffer(true);
	keyDataBuffer.setFromString(serviceName, DataBuffer::StringAsciiEnum);
	mapEntry.setKeyData(keyDataBuffer);

	FilterList filterList;
	encodeDirectoryFilterList(&filterList, serviceName, pConfigTree);
	mapEntry.setData(static_cast<Data&>(filterList));

	mapWIt.bind(mapEntry);

}


void ServiceDirectoryStreamItem::encodeDirectoryFilterList( FilterList* pFilterList, 
														    const RFA_String & rSvcName, 
															const ConfigTree* pConfigTree)
{
	assert(pFilterList);
	FilterListWriteIterator filterListWIt;
	filterListWIt.start(*pFilterList);  

	pFilterList->setTotalCountHint(2);	// This example specifies only 2 because there is only two filter entries


	//Encode FilterEntry
	FilterEntry filterEntry;
	filterEntry.setFilterId(rfa::rdm::SERVICE_INFO_ID);
	filterEntry.setAction(FilterEntry::Set);		// Set this to Set since this is the full filterEntry

	//Encode ElementList for Service Info
	ElementList elementList;
	encodeDirectoryInfoElementList(&elementList, rSvcName, pConfigTree);
	filterEntry.setData(static_cast<const Data&>(elementList));
	filterListWIt.bind(filterEntry);


	//Encode ElementList for Service State
	filterEntry.clear();
	elementList.clear();
	filterEntry.setFilterId(rfa::rdm::SERVICE_STATE_ID);
	filterEntry.setAction(FilterEntry::Set);   // Set this to Set since this is the full filterEntry
	encodeDirectoryStateElementList(&elementList, rSvcName, pConfigTree);
	filterEntry.setData(static_cast<const Data&>(elementList));
	filterListWIt.bind(filterEntry);

	filterListWIt.complete();
}

void ServiceDirectoryStreamItem::encodeDirectoryInfoElementList(ElementList* pElementList, 
																const RFA_String& rSvcName, 
																const ConfigTree* pConfigTree)
{
	assert(pElementList);
	ElementListWriteIterator elemListWIt;
	elemListWIt.start(*pElementList);

	ElementEntry element;
	DataBuffer dataBuffer; 	
	RFA_String str;
	RFA_String nameValue;

	// Encode Name
	str = "Name";
	element.setName(str);
	nameValue =rSvcName.c_str();
	dataBuffer.setFromString(nameValue,DataBuffer::StringAsciiEnum);
	element.setData( dataBuffer );
	elemListWIt.bind(element);

	// Encode Vendor
	element.clear();
	dataBuffer.clear();
	str = "Vendor";
	element.setName(str);
	nameValue = pConfigTree->getChildAsString(str,"Reuters");
	dataBuffer.setFromString(nameValue,DataBuffer::StringAsciiEnum);
	element.setData( dataBuffer );
	elemListWIt.bind(element);

	// Encode  isSource 
	element.clear();
	dataBuffer.clear();
	str = "IsSource";
	element.setName(str);
	bool configValue = pConfigTree->getChildAsBool(str,true);
	UInt32 isSource = configValue? 1 : 0;
	dataBuffer.setUInt32(isSource);		// Original Publisher  
	element.setData( dataBuffer );
	elemListWIt.bind(element);

	// Encode Capabilities
	element.clear();
	str = "Capabilities";
	element.setName(str);
	Array array, array2;
	encodeDirectoryArray(&array,rSvcName,pConfigTree);
	element.setData(static_cast<const Data&>(array));
	elemListWIt.bind(element);

	// Encode DictionariesUsed
	element.clear();
	str = "DictionariesUsed";
	element.setName(str);
	encodeDirectoryArrayDictUsed(&array2,rSvcName,pConfigTree);
	element.setData(static_cast<const Data&>(array2));
	elemListWIt.bind(element);


	// Encode Quality of Service
	element.clear();
	str = "QoS";
	element.setName(str);
	Array arrayqos;
	encodeDirectoryArrayQoS(&arrayqos,rSvcName,pConfigTree);
	element.setData(static_cast<const Data&>(arrayqos));
	elemListWIt.bind(element);

	elemListWIt.complete();
}

void ServiceDirectoryStreamItem::encodeDirectoryStateElementList(ElementList* pElementList,
																 const RFA_String& rSvcName, 
																 const ConfigTree* pConfigTree)
{
	assert(pElementList);
	ElementListWriteIterator elemListWIt;
	elemListWIt.start(*pElementList);
	
	ElementEntry element;

	//Encode Element ServiceState
	RFA_String str = "ServiceState";
	element.setName(str);

	bool configValue = pConfigTree->getChildAsBool(str,true);
	UInt32 intValue = configValue? 1: 0;
	DataBuffer dataBuffer(true);
	dataBuffer.setUInt32((UInt32)intValue);
	element.setData( dataBuffer );
	elemListWIt.bind(element);

	//Encode Element AcceptingRequests
	element.clear();
	str = "AcceptingRequests";
	element.setName(str);
	configValue = pConfigTree->getChildAsBool(str,true);
	intValue = configValue ? 1 : 0;
	dataBuffer.setUInt32((UInt32)intValue);
	element.setData( dataBuffer );
	elemListWIt.bind(element);

	elemListWIt.complete();
}



void ServiceDirectoryStreamItem::encodeDirectoryArray( Array* pArray,
													  const RFA_String& rSvcName, 
													  const ConfigTree* pConfigTree)
{
	assert(pArray);
	ArrayWriteIterator arrWIt; 
	arrWIt.start(*pArray);

	DataBuffer dataBuffer(true);
	ArrayEntry arrayEntry;
	int mType;

	const rfa::config::StringList *pCapabilityList = pConfigTree->getChildAsStringList("Capabilities","",",");
	if ( !pCapabilityList ) 
	{
		// use default capabilities lise
		// Specify Dictionary as a capability
		mType = rfa::rdm::MMT_DICTIONARY;
		dataBuffer.setUInt32((UInt32)mType);
		arrayEntry.setData(dataBuffer);
		arrWIt.bind(arrayEntry);

		arrayEntry.clear();
		dataBuffer.clear();

		// Specify Market Price as a capability
		mType = rfa::rdm::MMT_MARKET_PRICE;
		dataBuffer.setUInt32((UInt32)mType);
		arrayEntry.setData(dataBuffer);
		arrWIt.bind(arrayEntry);
	}
	else
	{
		for (unsigned int i = 0; i < pCapabilityList->size(); i ++)
		{
			RFA_String capStrValue = ((*pCapabilityList)[i]);
			mType = atoi(capStrValue.c_str());
	
			dataBuffer.clear();
			arrayEntry.clear();
	
			// each stringList entry is a capability the service supports
			dataBuffer.setUInt32((UInt32)mType);
			arrayEntry.setData(dataBuffer);
			arrWIt.bind(arrayEntry);
		}
	}
	arrWIt.complete();
	delete pCapabilityList;
}


void ServiceDirectoryStreamItem::encodeDirectoryArrayDictUsed( Array* pArray,
													  const RFA_String& rSvcName, 
													  const ConfigTree* pConfigTree)
{
	assert(pArray);

	ArrayWriteIterator arrWIt; 
	arrWIt.start(*pArray);

	DataBuffer dataBuffer(true);
	ArrayEntry arrayEntry;

	dataBuffer.setFromString(_FldDictNameUsed, DataBuffer::StringAsciiEnum);
	arrayEntry.setData(dataBuffer);
	arrWIt.bind(arrayEntry);

	arrayEntry.clear();
	dataBuffer.clear();

	dataBuffer.setFromString(_EnumDictNameUsed, DataBuffer::StringAsciiEnum);
	arrayEntry.setData(dataBuffer);
	arrWIt.bind(arrayEntry);
	arrWIt.complete();
}

void ServiceDirectoryStreamItem::encodeDirectoryArrayQoS( Array* pArray,
														 const RFA_String& rSvcName, 
														 const ConfigTree* pConfigTree)
{
	const ConfigTree* pQoSConfigTree = pConfigTree->getChildAsTree("QoS");
	rfa::common::QualityOfService	QoS;
	if ( pQoSConfigTree )
	{
		QoS.setRate(pQoSConfigTree->getChildAsLong("Rate"));
		QoS.setTimeliness(pQoSConfigTree->getChildAsLong("Timeliness"));
	}
	else
	{
		QoS.setRate(QualityOfService::tickByTick);
		QoS.setTimeliness(QualityOfService::realTime);
	}

	assert(pArray);
	ArrayWriteIterator arrWIt; 
	arrWIt.start(*pArray);
					
	DataBuffer dataBuffer(true);
	ArrayEntry arrayEntry;
	rfa::common::QualityOfServiceInfo QoSInfo;  
	QoSInfo.setQualityOfService(QoS);

	dataBuffer.setQualityOfServiceInfo(QoSInfo);
	arrayEntry.setData(dataBuffer);
	arrWIt.bind(arrayEntry);
		
	arrWIt.complete();
}
