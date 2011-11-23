#include "MarketPriceGenerator.h"
#include "MarketPriceStreamItem.h"
#include "ApplicationManager.h"
#include "ConfigManager.h"
#include "Config/ConfigTree.h"
#include "Config/ConfigStringList.h"
#include <assert.h>

using namespace rfa::common;
using namespace rfa::config;
using namespace rfa::message;

RFA_String MarketPriceGenerator::_domainName = RFA_String( "MarketPrice", 11, false );

MarketPriceGenerator::MarketPriceGenerator( ApplicationManager & appManager )
: DataGenerator( appManager )
{
    _pConfigTree = const_cast<ConfigTree*>(appManager.getAppConfigManager()->getTree(_domainName));
	assert( _pConfigTree );
}

MarketPriceGenerator::~MarketPriceGenerator()
{
}

StreamItem* MarketPriceGenerator::createStreamItem( const AttribInfo& attrib )
{
	const ConfigTree* pSrcConfigTree = _pConfigTree->getChildAsTree( attrib.getServiceName() );
	if ( !pSrcConfigTree )
		return 0;

	if ( !isValidRequest( attrib ) )
		return 0;

	RFA_String configItem;
	configItem.set( "FieldListID", 11, false );
	UInt16 FieldListID = (UInt16) pSrcConfigTree->getChildAsLong( configItem, 1 );
	configItem.set( "DictID", 6, false );
	Int16 DictID = (Int16) pSrcConfigTree->getChildAsLong( configItem, 1 );

	// setup QoS for this stream item
	QualityOfService	QoS;
	configItem.set( "Services", 8, false );

    const ConfigTree* pServicesConfigTree = const_cast<ConfigTree*>( appManager.getAppConfigManager()->getTree( configItem ) );

	if( pServicesConfigTree )
	{
		const ConfigTree* pTmpTree = pServicesConfigTree->getChildAsTree( attrib.getServiceName() );

		if ( pTmpTree )
		{
			configItem.set( "QoS", 3, false );
			const ConfigTree* pQoSConfigTree = pTmpTree->getChildAsTree( configItem );

			if ( pQoSConfigTree )
			{
				configItem.set( "Rate", 4, false );
				QoS.setRate( pQoSConfigTree->getChildAsLong( configItem ) );
				configItem.set( "Timeliness", 10, false );
				QoS.setTimeliness( pQoSConfigTree->getChildAsLong( configItem ) );
			}
		}
	}

	return new MarketPriceStreamItem( attrib, *this, FieldListID, DictID, QoS );
}

void MarketPriceGenerator::removeStreamItem( StreamItem & streamItem )
{
	assert( streamItem.getNumOfRequestTokens() == 0 );
	delete &streamItem;
}

bool MarketPriceGenerator::isValidRequest( const AttribInfo& attrib )
{
	const RFA_String& serviceName = attrib.getServiceName();
	const RFA_String& itemName = attrib.getName();

	if (!_pConfigTree)
		return false;

	const ConfigTree* pSrcConfigTree = _pConfigTree->getChildAsTree( serviceName );
	if ( !pSrcConfigTree )
		return false;
	else
	{
		const StringList *pStringList = pSrcConfigTree->getChildAsStringList( "ItemList", "", "," );
		if ( !pStringList || pStringList->size() == 0 ) {
			if ( pStringList ) delete pStringList;
			return true;
		}
		for ( unsigned int i = 0; i < pStringList->size(); i ++ )
		{
			const RFA_String& tmpItemName = (*pStringList)[i];
			if ( itemName == tmpItemName ) {
				delete pStringList;
				return true;
			}
		}

		delete pStringList;
		return false;
	}
}
