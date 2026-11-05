/*=============================================================================
	FFileManagerMac.h: Unreal Mac OS based file manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FFILEMANAGERMAC_H__
#define __FFILEMANAGERMAC_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
	FFileManagerMac
-----------------------------------------------------------------------------*/

class FFileManagerMac : public FFileManagerGeneric
{
public:
	virtual void Init(UBOOL Startup);

	virtual UBOOL SetDefaultDirectory();
	virtual UBOOL SetCurDirectory(const TCHAR* Directory);
	virtual FString GetCurrentDirectory();

	virtual FArchive* CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error );
	virtual FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize );
	virtual INT	UncompressedFileSize( const TCHAR* Filename );
	virtual DWORD Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress );
	virtual UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 );
	virtual UBOOL IsReadOnly( const TCHAR* Filename );
	virtual UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
	virtual UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
	virtual UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
	virtual void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories );
	virtual DOUBLE GetFileAgeSeconds( const TCHAR* Filename );
	virtual DOUBLE GetFileTimestamp( const TCHAR* Filename );
	virtual UBOOL GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp );
	virtual UBOOL TouchFile(const TCHAR* Filename);

	virtual FString ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath);
	virtual FString ConvertToRelativePath( const TCHAR* Filename );
	virtual FString ConvertToAbsolutePath( const TCHAR* Filename );

	virtual INT FileSize( const TCHAR* Filename );
	virtual UBOOL GetDiskFreeSpace(const TCHAR* Drive, QWORD& FreeBytesToCaller, QWORD& TotalBytes, QWORD& FreeBytes );
};

#endif // __FFILEMANAGERMAC_H__
