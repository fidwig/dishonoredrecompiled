/*=============================================================================
	MacThreading.h: Unreal definitions for multithreading on macOS (pthreads).
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MACTHREADING_H__
#define __MACTHREADING_H__

#include <pthread.h>
#include <sys/time.h>

/**
 * Interlocked style functions for threadsafe atomic operations
 */

FORCEINLINE INT appInterlockedIncrement(volatile INT* Value)
{
	return (INT)__sync_add_and_fetch(Value, 1);
}

FORCEINLINE INT appInterlockedDecrement(volatile INT* Value)
{
	return (INT)__sync_sub_and_fetch(Value, 1);
}

FORCEINLINE INT appInterlockedAdd(volatile INT* Value, INT Amount)
{
	return (INT)__sync_fetch_and_add(Value, Amount);
}

FORCEINLINE INT appInterlockedExchange(volatile INT* Value, INT Exchange)
{
	return (INT)__sync_lock_test_and_set(Value, Exchange);
}

FORCEINLINE INT appInterlockedCompareExchange(INT* Dest, INT Exchange, INT Comperand)
{
	return (INT)__sync_val_compare_and_swap(Dest, Comperand, Exchange);
}

FORCEINLINE void* appInterlockedCompareExchangePointer(void** Dest, void* Exchange, void* Comperand)
{
	return (void*)__sync_val_compare_and_swap(Dest, Comperand, Exchange);
}

FORCEINLINE HANDLE appGetCurrentThread(void)
{
	return (HANDLE)(uintptr_t)pthread_self();
}

FORCEINLINE DWORD appGetCurrentThreadId(void)
{
	return (DWORD)(uintptr_t)pthread_self();
}

FORCEINLINE DWORD appSetThreadAffinity(HANDLE ThreadHandle, DWORD PreferredProcessor)
{
	(void)ThreadHandle;
	(void)PreferredProcessor;
	return 0;
}

FORCEINLINE DWORD appAllocTlsSlot(void)
{
	pthread_key_t Key;
	if (pthread_key_create(&Key, NULL) == 0)
	{
		return (DWORD)Key;
	}
	return (DWORD)-1;
}

FORCEINLINE void appSetTlsValue(DWORD SlotIndex, void* Value)
{
	pthread_setspecific((pthread_key_t)SlotIndex, Value);
}

FORCEINLINE void* appGetTlsValue(DWORD SlotIndex)
{
	return pthread_getspecific((pthread_key_t)SlotIndex);
}

FORCEINLINE void appFreeTlsSlot(DWORD SlotIndex)
{
	pthread_key_delete((pthread_key_t)SlotIndex);
}

/**
 * Critical section implementation using recursive pthread_mutex.
 */
class FCriticalSection : public FSynchronize
{
	pthread_mutex_t Mutex;

public:
	FORCEINLINE FCriticalSection()
	{
		pthread_mutexattr_t Attr;
		pthread_mutexattr_init(&Attr);
		pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&Mutex, &Attr);
		pthread_mutexattr_destroy(&Attr);
	}

	FORCEINLINE ~FCriticalSection()
	{
		pthread_mutex_destroy(&Mutex);
	}

	FORCEINLINE void Lock(void)
	{
		pthread_mutex_lock(&Mutex);
	}

	FORCEINLINE void Unlock(void)
	{
		pthread_mutex_unlock(&Mutex);
	}
};

/**
 * Event synchronization object using pthread condition variables.
 */
class FEventMac : public FEvent
{
	pthread_mutex_t Mutex;
	pthread_cond_t  Cond;
	UBOOL           bIsManualReset;
	UBOOL           bTriggered;
	UBOOL           bInitialized;

public:
	FEventMac();
	virtual ~FEventMac();

	virtual void Lock(void);
	virtual void Unlock(void);

	virtual UBOOL Create(UBOOL bInIsManualReset = FALSE, const TCHAR* InName = NULL);
	virtual void Trigger(void);
	virtual void Reset(void);
	virtual void Pulse(void);
	virtual UBOOL Wait(DWORD WaitTime = (DWORD)-1);
};

/**
 * Synchronization object factory for macOS.
 */
class FSynchronizeFactoryMac : public FSynchronizeFactory
{
public:
	FSynchronizeFactoryMac() {}
	virtual FCriticalSection* CreateCriticalSection(void);
	virtual FEvent* CreateSynchEvent(UBOOL bIsManualReset = FALSE, const TCHAR* InName = NULL);
	virtual void Destroy(FSynchronize* InSynchObj);
};

/**
 * Pooled thread for macOS.
 */
class FQueuedThreadMac : public FQueuedThread
{
	FEvent*                 DoWorkEvent;
	pthread_t               Thread;
	DWORD                   ThreadID;
	volatile UBOOL          TimeToDie;
	FQueuedWork* volatile   QueuedWork;
	FQueuedThreadPool*      OwningThreadPool;

	static void* _ThreadProc(void* pThis);
	void Run(void);

public:
	FQueuedThreadMac();
	virtual ~FQueuedThreadMac();

	virtual UBOOL Create(class FQueuedThreadPool* InPool, DWORD ProcessorMask = 0,
		DWORD InStackSize = 0, EThreadPriority InThreadPri = TPri_Normal);
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE, DWORD WaitTime = (DWORD)-1);
	virtual void DoWork(FQueuedWork* InWork);
};

/**
 * Thread pool for macOS.
 */
class FQueuedThreadPoolMac : public FQueuedThreadPoolBase
{
public:
	virtual UBOOL Create(DWORD InNumQueuedThreads, DWORD ProcessorMask = 0,
		DWORD StackSize = (32 * 1024), EThreadPriority ThreadPriority = TPri_Normal);
};

/**
 * Runnable thread for macOS.
 */
class FRunnableThreadMac : public FRunnableThread
{
	pthread_t       Thread;
	FRunnable*      Runnable;
	FEvent*         ThreadInitSyncEvent;
	UBOOL           bShouldDeleteSelf;
	UBOOL           bShouldDeleteRunnable;
	EThreadPriority ThreadPriority;
	DWORD           ThreadID;

	static void* _ThreadProc(void* pThis);
	DWORD Run(void);

public:
	FRunnableThreadMac();
	virtual ~FRunnableThreadMac();

	virtual UBOOL Create(FRunnable* InRunnable, const TCHAR* ThreadName,
		UBOOL bAutoDeleteSelf = 0, UBOOL bAutoDeleteRunnable = 0,
		DWORD InStackSize = 0, EThreadPriority InThreadPri = TPri_Normal);
	virtual void SetThreadPriority(EThreadPriority NewPriority) {}
	virtual void SetProcessorAffinity(DWORD ProcessorNum) { (void)ProcessorNum; }
	virtual void Suspend(UBOOL bShouldPause = 1);
	virtual UBOOL Kill(UBOOL bShouldWait = FALSE);
	virtual void WaitForCompletion(void);
	virtual DWORD GetThreadID(void);
};

/**
 * Thread factory for macOS.
 */
class FThreadFactoryMac : public FThreadFactory
{
public:
	virtual FRunnableThread* CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName,
		UBOOL bAutoDeleteSelf = 0, UBOOL bAutoDeleteRunnable = 0,
		DWORD InStackSize = 0, EThreadPriority InThreadPri = TPri_Normal);
	virtual void Destroy(FRunnableThread* InThread);
};

#endif // __MACTHREADING_H__
