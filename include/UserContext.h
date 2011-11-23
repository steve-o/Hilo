#ifndef _USERCONTEXT_H
#define _USERCONTEXT_H


#include "StreamItem.h"
#include "Common/RFA_String.h"

namespace rfa {
	namespace data {
		class ElementList;
	}
}

class UserContext : public StreamItem
{
public:
	UserContext(const rfa::message::AttribInfo& attribInfo, DataGenerator & dataGenerator);
	virtual ~UserContext() {}

	virtual bool satisfies(const rfa::message::AttribInfo & attribInfo) const;
	virtual void createStatus(rfa::common::RespStatus & status);
	virtual void createData(rfa::common::Data & data, rfa::message::RespMsg::RespType respType, rfa::common::UInt32 maxSize = 800000);
	virtual void continueData(rfa::common::Data & data, rfa::common::UInt32 maxSize = 800000);

protected:
	void saveAttrib(const rfa::data::ElementList & elementList);

	rfa::common::RFA_String _userName;
	rfa::common::RFA_String _applicationId;
	rfa::common::RFA_String _position;
};

#endif
