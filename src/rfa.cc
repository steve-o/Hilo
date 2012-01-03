/* RFA context.
 */

#include "rfa.hh"

#include <cassert>

using rfa::common::RFA_String;

static const RFA_String kContextName ("RFA");
static const RFA_String kConnectionType ("RSSL_NIPROV");

/* Translate forward slashes into backward slashes for broken Rfa library.
 */
static
void
fix_rfa_string_path (
	RFA_String&	rfa_str
	)
{
#ifndef RFA_HIVE_ABBREVIATION_FIXED
/* RFA string API is hopeless, use std library. */
	std::string str (rfa_str.c_str());
	if (0 == str.compare (0, 2, "HK")) {
		if (0 == str.compare (2, 2, "LM"))
			str.replace (2, 2, "EY_LOCAL_MACHINE");
		else if (0 == strncmp (str.c_str(), "HKCC", 4))
			str.replace (2, 2, "EY_CURRENT_CONFIG");
		else if (0 == strncmp (str.c_str(), "HKCR", 4))
			str.replace (2, 2, "EY_CLASSES_ROOT");
		else if (0 == strncmp (str.c_str(), "HKCU", 4))
			str.replace (2, 2, "EY_CURRENT_USER");
		else if (0 == strncmp (str.c_str(), "HKU", 3))
			str.replace (2, 2, "EY_USERS");
		rfa_str.set (str.c_str());
	}
#endif
#ifndef RFA_FORWARD_SLASH_IN_PATH_FIXED
	size_t pos = 0;
	while (-1 != (pos = rfa_str.find ("/", (unsigned)pos)))
		rfa_str.replace ((unsigned)pos++, 1, "\\");
#endif
}

hilo::rfa_t::rfa_t (const config_t& config) :
	config_ (config),
	rfa_config_ (nullptr)
{
}

hilo::rfa_t::~rfa_t()
{
	if (nullptr != rfa_config_)
		rfa_config_->release();
	rfa::common::Context::uninitialize();
}

bool
hilo::rfa_t::init()
{
	const RFA_String sessionName (config_.session_name.c_str(), 0, false),
		connectionName (config_.connection_name.c_str(), 0, false);

	rfa::common::Context::initialize();

/* 8.2.3 Populate Config Database.
 */
	rfa::config::StagingConfigDatabase* staging = rfa::config::StagingConfigDatabase::create();
	assert (nullptr != staging);

/* Disable Windows Event Logger. */
	RFA_String name, value;

	name.set ("/Logger/AppLogger/windowsLoggerEnabled");
	fix_rfa_string_path (name);
	staging->setBool (name, false);
/* Session list */
	name = "/Sessions/" + sessionName + "/connectionList";
	fix_rfa_string_path (name);
	staging->setString (name, connectionName);
/* Connection list */
	name = "/Connections/" + connectionName + "/connectionType";
	fix_rfa_string_path (name);
	staging->setString (name, kConnectionType);
	name = "/Connections/" + connectionName + "/hostname";
	fix_rfa_string_path (name);
	value.set (config_.adh_address.c_str());
	staging->setString (name, value);
	name = "/Connections/" + connectionName + "/rsslPort";
	fix_rfa_string_path (name);
	value.set (config_.adh_port.c_str());
	staging->setString (name, value);

	rfa_config_ = rfa::config::ConfigDatabase::acquire (kContextName);
	assert (nullptr != rfa_config_);

	bool is_config_merged = rfa_config_->merge (*staging);
	staging->destroy();
	if (!is_config_merged)
		return false;

/* Windows Registry override. */
	if (!config_.key.empty()) {
		staging = rfa::config::StagingConfigDatabase::create();
		assert (nullptr != staging);
		name = config_.key.c_str();
		fix_rfa_string_path (name);
		staging->load (rfa::config::windowsRegistry, name);
		is_config_merged = rfa_config_->merge (*staging);
		staging->destroy();
		if (!is_config_merged)
			return false;
	}

	return true;
}

/* eof */
