# CMake build script for Velocity FX High-Low plugin
# x64 Windows Server-only
# 2011/12/21 -- Steven.McCoy@thomsonreuters.com

cmake_minimum_required (VERSION 2.8.8)

project (Hilo)

set(HILO_VERSION_MAJOR 3)
set(HILO_VERSION_MINOR 1)
set(HILO_VERSION_BUILD 55)

# Net-SNMP SDK installed, tested 5.7.1.
set(NETSNMP_ROOT D:/net-snmp-5.7.1/stage/vc100/x64/release)
set(NETSNMP_INCLUDE_DIRS ${NETSNMP_ROOT}/include)
set(NETSNMP_LIBRARY_DIRS ${NETSNMP_ROOT}/lib)
set(NETSNMP_LIBRARIES
	netsnmp.lib
	netsnmpagent.lib
	netsnmpmibs.lib
)

# TREP-VA 7.0 SDK
set(VHAYU_ROOT D:/Vhayu-7.0.0)
set(VHAYU_INCLUDE_DIRS
	${VHAYU_ROOT}/Engine/TBSDK/Include
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/include
)
set(VHAYU_LIBRARY_DIRS
	${VHAYU_ROOT}/Engine/TBSDK/Lib64/Release
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/lib
)
set(VHAYU_LIBRARIES
	FlexRecord.lib
	PluginFramework.lib
	Utility.lib
	SEDll.lib
	xerces-c_3.lib
)

# Thomson Reuters Robust Foundation API
set(RFA_ROOT D:/rfa7.2.1.L1.win-shared.rrg)
set(RFA_INCLUDE_DIRS
	${RFA_ROOT}/Include
	${RFA_ROOT}/Include/rwf
)
set(RFA_LIBRARY_DIRS ${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD)
set(RFA_LIBRARIES
        RFA7_Common100_x64.lib
        RFA7_Config100_x64.lib
        RFA7_Logger100_x64.lib
        RFA7_Data100_x64.lib
        RFA7_Connections100_x64.lib
        RFA7_Connections_OMM100_x64.lib
        RFA7_SessionLayer100_x64.lib
)
set(rfa_runtime_libraries
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Common100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Config100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Logger100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Data100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_RSSL_Adapter100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_RSSL_Cons_Adapter100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_RSSL_Prov_Adapter100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Connections100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Connections_OMM100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_SessionLayer_OMM100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_SessionLayer100_x64.dll
)

# Boost headers plus built libraries
set(BOOST_ROOT D:/boost_1_51_0)
set(BOOST_LIBRARYDIR ${BOOST_ROOT}/stage/lib)
set(Boost_USE_STATIC_LIBS ON)
find_package (Boost 1.50 COMPONENTS thread REQUIRED)

find_package(PythonInterp REQUIRED)

option(CONFIG_32BIT_PRICE
	"Publish 32-bit prices instead of 64-bit." OFF)

#-----------------------------------------------------------------------------
# force off-tree build

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "CMake generation is not allowed within the source directory!
Remove the CMakeCache.txt file and try again from another folder, e.g.:

   del CMakeCache.txt
   mkdir build
   cd build
   cmake ..
")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

#-----------------------------------------------------------------------------
# default to Release build, frequently broken: manually run cmake-gui to fix

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

#-----------------------------------------------------------------------------
# platform specifics

add_definitions(
	-DWIN32
	-DWIN32_LEAN_AND_MEAN
# Windows Server 2008 R2, Windows 7
	-D_WIN32_WINNT=0x0601
# Net-SNMP Autoconf overrides
	-DHAVE_STDINT_H
	-DHAVE_SOCKLEN_T
# RFA version
        -DRFA_LIBRARY_VERSION="7.2.1."
# production release
	-DOFFICIAL_BUILD
)

if(CONFIG_32BIT_PRICE)
	add_definitions(
		-DCONFIG_32BIT_PRICE
	)
endif(CONFIG_32BIT_PRICE)

# SEH Exceptions.
string(REGEX REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Parallel make.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Optimization flags.
# http://msdn.microsoft.com/en-us/magazine/cc301698.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

# Disable buffer security check.
# http://msdn.microsoft.com/en-us/library/8dbf701c(v=vs.80).aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GS-")

#-----------------------------------------------------------------------------
# source files

configure_file(
	${CMAKE_SOURCE_DIR}/version_generator.py.in
	${CMAKE_BINARY_DIR}/version_generator.py
	@ONLY
)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/version.cc
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_BINARY_DIR}/version_generator.py > ${CMAKE_BINARY_DIR}/version.cc
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	DEPENDS ${CMAKE_BINARY_DIR}/version_generator.py
)
configure_file(
	${CMAKE_SOURCE_DIR}/src/version.hh
	${CMAKE_BINARY_DIR}/version.hh
	COPYONLY
)
set(cxx-sources
	src/get_hilo.cc
	src/config.cc
	src/error.cc
	src/plugin.cc
	src/provider.cc
	src/rfa.cc
	src/rfa_logging.cc
	src/session.cc
	src/snmp_agent.cc
	src/stitch.cc
	src/stitchMIB.cc
	src/tcl.cc
	src/chromium/chromium_switches.cc
	src/chromium/command_line.cc
	src/chromium/debug/stack_trace.cc
	src/chromium/debug/stack_trace_win.cc
	src/chromium/memory/singleton.cc
	src/chromium/logging.cc
	src/chromium/string_piece.cc
	src/chromium/string_split.cc
	src/chromium/string_util.cc
	src/chromium/synchronization/lock.cc
	src/chromium/synchronization/lock_impl_win.cc
	src/chromium/vlog.cc
	${CMAKE_BINARY_DIR}/version.cc
)

