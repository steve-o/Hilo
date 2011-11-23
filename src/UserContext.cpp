#include "UserContext.h"

#include <assert.h>
#include <iostream>

#include "vpf/vpf.h"

#include "Data/ElementList.h"
#include "Data/ElementListReadIterator.h"
#include "Data/ElementListWriteIterator.h"
#include "Message/AttribInfo.h"

#include "Authorizer.h"

using namespace std;
using namespace rfa::message;
using namespace rfa::common;

UserContext::UserContext( const AttribInfo& attribInfo, DataGenerator & dataGenerator )
: StreamItem( attribInfo, dataGenerator )
{
	_userName = attribInfo.getName();
	
	if ( attribInfo.getHintMask() & AttribInfo::AttribFlag )
	{
		assert(attribInfo.getAttrib().getDataType() == ElementListEnum);
		saveAttrib( static_cast<const ElementList &>(attribInfo.getAttrib()) );
	}

}

void UserContext::saveAttrib( const ElementList & elementList )
{
	// only save the attributes that are understood and not overwritten
	ElementListReadIterator readIter;
	readIter.start( elementList );
	while ( !readIter.off() )
	{
		const ElementEntry & entry = readIter.value();
		if ( entry.getName() == "ApplicationId" )
		{
			assert( entry.getData().getDataType() == DataBufferEnum );
			const DataBuffer & buf = static_cast<const DataBuffer&>(entry.getData());
			assert( buf.getDataBufferType() == DataBuffer::StringAsciiEnum );
			_applicationId = buf.getAsString();
		}
		else if ( entry.getName() == "Position" )
		{
			assert( entry.getData().getDataType() == DataBufferEnum );
			const DataBuffer & buf = static_cast<const DataBuffer&>(entry.getData());
			assert( buf.getDataBufferType() == DataBuffer::StringAsciiEnum );
			_position = buf.getAsString();
		}

		readIter.forth();
	}
}

bool UserContext::satisfies( const AttribInfo & attribInfo ) const
{
	if ( !(_userName == attribInfo.getName()) )
		return false;

	bool applicationIdMatched = false;
	bool positionMatched = false;

	assert( attribInfo.getAttrib().getDataType() == ElementListEnum );
	const ElementList & elementList = static_cast<const ElementList &>(attribInfo.getAttrib());
	ElementListReadIterator readIter;
	readIter.start( elementList );
	while ( !readIter.off() )
	{
		const ElementEntry & entry = readIter.value();

		if ( entry.getName() == "ApplicationId" )
		{
			assert( entry.getData().getDataType() == DataBufferEnum );
			const DataBuffer & buf = static_cast<const DataBuffer&>(entry.getData());
			assert( buf.getDataBufferType() == DataBuffer::StringAsciiEnum );
			if ( _applicationId == buf.getAsString() )
				applicationIdMatched = true;
			else
				return false;
		}
		else if ( entry.getName() == "Position" )
		{
			assert( entry.getData().getDataType() == DataBufferEnum );
			const DataBuffer & buf = static_cast<const DataBuffer&>(entry.getData());
			assert( buf.getDataBufferType() == DataBuffer::StringAsciiEnum );
			if ( _position == buf.getAsString() )
				positionMatched = true;
			else
				return false;
		}
		// else ignore
		
		readIter.forth();
	}

	return positionMatched && applicationIdMatched;
}

void UserContext::createStatus( RespStatus & status )
{
}

void UserContext::createData( Data & data, RespMsg::RespType respType, UInt32 maxSize )
{
	assert(data.getDataType() == ElementListEnum);
	ElementList & attrib = static_cast<ElementList &>(data);
	ElementListWriteIterator iter;
	ElementEntry entry;
	DataBuffer buffer;
	RFA_String elemName;

	iter.start(attrib);

	elemName = RFA_String( "ApplicationId", 0, false );
	buffer.setFromString( _applicationId, DataBuffer::StringAsciiEnum );
	entry.setName(elemName);
	entry.setData(buffer);
	iter.bind(entry);

	elemName = RFA_String( "Position", 0, false );
	buffer.setFromString( _position, DataBuffer::StringAsciiEnum );
	entry.setName(elemName);
	entry.setData(buffer);
	iter.bind(entry);

	elemName = RFA_String( "ProvidePermissionProfile", 0, false );
	buffer.setUInt32(0);
	entry.setName(elemName);
	entry.setData(buffer);
	iter.bind(entry);

	elemName = RFA_String( "ProvidePermissionExpressions", 0, false );
	buffer.setUInt32(0);
	entry.setName(elemName);
	entry.setData(buffer);
	iter.bind(entry);

	elemName = RFA_String( "SingleOpen", 0, false );
	buffer.setUInt32(0);
	entry.setName(elemName);
	entry.setData(buffer);
	iter.bind(entry);

	elemName = RFA_String( "AllowSuspectData", 0, false );
	buffer.setUInt32(1);
	entry.setName(elemName);
	entry.setData(buffer);
	iter.bind(entry);

	iter.complete();
}

void UserContext::continueData( Data & data, UInt32 maxSize )
{
}
