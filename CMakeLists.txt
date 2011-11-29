# CMake build script for Velocity RDM publisher plugin
# x64 Windows Server-only
# 2011/11/18 -- Steven.McCoy@thomsonreuters.com

cmake_minimum_required (VERSION 2.8)

project (RDMPublisher)

set(VHAYU_DIR C:/Vhayu)

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
)

#-----------------------------------------------------------------------------
# source files

set(cxx-sources
	src/velocity_receiver.cc
	src/plugin.cc
)

include_directories(
	include
	${VHAYU_DIR}/Engine/TBSDK/Include
	${VHAYU_DIR}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/include
	${VHAYU_DIR}/Engine/ThirdParty/rfa_6.5.0.E2/Include/rwf
	${VHAYU_DIR}/Engine/ThirdParty/rfa_6.5.0.E2/Include
)

link_directories(
	${VHAYU_DIR}/Engine/TBSDK/Lib64/Release
	${VHAYU_DIR}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/lib
	${VHAYU_DIR}/Engine/ThirdParty/rfa_6.5.0.E2/Libs/WIN_64_VS100
)

#-----------------------------------------------------------------------------
# output

add_library(RDMPublisher SHARED ${cxx-sources})

target_link_libraries(RDMPublisher
	FlexRecord.lib
	PluginFramework.lib
	Utility.lib
	SEDll.lib
)

# end of file
