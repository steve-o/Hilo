#ifndef _DIRECTORY_MANAGER_H
#define _DIRECTORY_MANAGER_H

#include "DomainManager.h"
#include <list>

namespace rfa {
	namespace message {
		class AttribInfo;
	}

	namespace common {
		class RFA_String;
		class QualityOfService;
		class Data;
		class RespStatus;
	}

	namespace config{
		class ConfigTree;
	}
}

class ServiceDirectory;
class ServiceDirectoryStreamItem;

class DirectoryManager : public DomainManager
{
public:
	DirectoryManager(ApplicationManager & appManager, ServiceDirectory& serviceDirectory);
	virtual ~DirectoryManager();

	virtual void processReqMsg(SessionManager & sessionManager, 
		rfa::sessionLayer::RequestToken & token, const rfa::message::ReqMsg & reqMsg);
	virtual void processCloseReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
	virtual void processReReqMsg(SessionManager & sessionManager,
		rfa::sessionLayer::RequestToken & token, StreamItem & streamItem,
		const rfa::message::ReqMsg & msg);
	bool    isValidRequest(const rfa::message::AttribInfo& attrib);

protected:
	ServiceDirectoryStreamItem* getStreamItem(const rfa::message::AttribInfo& attrib);
	void			sendRefresh( rfa::sessionLayer::RequestToken & token, 
								 const rfa::message::ReqMsg & reqMsg, 
								 ServiceDirectoryStreamItem & srcDirStreamItem );
	void			cleanupStreamItem(ServiceDirectoryStreamItem *  pSrcDirSI);

	typedef std::list< ServiceDirectoryStreamItem *> SRC_DIR_SI_LIST;
	SRC_DIR_SI_LIST									_streams;

	ServiceDirectory&				_srcDirGen;
	const rfa::config::ConfigTree*	_pConfigTree;

private :
	DirectoryManager& operator=( const DirectoryManager& );
};

#endif

