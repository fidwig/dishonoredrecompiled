/*=============================================================================
	MacThreading.cpp: Unreal multithreading support on macOS.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if PLATFORM_MACOSX

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

/** The global synchronization object factory. */
FSynchronizeFactory* GSynchronizeFactory = NULL;
/** The global thread factory. */
FThreadFactory* GThreadFactory = NULL;
/** The global thread pool. */
FQueuedThreadPool* GThreadPool = NULL;

/*-----------------------------------------------------------------------------
	FEventMac
-----------------------------------------------------------------------------*/

FEventMac::FEventMac()
	: bIsManualReset(FALSE)
	, bTriggered(FALSE)
	, bInitialized(FALSE)
{
	pthread_mutex_init(&Mutex, NULL);
	pthread_cond_init(&Cond, NULL);
}

FEventMac::~FEventMac()
{
	pthread_cond_destroy(&Cond);
	pthread_mutex_destroy(&Mutex);
}

void FEventMac::Lock()
{
	Wait((DWORD)-1);
}

void FEventMac::Unlock()
{
	Reset();
}

UBOOL FEventMac::Create(UBOOL bInIsManualReset, const TCHAR* InName)
{
	(void)InName;
	bIsManualReset = bInIsManualReset;
	bTriggered = FALSE;
	bInitialized = TRUE;
	return TRUE;
}

void FEventMac::Trigger()
{
	pthread_mutex_lock(&Mutex);
	bTriggered = TRUE;
	if (bIsManualReset)
	{
		pthread_cond_broadcast(&Cond);
	}
	else
	{
		pthread_cond_signal(&Cond);
	}
	pthread_mutex_unlock(&Mutex);
}

void FEventMac::Reset()
{
	pthread_mutex_lock(&Mutex);
	bTriggered = FALSE;
	pthread_mutex_unlock(&Mutex);
}

void FEventMac::Pulse()
{
	pthread_mutex_lock(&Mutex);
	bTriggered = TRUE;
	pthread_cond_broadcast(&Cond);
	bTriggered = FALSE;
	pthread_mutex_unlock(&Mutex);
}

UBOOL FEventMac::Wait(DWORD WaitTime)
{
	pthread_mutex_lock(&Mutex);

	UBOOL bSignaled = TRUE;
	if (!bTriggered)
	{
		if (WaitTime == (DWORD)-1)
		{
			while (!bTriggered)
			{
				pthread_cond_wait(&Cond, &Mutex);
			}
		}
		else
		{
			struct timeval Now;
			gettimeofday(&Now, NULL);
			struct timespec AbsTime;
			uint64_t Nanos = (uint64_t)Now.tv_usec * 1000 + (uint64_t)WaitTime * 1000000;
			AbsTime.tv_sec = Now.tv_sec + (Nanos / 1000000000);
			AbsTime.tv_nsec = Nanos % 1000000000;

			while (!bTriggered)
			{
				int Err = pthread_cond_timedwait(&Cond, &Mutex, &AbsTime);
				if (Err != 0)
				{
					bSignaled = FALSE;
					break;
				}
			}
		}
	}

	if (bSignaled && !bIsManualReset)
	{
		bTriggered = FALSE;
	}

	pthread_mutex_unlock(&Mutex);
	return bSignaled;
}

/*-----------------------------------------------------------------------------
	FSynchronizeFactoryMac
-----------------------------------------------------------------------------*/

FCriticalSection* FSynchronizeFactoryMac::CreateCriticalSection()
{
	return new FCriticalSection();
}

FEvent* FSynchronizeFactoryMac::CreateSynchEvent(UBOOL bIsManualReset, const TCHAR* InName)
{
	FEvent* Event = new FEventMac();
	if (!Event->Create(bIsManualReset, InName))
	{
		delete Event;
		return NULL;
	}
	return Event;
}

void FSynchronizeFactoryMac::Destroy(FSynchronize* InSynchObj)
{
	delete InSynchObj;
}

/*-----------------------------------------------------------------------------
	FRunnableThreadMac
-----------------------------------------------------------------------------*/