configure_file(
	${CMAKE_SOURCE_DIR}/version.rc.in
	${CMAKE_BINARY_DIR}/version.rc
	@ONLY
)
set(rc-sources
	${CMAKE_BINARY_DIR}/version.rc
)

include_directories(
	include
	${NETSNMP_INCLUDE_DIRS}
	${VHAYU_INCLUDE_DIRS}
	${RFA_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${NETSNMP_LIBRARY_DIRS}
	${VHAYU_LIBRARY_DIRS}
	${RFA_LIBRARY_DIRS}
	${Boost_LIBRARY_DIRS}
)

#-----------------------------------------------------------------------------
# output

if(CONFIG_32BIT_PRICE)
	set(HILO_TARGET Hilo32)
else(CONFIG_32BIT_PRICE)
	set(HILO_TARGET Hilo)
endif(CONFIG_32BIT_PRICE)

add_library(${HILO_TARGET} SHARED ${cxx-sources} ${rc-sources})
target_link_libraries(${HILO_TARGET}
	${NETSNMP_LIBRARIES}
	${VHAYU_LIBRARIES}
	${RFA_LIBRARIES}
	${Boost_LIBRARIES}
	ws2_32.lib
	dbghelp.lib
)
install (TARGETS ${HILO_TARGET} DESTINATION bin)

set(config
	${CMAKE_SOURCE_DIR}/config/HiloAndStitch.xml
	${CMAKE_SOURCE_DIR}/config/create-symbol-list.pl
	${CMAKE_SOURCE_DIR}/config/example.reg
)
file(GLOB mibs "${CMAKE_SOURCE_DIR}/mibs/*.txt")
install (FILES ${rfa_runtime_libraries} DESTINATION bin)
install (FILES ${config} DESTINATION config)
install (FILES ${mibs} DESTINATION mibs)

include (InstallRequiredSystemLibraries)

set (CPACK_SOURCE_IGNORE_FILES "*.lib")

set (CPACK_INSTALL_CMAKE_PROJECTS
	"${CMAKE_SOURCE_DIR}/build;Hilo;ALL;/"
	"${CMAKE_SOURCE_DIR}/build32;Hilo;ALL;/"
)

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Velocity Analytics FX High-Low plugin")
set (CPACK_PACKAGE_VENDOR "Thomson Reuters")
set (CPACK_NSIS_CONTACT "Steven McCoy <Steven.McCoy@thomsonreuters.com>")
# There is a bug in NSI that does not handle full unix paths properly. Make
# sure there is at least one set of four (4) backlasshes.
set (CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}\\\\installer.ico")
set (CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}\\\\installer.ico")
set (CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\\\branding.bmp")
set (CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set (CPACK_PACKAGE_VERSION_MAJOR "${HILO_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${HILO_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${HILO_VERSION_BUILD}")

include (CPack)

# end of file
