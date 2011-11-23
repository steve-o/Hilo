#ifndef EventQueueDispatcher_H
#define EventQueueDispatcher_H

#include "vpf/vpf.h"

class ReqRouter;
class ConfigManager;
#include <windows.h>
#include "TBUserDll.h"

/**
 * EventQueueDispatcher
 */
class EventQueueDispatcher : public vpf::Runnable
{
public:
	EventQueueDispatcher(rfa::common::EventQueue* eq, void* dllHandle) :
	  mQueue(eq), mDllHandle(dllHandle), mThreadHandle(0), mNumThreads(0)
	  {}

    void start() {
		DWORD tid;
		mThreadHandle = CreateThread(NULL, 0, ThreadFunc, this, 0, &tid);
	}

	virtual void run() {
		InterlockedIncrement(&mNumThreads);
		MsgLog(eMsgInfo, 393319, "* EventQueueDispatcher: starting");
		while (mQueue->isActive()) {
			mQueue->dispatch(-1); // InfiniteWait
		}
		MsgLog(eMsgInfo, 393352, "* EventQueueDispatcher: stopped");
		InterlockedDecrement(&mNumThreads);
	}

	void stop() {
		if (mThreadHandle) {
			while (mNumThreads) {
				MsgLog(eMsgInfo, 393359, "* EventQueueDispatcher: waiting in stop (%d)", mNumThreads);
				Sleep(100);
			}
			CloseHandle(mThreadHandle);
		}
	}

private:
	rfa::common::EventQueue* mQueue;
	void* mDllHandle;
	HANDLE mThreadHandle;
	long mNumThreads;
};

#endif
