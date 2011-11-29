
#ifndef __TEMP_VELOCITY_PLUGIN_HH__
#define __TEMP_VELOCITY_PLUGIN_HH__

#pragma once


/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace temp
{

	class velocity_plugin_t : public vpf::ObjectFactory
	{
	public:
		velocity_plugin_t();
		virtual void* newInstance (const char* type);
	};

} /* namespace temp */

#endif /* __TEMP_VELOCITY_PLUGIN_HH__ */

/* eof */
