# CMake build script for Velocity FX High-Low plugin
# x64 Windows Server-only
# 2011/12/21 -- Steven.McCoy@thomsonreuters.com

cmake_minimum_required (VERSION 2.8)

project (Hilo)

set(NETSNMP_ROOT C:/usr)
set(VHAYU_ROOT C:/Vhayu)
set(RFA_ROOT C:/rfa7.2.0.L1.win-shared.rrg)
set(BOOST_ROOT C:/boost_1_48_0)
set(BOOST_LIBRARYDIR ${BOOST_ROOT}/stage/lib)

set(Boost_USE_STATIC_LIBS ON)

find_package (Boost 1.44 COMPONENTS thread REQUIRED)

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
# default to Release build

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
# Windows Server 2008 (not-R2)
	-D_WIN32_WINNT=0x0600
# Net-SNMP Autoconf overrides
	-DHAVE_STDINT_H
)

# SEH Exceptions
set(CMAKE_CXX_FLAGS "/EHa")

#-----------------------------------------------------------------------------
# source files

set(cxx-sources
	src/get_hilo.cc
	src/config.cc
	src/error.cc
	src/plugin.cc
	src/provider.cc
	src/rfa.cc
	src/rfa_logging.cc
	src/snmp_agent.cc
	src/stitch.cc
	src/stitchMIB.cc
	src/chromium/debug/stack_trace.cc
	src/chromium/debug/stack_trace_win.cc
	src/chromium/memory/singleton.cc
	src/chromium/logging.cc
	src/chromium/synchronization/lock.cc
	src/chromium/synchronization/lock_impl_win.cc
)

set(rc-sources
	src/version.rc
)

include_directories(
	include
	${NETSNMP_ROOT}/include
	${VHAYU_ROOT}/Engine/TBSDK/Include
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/include
	${RFA_ROOT}/Include
	${RFA_ROOT}/Include/rwf
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${NETSNMP_ROOT}/lib
	${VHAYU_ROOT}/Engine/TBSDK/Lib64/Release
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/lib
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD
	${Boost_LIBRARY_DIRS}
)

#-----------------------------------------------------------------------------
# output

add_library(Hilo SHARED ${cxx-sources} ${rc-sources})

target_link_libraries(Hilo
	netsnmp.lib
	netsnmpagent.lib
	netsnmpmibs.lib
	FlexRecord.lib
	PluginFramework.lib
	Utility.lib
	SEDll.lib
	xerces-c_3.lib
	RFA7_Common100_x64.lib
	RFA7_Config100_x64.lib
	RFA7_Logger100_x64.lib
	RFA7_Data100_x64.lib
	RFA7_Connections100_x64.lib
	RFA7_Connections_OMM100_x64.lib
	RFA7_SessionLayer100_x64.lib
	${Boost_LIBRARIES}
	ws2_32.lib
	dbghelp.lib
)

set(rfa_runtime_libraries
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Common100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Config100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Logger100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Data100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Connections100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_Connections_OMM100_x64.dll
	${RFA_ROOT}/Libs/WIN_64_VS100/Release_MD/RFA7_SessionLayer100_x64.dll
)

file(GLOB config "${CMAKE_CURRENT_SOURCE_DIR}/config/*.xml")
file(GLOB mibs "${CMAKE_CURRENT_SOURCE_DIR}/mibs/*.txt")

install (TARGETS Hilo DESTINATION bin)
install (FILES ${rfa_runtime_libraries} DESTINATION bin)
install (FILES ${config} DESTINATION config)
install (FILES ${mibs} DESTINATION mibs)

set (CPACK_SOURCE_IGNORE_FILES "*.lib")

include (InstallRequiredSystemLibraries)
set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Velocity Analytics FX High-Low plugin")
set (CPACK_PACKAGE_VENDOR "Thomson Reuters")
set (CPACK_NSIS_CONTACT "Steven McCoy <Steven.McCoy@thomsonreuters.com>")
# There is a bug in NSI that does not handle full unix paths properly. Make
# sure there is at least one set of four (4) backlasshes.
set (CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\branding.bmp")
set (CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set (CPACK_PACKAGE_VERSION_MAJOR "1")
set (CPACK_PACKAGE_VERSION_MINOR "5")
set (CPACK_PACKAGE_VERSION_PATCH "34")
include (CPack)

# end of file
