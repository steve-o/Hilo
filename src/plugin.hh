
#ifndef __HILO_VELOCITY_PLUGIN_HH__
#define __HILO_VELOCITY_PLUGIN_HH__

#pragma once


/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace hilo
{

	class PluginFactory : public vpf::ObjectFactory
	{
	public:
		PluginFactory();
		virtual void* newInstance (const char* type);
	};

} /* namespace hilo */

#endif /* __HILO_VELOCITY_PLUGIN_HH__ */

/* eof */