FRunnableThreadMac::FRunnableThreadMac()
	: Thread(NULL)
	, Runnable(NULL)
	, ThreadInitSyncEvent(NULL)
	, bShouldDeleteSelf(FALSE)
	, bShouldDeleteRunnable(FALSE)
	, ThreadPriority(TPri_Normal)
	, ThreadID(0)
{
}

FRunnableThreadMac::~FRunnableThreadMac()
{
	if (Thread != NULL)
	{
		Kill(TRUE);
	}
}

void* FRunnableThreadMac::_ThreadProc(void* pThis)
{
	FRunnableThreadMac* Self = (FRunnableThreadMac*)pThis;
	Self->Run();
	return NULL;
}

DWORD FRunnableThreadMac::Run()
{
	check(Runnable);

	UBOOL bInitOk = Runnable->Init();
	if (ThreadInitSyncEvent)
	{
		ThreadInitSyncEvent->Trigger();
	}

	DWORD ExitCode = 0;
	if (bInitOk)
	{
		ExitCode = Runnable->Run();
		Runnable->Exit();
	}

	if (bShouldDeleteRunnable)
	{
		delete Runnable;
		Runnable = NULL;
	}

	if (bShouldDeleteSelf)
	{
		delete this;
	}

	return ExitCode;
}

UBOOL FRunnableThreadMac::Create(FRunnable* InRunnable, const TCHAR* ThreadName,
	UBOOL bAutoDeleteSelf, UBOOL bAutoDeleteRunnable,
	DWORD InStackSize, EThreadPriority InThreadPri)
{
	check(InRunnable);
	(void)ThreadName;
	Runnable = InRunnable;
	ThreadPriority = InThreadPri;
	bShouldDeleteSelf = bAutoDeleteSelf;
	bShouldDeleteRunnable = bAutoDeleteRunnable;

	ThreadInitSyncEvent = GSynchronizeFactory->CreateSynchEvent(TRUE);

	pthread_attr_t Attr;
	pthread_attr_init(&Attr);
	DWORD StackSize = Max(InStackSize, (DWORD)(2 * 1024 * 1024));
	pthread_attr_setstacksize(&Attr, StackSize);

	int Err = pthread_create(&Thread, &Attr, _ThreadProc, this);
	pthread_attr_destroy(&Attr);

	if (Err != 0)
	{
		Thread = NULL;
		if (bAutoDeleteRunnable)
		{
			delete InRunnable;
		}
		Runnable = NULL;
		GSynchronizeFactory->Destroy(ThreadInitSyncEvent);
		ThreadInitSyncEvent = NULL;
		return FALSE;
	}

	ThreadID = (DWORD)(uintptr_t)Thread;
	ThreadInitSyncEvent->Wait((DWORD)-1);

	GSynchronizeFactory->Destroy(ThreadInitSyncEvent);
	ThreadInitSyncEvent = NULL;
	return TRUE;
}

void FRunnableThreadMac::Suspend(UBOOL bShouldPause)
{
	(void)bShouldPause;
}

UBOOL FRunnableThreadMac::Kill(UBOOL bShouldWait)
{
	if (Runnable)
	{
		Runnable->Stop();
	}
	if (bShouldWait && Thread)
	{
		pthread_join(Thread, NULL);
		Thread = NULL;
	}
	return TRUE;
}

void FRunnableThreadMac::WaitForCompletion()
{
	if (Thread)
	{
		pthread_join(Thread, NULL);
		Thread = NULL;
	}
}

DWORD FRunnableThreadMac::GetThreadID()
{
	return ThreadID;
}

/*-----------------------------------------------------------------------------
	FThreadFactoryMac
-----------------------------------------------------------------------------*/

FRunnableThread* FThreadFactoryMac::CreateThread(FRunnable* InRunnable, const TCHAR* ThreadName,
	UBOOL bAutoDeleteSelf, UBOOL bAutoDeleteRunnable,
	DWORD InStackSize, EThreadPriority InThreadPri)
{
	FRunnableThreadMac* Thread = new FRunnableThreadMac();
	if (!Thread->Create(InRunnable, ThreadName, bAutoDeleteSelf, bAutoDeleteRunnable, InStackSize, InThreadPri))
	{
		delete Thread;
		return NULL;
	}
	return Thread;
}

