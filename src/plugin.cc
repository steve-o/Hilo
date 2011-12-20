
#include "plugin.hh"
#include "temp.hh"

static const char* kPluginType = "TempPlugin";

static temp::PluginFactory g_factory_instance;

temp::PluginFactory::PluginFactory()
{
	registerType (kPluginType);
}

/* no API to unregister type. */

void*
temp::PluginFactory::newInstance (
	const char*	type
	)
{
	assert (0 == strcmp (kPluginType, type));
	return new temp::temp_t();
}

/* eof */
