/* Boilerplate for exporting a data type to the Analytics Engine.
 */

/* VA leaks a dependency upon _MAX_PATH */
#include <cstdlib>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "chromium/logging.hh"
#include "stitch.hh"

static const char* kPluginType = "HiloPlugin";

namespace
{
	class winsock_t
	{
		bool initialized;
	public:
		winsock_t (unsigned majorVersion, unsigned minorVersion) :
			initialized (false)
		{
			WORD wVersionRequested = MAKEWORD (majorVersion, minorVersion);
			WSADATA wsaData;
			if (WSAStartup (wVersionRequested, &wsaData) != 0) {
				LOG(ERROR) << "WSAStartup returned " << WSAGetLastError();
				return;
			}
			if (LOBYTE (wsaData.wVersion) != majorVersion || HIBYTE (wsaData.wVersion) != minorVersion) {
				WSACleanup();
				LOG(ERROR) << "WSAStartup failed to provide requested version " << majorVersion << '.' << minorVersion;
				return;
			}
			initialized = true;
		}

		~winsock_t ()
		{
			if (initialized)
				WSACleanup();
		}
	};

	class factory_t : public vpf::ObjectFactory
	{
		winsock_t winsock;
	public:
		factory_t() :
			winsock (2, 2)
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