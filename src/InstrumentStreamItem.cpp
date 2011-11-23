#include "InstrumentStreamItem.h"


InstrumentStreamItem::InstrumentStreamItem(const rfa::message::AttribInfo& attribInfo, DataGenerator & pDataGen, rfa::common::QualityOfService * pQoS)
:StreamItem(attribInfo,pDataGen),_itemName(attribInfo.getName()),_serviceName(attribInfo.getServiceName())
{
        _pfEvent =0;

	if(pQoS)
		_QoS=*pQoS;
}


bool InstrumentStreamItem::satisfies(const rfa::message::AttribInfo & attribInfo) const
{
	return ( _itemName == attribInfo.getName()&&
			 _serviceName == attribInfo.getServiceName() )? true : false;
}

