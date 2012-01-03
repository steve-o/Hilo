/* Boilerplate for exporting a data type to the Analytics Engine.
 */

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "stitch.hh"

static const char* kPluginType = "HiloPlugin";

namespace
{
	class factory_t : public vpf::ObjectFactory
	{
	public:
		factory_t()
		{
			registerType (kPluginType);
		}

/* no API to unregister type. */

		virtual void* newInstance (const char* type)
		{
			assert (0 == strcmp (kPluginType, type));
			return new hilo::stitch_t();
		}
	};

	static factory_t g_factory_instance;

} /* anonymous namespace */

/* eof */