void FThreadFactoryMac::Destroy(FRunnableThread* InThread)
{
	delete InThread;
}

/*-----------------------------------------------------------------------------
	FQueuedThreadMac
-----------------------------------------------------------------------------*/

FQueuedThreadMac::FQueuedThreadMac()
	: DoWorkEvent(NULL)
	, Thread(NULL)
	, ThreadID(0)
	, TimeToDie(FALSE)
	, QueuedWork(NULL)
	, OwningThreadPool(NULL)
{
}

FQueuedThreadMac::~FQueuedThreadMac()
{
	if (Thread != NULL)
	{
		Kill(TRUE);
	}
}

void* FQueuedThreadMac::_ThreadProc(void* pThis)
{
	FQueuedThreadMac* Self = (FQueuedThreadMac*)pThis;
	Self->Run();
	return NULL;
}

void FQueuedThreadMac::Run()
{
	while (!TimeToDie)
	{
		DoWorkEvent->Wait();
		FQueuedWork* LocalQueuedWork = QueuedWork;
		QueuedWork = NULL;
		while (LocalQueuedWork)
		{
			LocalQueuedWork->DoThreadedWork();
			LocalQueuedWork = OwningThreadPool->ReturnToPoolOrGetNextJob(this);
		}
	}
}

UBOOL FQueuedThreadMac::Create(FQueuedThreadPool* InPool, DWORD ProcessorMask,
	DWORD InStackSize, EThreadPriority InThreadPri)
{
	(void)ProcessorMask;
	(void)InThreadPri;
	OwningThreadPool = InPool;
	DoWorkEvent = GSynchronizeFactory->CreateSynchEvent();
	if (!DoWorkEvent)
	{
		return FALSE;
	}

	pthread_attr_t Attr;
	pthread_attr_init(&Attr);
	DWORD StackSize = Max(InStackSize, (DWORD)(2 * 1024 * 1024));
	pthread_attr_setstacksize(&Attr, StackSize);

	int Err = pthread_create(&Thread, &Attr, _ThreadProc, this);
	pthread_attr_destroy(&Attr);

	if (Err != 0)
	{
		Thread = NULL;
		GSynchronizeFactory->Destroy(DoWorkEvent);
		DoWorkEvent = NULL;
		return FALSE;
	}

	ThreadID = (DWORD)(uintptr_t)Thread;
	return TRUE;
}

UBOOL FQueuedThreadMac::Kill(UBOOL bShouldWait, DWORD WaitTime)
{
	(void)WaitTime;
	TimeToDie = TRUE;
	if (DoWorkEvent)
	{
		DoWorkEvent->Trigger();
	}
	if (bShouldWait && Thread)
	{
		pthread_join(Thread, NULL);
		Thread = NULL;
	}
	if (DoWorkEvent)
	{
		GSynchronizeFactory->Destroy(DoWorkEvent);
		DoWorkEvent = NULL;
	}
	return TRUE;
}

void FQueuedThreadMac::DoWork(FQueuedWork* InWork)
{
	check(QueuedWork == NULL);
	QueuedWork = InWork;
	DoWorkEvent->Trigger();
}

/*-----------------------------------------------------------------------------
	FQueuedThreadPoolMac
-----------------------------------------------------------------------------*/

UBOOL FQueuedThreadPoolMac::Create(DWORD InNumQueuedThreads, DWORD ProcessorMask,
	DWORD StackSize, EThreadPriority ThreadPriority)
{
	SynchQueue = GSynchronizeFactory->CreateCriticalSection();
	UBOOL bWasSuccessful = TRUE;
	for (DWORD Count = 0; Count < InNumQueuedThreads; Count++)
	{
		FQueuedThread* Thread = new FQueuedThreadMac();
		if (Thread->Create(this, ProcessorMask, StackSize, ThreadPriority))
		{
			QueuedThreads.AddItem(Thread);
		}
		else
		{
			delete Thread;
			bWasSuccessful = FALSE;
			break;
		}
	}
	if (!bWasSuccessful)
	{
		Destroy();
	}
	return bWasSuccessful;
}

#endif // PLATFORM_MACOSX
