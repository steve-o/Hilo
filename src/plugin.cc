
#include "plugin.hh"
#include "hilo.hh"

static const char* kPluginType = "HiloPlugin";

static hilo::PluginFactory g_factory_instance;

hilo::PluginFactory::PluginFactory()
{
	registerType (kPluginType);
}

/* no API to unregister type. */

void*
hilo::PluginFactory::newInstance (
	const char*	type
	)
{
	assert (0 == strcmp (kPluginType, type));
	return new hilo::hilo_t();
}

/* eof */
