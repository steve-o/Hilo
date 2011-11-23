#ifndef _MARKET_PRICE_GENERATOR_H
#define _MARKET_PRICE_GENERATOR_H

#include "DataGenerator.h"
#include "Common/RFA_String.h"

class MarketPriceGenerator : public DataGenerator
{
public:
	MarketPriceGenerator( ApplicationManager & appManager );
	virtual ~MarketPriceGenerator();
	
	virtual bool isValidRequest( const rfa::message::AttribInfo& attrib );
	virtual void removeStreamItem( StreamItem & streamItem );

protected:
	virtual StreamItem* createStreamItem( const rfa::message::AttribInfo& attrib );
	static rfa::common::RFA_String _domainName;
	
};

#endif
