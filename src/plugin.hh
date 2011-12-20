
#ifndef __TEMP_VELOCITY_PLUGIN_HH__
#define __TEMP_VELOCITY_PLUGIN_HH__

#pragma once


/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace temp
{

	class PluginFactory : public vpf::ObjectFactory
	{
	public:
		PluginFactory();
		virtual void* newInstance (const char* type);
	};

} /* namespace temp */

#endif /* __TEMP_VELOCITY_PLUGIN_HH__ */

/* eof */
