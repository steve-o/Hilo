#ifndef _MARKETPRICE_STREAMITEM_H
#define _MARKETPRICE_STREAMITEM_H


#include "InstrumentStreamItem.h"
#include "MarketPriceGenerator.h"


namespace rfa {
	namespace data {
		class FieldList;
		class FieldListWriteIterator;
	}
}

class MarketPriceStreamItem : public InstrumentStreamItem
{
public:
	MarketPriceStreamItem( const rfa::message::AttribInfo& attribInfo,
							DataGenerator& dataGenerator, 
							rfa::common::UInt16 fieldListID,
							rfa::common::Int16 dictID,
							rfa::common::QualityOfService& rQoS );
	virtual ~MarketPriceStreamItem() {}

	virtual void createStatus( rfa::common::RespStatus& status );
	virtual void createData( rfa::common::Data& data, rfa::message::RespMsg::RespType respType, rfa::common::UInt32 maxSize = 800000 );
	virtual void continueData( rfa::common::Data & data, rfa::common::UInt32 maxSize = 800000 );

	virtual void generateRespMsg( rfa::message::RespMsg::RespType respType, 
								 RespMsg* pRespMsg, 
								 rfa::common::RespStatus* pRespStatus );

protected:
	void encodeMarketPriceFieldList( rfa::data::FieldList* pFieldList,
									rfa::data::FieldListWriteIterator* fieldListWIt,
									rfa::message::RespMsg::RespType respType);

private:
	rfa::common::UInt16 _fieldListID;
	rfa::common::Int16 _dictID;
	rfa::common::Int32 _priceVal;
};

#endif
