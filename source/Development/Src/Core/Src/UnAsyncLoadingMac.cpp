/*=============================================================================
	UnAsyncLoadingMac.cpp: Unreal async loading code, Mac implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if PLATFORM_MACOSX

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

UBOOL FAsyncIOSystemMac::PlatformReadDoNotCallDirectly( FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest )
{
	int fd = (int)(intptr_t)FileHandle.Handle;
	if (fd < 0)
	{
		return FALSE;
	}

	if (Offset != INDEX_NONE)
	{
		if (lseek(fd, (off_t)Offset, SEEK_SET) == (off_t)-1)
		{
			return FALSE;
		}
	}

	ssize_t BytesRead = read(fd, Dest, (size_t)Size);
	return BytesRead == (ssize_t)Size;
}

FAsyncIOHandle FAsyncIOSystemMac::PlatformCreateHandle( const TCHAR* FileName )
{
	FAsyncIOHandle Handle;
	FString Normalized(FileName);
	for (INT i = 0; i < Normalized.Len(); ++i)
	{
		if (Normalized[i] == TEXT('\\'))
		{
			Normalized[i] = TEXT('/');
		}
	}
	FTCHARToANSI AnsiPath(*Normalized);
	int fd = open(AnsiPath, O_RDONLY);
	if (fd >= 0)
	{
		Handle.Handle = (void*)(intptr_t)fd;
	}
	else
	{
		Handle.Handle = (void*)(intptr_t)-1;
	}
	return Handle;
}

void FAsyncIOSystemMac::PlatformDestroyHandle( FAsyncIOHandle FileHandle )
{
	int fd = (int)(intptr_t)FileHandle.Handle;
	if (fd >= 0)
	{
		close(fd);
	}
}

UBOOL FAsyncIOSystemMac::PlatformIsHandleValid( FAsyncIOHandle FileHandle )
{
	int fd = (int)(intptr_t)FileHandle.Handle;
	return fd >= 0;
}

#endif // PLATFORM_MACOSX
