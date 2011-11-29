
#include "plugin.hh"
#include "velocity_receiver.hh"

static temp::velocity_plugin_t g_factory_instance;


temp::velocity_plugin_t::velocity_plugin_t()
{
	registerType ("TempPlugin");
}

void*
temp::velocity_plugin_t::newInstance (
	const char*	type
	)
{
	printf ("type: %s\n", type);
	return new temp::velocity_receiver_t();
}

/* eof */
