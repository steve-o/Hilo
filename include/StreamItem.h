#ifndef _STREAMITEM_H
#define _STREAMITEM_H

#pragma warning( disable : 4512 )
#include <list>

#include "Common/Common.h"
#include "Message/RespMsg.h"

namespace rfa {
	namespace common {
		class RespStatus;
		class Data;
	}
	namespace message {
		class AttribInfo;
	}
	namespace sessionLayer {
		class RequestToken;
	}
}

class DataGenerator;
class DomainManager;

typedef std::list<rfa::sessionLayer::RequestToken*>	TOKENS;

class StreamItem
{
public:

	StreamItem(const rfa::message::AttribInfo& attribInfo, DataGenerator & dataGenerator);
	virtual ~StreamItem();

	virtual bool satisfies(const rfa::message::AttribInfo & attribInfo) const = 0;
	virtual void createStatus(rfa::common::RespStatus & status) = 0;
	virtual void createData(rfa::common::Data & data, rfa::message::RespMsg::RespType respType, rfa::common::UInt32 maxSize = 800000) = 0;
	virtual void continueData(rfa::common::Data & data, rfa::common::UInt32 maxSize = 800000) = 0;
	virtual void generateRespMsg(rfa::message::RespMsg::RespType respType, rfa::message::RespMsg* pRespMsg, rfa::common::RespStatus* pRespStatus);
	inline bool isComplete() const { return _isComplete; }

	void addRequestToken(rfa::sessionLayer::RequestToken & token);
	void removeRequestToken(rfa::sessionLayer::RequestToken & token);
	int	 getNumOfRequestTokens( void ) const;
	const TOKENS* getRequestTokens() const;

protected:
	virtual void	cleanup();


	TOKENS			 _tokens;
	bool			_isComplete;
	DataGenerator & _dataGenerator;
};

#endif

