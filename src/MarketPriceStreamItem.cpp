#include "MarketPriceStreamItem.h"
#include "Common/Data.h"
#include "Common/RFA_String.h"
#include "RDM/RDM.h"

#include "Data/FieldList.h"
#include "Data/FieldEntry.h"
#include "Data/FieldListWriteIterator.h"
#include "Data/DataBuffer.h"
#include "assert.h"
#include <time.h>

using namespace rfa::common;
using namespace rfa::message;
using namespace rfa::rdm;
using namespace rfa::data;

#include "VelocityProvider.h"


MarketPriceStreamItem::MarketPriceStreamItem( const AttribInfo& attribInfo, 
											 DataGenerator& pDataGen,
											 UInt16 fieldListID,
											 Int16 dictID,
											 QualityOfService& rQoS )
: InstrumentStreamItem( attribInfo, pDataGen, &rQoS ),
  _fieldListID( fieldListID ),
  _dictID( dictID ),
  _priceVal( 100 )
{
}

void MarketPriceStreamItem::createStatus( RespStatus& status )
{
}

void MarketPriceStreamItem::createData( Data& data, RespMsg::RespType respType, UInt32 maxSize )
{
	encodeMarketPriceFieldList( static_cast<FieldList*>(&data),
								static_cast<FieldListWriteIterator*>(&(_dataGenerator.reusableFieldListWriteIterator())),
								respType);
}

void MarketPriceStreamItem::continueData( Data& data, UInt32 maxSize )
{
}

void MarketPriceStreamItem::generateRespMsg( RespMsg::RespType respType, 
											RespMsg* pRespMsg, 
											RespStatus* pRespStatus )
{
	// set MsgModelType
	pRespMsg->setMsgModelType( MMT_MARKET_PRICE );	
	// Set response Type
	pRespMsg->setRespType( respType );

	if ( respType == RespMsg::RefreshEnum )
	{
		// set IndicationMask
		pRespMsg->setIndicationMask( RespMsg::RefreshCompleteFlag );    
		// set RespTypeNum
		pRespMsg->setRespTypeNum( REFRESH_SOLICITED );   //for solicited refresh, value is 0     	
		
		// QoS	 specification 
		pRespMsg->setQualityOfService( _QoS );		// This uses the quality of service that was specified for the service directory - for the particular service

		// define status for Refresh messages only
		pRespStatus->setDataState( RespStatus::OkEnum );	
		pRespStatus->setStatusCode( RespStatus::NoneEnum );
		RFA_String  tmpStr;
		tmpStr.set( "MessageComplete", 15, false );
		pRespStatus->setStatusText( tmpStr );

		// Set respStatus
		pRespMsg->setRespStatus( *pRespStatus );
	}

	_dataGenerator.reusableFieldList().clear();
	createData( static_cast<Data&>(_dataGenerator.reusableFieldList()), respType );
	pRespMsg->setPayload( _dataGenerator.reusableFieldList() );
}

void MarketPriceStreamItem::encodeMarketPriceFieldList( FieldList* pFieldList, FieldListWriteIterator* pfieldListWIt, RespMsg::RespType respType )
{
	assert( pFieldList );
	assert( pfieldListWIt );

	pFieldList->setInfo( _dictID, _fieldListID );//specify values that are passed through to consumer application.
	pfieldListWIt->start( *pFieldList );

	FieldEntry field;
	DataBuffer dataBuffer( true );
	
	if (!_data.empty() && _defID != 0)
	{
		field.setFieldID(_defID);
		dataBuffer.setFromString(_data, rfa::data::DataBuffer::StringAsciiEnum);
		field.setData(dataBuffer);
		pfieldListWIt->bind(field);
	}

	pfieldListWIt->complete();
}
