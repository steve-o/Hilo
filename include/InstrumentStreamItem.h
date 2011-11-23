#ifndef _INSTRUMENT_STREAMITEM_H
#define _INSTRUMENT_STREAMITEM_H

#include "StreamItem.h"
#include "Common/RFA_String.h"

#include "vpf/vpf.h"
using namespace vpf;

#include "VelocityProvider.h"

class InstrumentStreamItem : public StreamItem
{
public:
	InstrumentStreamItem(const rfa::message::AttribInfo& attribInfo, DataGenerator & dataGenerator, rfa::common::QualityOfService * pQoS =0);
	virtual ~InstrumentStreamItem() {}

	virtual bool satisfies(const rfa::message::AttribInfo & attribInfo) const;

	void setData(RFA_String& data, int defID)
	{
		_data = data;
		_defID = defID;
	}


	inline const rfa::common::RFA_String& getServiceName() const { return _serviceName; }
	inline const rfa::common::RFA_String& getItemName() const { return _itemName; }
	inline const rfa::common::QualityOfService& getQoS() const { return _QoS; }
protected:

	rfa::common::RFA_String		_itemName;
	rfa::common::RFA_String		_serviceName;
	rfa::common::QualityOfService _QoS;
	vpf::Event *_pfEvent;

	RFA_String _data;
	int _defID;
    //FlexRecData *frData;
	//FlexRecBaseTickData *frBase;

};

#endif
