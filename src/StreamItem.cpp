#include "StreamItem.h"

//#include "DomainManager.h"
#include "DataGenerator.h"
#include "Common/RespStatus.h"

StreamItem::StreamItem(const rfa::message::AttribInfo& attribInfo, DataGenerator & dataGenerator)
			: _isComplete(false), _dataGenerator(dataGenerator)
{
}

StreamItem::~StreamItem()
{
	cleanup();
}

void StreamItem::cleanup()
{
	
}

void StreamItem::addRequestToken(rfa::sessionLayer::RequestToken & token)
{
	_tokens.push_back(&token);
}



void StreamItem::removeRequestToken(rfa::sessionLayer::RequestToken & token)
{
	_tokens.remove(&token);
}

void StreamItem::generateRespMsg(rfa::message::RespMsg::RespType respType, 
								 rfa::message::RespMsg* pRespMsg, 
								 rfa::common::RespStatus* pRespStatus)
{

}

int	 StreamItem::getNumOfRequestTokens( void ) const
{
	return _tokens.size();
}	

const TOKENS* StreamItem::getRequestTokens() const
{
	return &_tokens;
}

