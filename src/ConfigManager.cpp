#include "ConfigManager.h"

#include <assert.h>

#include "Config/ConfigDatabase.h"
#include "Config/StagingConfigDatabase.h"
#include "Config/ConfigTree.h"

using namespace std;
// extern "C" void checkException(rfa::common::Exception& e);

ConfigManager::ConfigManager(const RFA_String & configName)
{
	_configName = configName;
	_stagingConfigDatabase = 0;
	_configDatabase = 0;
}

ConfigManager::~ConfigManager()
{
	cleanup();
}

void ConfigManager::cleanup()
{
	if (_stagingConfigDatabase)
	{
		_stagingConfigDatabase->destroy();
		_stagingConfigDatabase = 0;
	}
	if (_configDatabase)
	{
        _configDatabase->release();
		_configDatabase = 0;
	}
}

bool ConfigManager::load(ConfigRepositoryType type, const RFA_String & location)
{	
	try
	{
		// Populate the ConfigDatabase
		_configDatabase = rfa::config::ConfigDatabase::acquire(_configName);
		assert(_configDatabase);

		_stagingConfigDatabase = rfa::config::StagingConfigDatabase::create();
		assert(_stagingConfigDatabase);

		if (!_stagingConfigDatabase->load(type, location))
		{
			cleanup();
			return false;
		}

		initTree();

	}
	catch (Exception& e)
	{
		cleanup();
		return false;
		
	}
	return true;
}

bool ConfigManager::initFile(const RFA_String & configFileName)
{
	return load(flatFile, configFileName);
}

/* 
 This method is used for populating parameters for RFAConfig Database.
 Typically these parameters used to exist in RFAConfig File (providerRFA.cfg). 
 For example the connectionType is always RSSL. So this will populated in the
 program and removed from the Config File.
*/

void ConfigManager::populateRFAConfig()
{
	try
	{
		// Populate the ConfigDatabase
		assert(_configDatabase);

		//_stagingConfigDatabase = rfa::config::StagingConfigDatabase::create();
		//assert(_stagingConfigDatabase);

		_stagingConfigDatabase->setString("\\Connections\\VelocityProviderConnection\\ConnectionType", "RSSL");
		_stagingConfigDatabase->setString("\\Sessions\\VelocitySession\\connectionList", "VelocityProviderConnection");
		initTree();
	}
	catch (Exception& e)
	{
		
	}
//	return true;
}

void ConfigManager::populateAPPConfig()
{
	try
	{
		// Populate the ConfigDatabase
		_configDatabase = rfa::config::ConfigDatabase::acquire(_configName);
		assert(_configDatabase);

		RFA_String fieldDictionaryFilename = _configTree->getChildAsString("FieldDictionaryFilename", "./SampleRDMFieldDictionary");
		RFA_String enumDictionaryFilename = _configTree->getChildAsString("EnumDictionaryFilename", "./SampleRDMEnumType.def");

		_stagingConfigDatabase = rfa::config::StagingConfigDatabase::create();
		assert(_stagingConfigDatabase);

		_stagingConfigDatabase->setString("\\Session", "VelocitySession");
		_stagingConfigDatabase->setString("\\FieldDictionaryFilename", fieldDictionaryFilename.c_str());
		_stagingConfigDatabase->setString("\\EnumDictionaryFilename", enumDictionaryFilename.c_str());
		

		_stagingConfigDatabase->setString("\\Login", NULL);
		_stagingConfigDatabase->setString("\\Login\\dumpLoginParameters", "true");
		_stagingConfigDatabase->setString("\\Login\\userNames", "user,user1");

		_stagingConfigDatabase->setString("\\Services" ,NULL);
		_stagingConfigDatabase->setString("\\Services\\VelocityService", "");
		_stagingConfigDatabase->setString("\\Services\\VelocityService\\Vendor", "VelocityVendor" );
		_stagingConfigDatabase->setBool("\\Services\\VelocityService\\IsSource", true);
		_stagingConfigDatabase->setString("\\Services\\VelocityService\\QoS", "");
		_stagingConfigDatabase->setLong("\\Services\\VelocityService\\QoS\\Rate", 0);
		_stagingConfigDatabase->setLong("\\Services\\VelocityService\\QoS\\Timeliness", 0);
		_stagingConfigDatabase->setLong("\\Services\\VelocityService\\MaxNumRequests", 1000);
		_stagingConfigDatabase->setBool("\\Services\\VelocityService\\AcceptingRequests", true);
		_stagingConfigDatabase->setString("\\Services\\VelocityService\\Capabilities" , "5,6,7,9");

		_stagingConfigDatabase->setString("\\MarketPrice", NULL);
		_stagingConfigDatabase->setString("\\MarketPrice\\VelocityService", NULL);
		_stagingConfigDatabase->setLong("\\MarketPrice\\VelocityService\\UpdateInterval", 100);
		_stagingConfigDatabase->setString("\\MarketPrice\\VelocityService\\ItemList", "");


		initTree();
	}
	catch (Exception& e)
	{
		
	}
	//return true;
}

void ConfigManager::initTree()
{
	_configDatabase->merge(*_stagingConfigDatabase);
	_configTree = _stagingConfigDatabase->getConfigTree();
	assert(_configTree);
}

bool ConfigManager::getBool(const RFA_String & variableName, bool defaultVal) const
{
	return _configTree->getChildAsBool(variableName, defaultVal);
}

const RFA_String ConfigManager::getString(const RFA_String & variableName, const RFA_String & defaultVal) const
{
	return _configTree->getChildAsString(variableName, defaultVal);
}

long ConfigManager::getLong(const RFA_String & variableName, long defaultVal) const
{
	return _configTree->getChildAsLong(variableName, defaultVal);
}

const ConfigTree* ConfigManager::getTree(const RFA_String& variableName)
{
	return _configTree->getChildAsTree(variableName);
}
