/* lock_impl.hh
 *
 * A basic platform specific spin-lock.
 *
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 */

#ifndef __LOCK_IMPL_HH_
#define __LOCK_IMPL_HH_

#pragma once

#include <windows.h>

/* Boost noncopyable base class */
#include <boost/utility.hpp>

namespace chromium {
namespace internal {

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class.  Most users should not use LockImpl directly, but
// should instead use Lock.
class LockImpl :
	boost::noncopyable
{
public:
	typedef CRITICAL_SECTION OSLockType;

	LockImpl();
	~LockImpl();

// If the lock is not held, take it and return true.  If the lock is already
// held by something else, immediately return false.
	bool Try();

// Take the lock, blocking until it is available if necessary.
	void Lock();

// Release the lock.  This must only be called by the lock's holder: after
// a successful call to Try, or a call to Lock.
	void Unlock();

private:
	OSLockType os_lock_;
};

} /* namespace internal */
} /* namespace chromium */

#endif /* __LOCK_IMPL_HH_ */

/* eof */