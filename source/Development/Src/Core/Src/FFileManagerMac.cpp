/*=============================================================================
	FFileManagerMac.cpp: Unreal Mac OS based file manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if PLATFORM_MACOSX

#include "FFileManagerMac.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <utime.h>
#include <errno.h>

/*-----------------------------------------------------------------------------
	FArchiveFileReaderMac
-----------------------------------------------------------------------------*/

class FArchiveFileReaderMac : public FArchive
{
public:
	FArchiveFileReaderMac( FILE* InFile, FOutputDevice* InError, INT InSize )
		: File( InFile )
		, Error( InError )
		, Size( InSize )
		, Pos( 0 )
		, BufferBase( 0 )
		, BufferCount( 0 )
	{
		ArIsLoading = ArIsPersistent = 1;
	}

	virtual ~FArchiveFileReaderMac()
	{
		Close();
	}

	virtual void Seek( INT InPos )
	{
		check(InPos >= 0);
		check(InPos <= Size);
		if (fseek(File, InPos, SEEK_SET) != 0)
		{
			ArIsError = 1;
			Error->Logf(TEXT("fseek failed to pos %d / %d (errno %d)"), InPos, Size, errno);
		}
		Pos = InPos;
		BufferBase = Pos;
		BufferCount = 0;
	}

	virtual INT Tell()
	{
		return Pos;
	}

	virtual INT TotalSize()
	{
		return Size;
	}

	virtual UBOOL Close()
	{
		if (File)
		{
			fclose(File);
			File = NULL;
		}
		return !ArIsError;
	}

	virtual void Serialize( void* V, INT Length )
	{
		while (Length > 0)
		{
			INT Copy = Min(Length, BufferBase + BufferCount - Pos);
			if (Copy == 0)
			{
				if (Length >= (INT)sizeof(Buffer))
				{
					if (fread(V, Length, 1, File) != 1)
					{
						ArIsError = 1;
						Error->Logf(TEXT("fread failed: Length=%d errno=%d"), Length, errno);
					}
					Pos += Length;
					BufferBase += Length;
					return;
				}
				Precache(MAXINT);
				Copy = Min(Length, BufferBase + BufferCount - Pos);
				if (Copy <= 0)
				{
					ArIsError = 1;
					break;
				}
			}
			memcpy(V, Buffer + Pos - BufferBase, Copy);
			Pos += Copy;
			Length -= Copy;
			V = (BYTE*)V + Copy;
		}
	}

protected:
	void Precache( INT HintCount )
	{
		BufferBase = Pos;
		BufferCount = Min(Min(HintCount, (INT)(sizeof(Buffer) - (Pos & (sizeof(Buffer) - 1)))), Size - Pos);
		BufferCount = Max(BufferCount, 0);
		if (BufferCount > 0)
		{
			size_t Read = fread(Buffer, 1, BufferCount, File);
			if ((INT)Read != BufferCount)
			{
				ArIsError = 1;
				Error->Logf(TEXT("fread precache failed: Read=%d BufferCount=%d errno=%d"), (int)Read, BufferCount, errno);
			}
		}
	}

	FILE*          File;
	FOutputDevice* Error;
	INT            Size;
	INT            Pos;
	INT            BufferBase;
	INT            BufferCount;
	BYTE           Buffer[4096];
};

/*-----------------------------------------------------------------------------
	FArchiveFileWriterMac
-----------------------------------------------------------------------------*/

class FArchiveFileWriterMac : public FArchive
{
public:
	FArchiveFileWriterMac( FILE* InFile, FOutputDevice* InError )
		: File( InFile )
		, Error( InError )
		, Pos( 0 )
		, BufferCount( 0 )
	{
		ArIsSaving = ArIsPersistent = 1;
	}

	virtual ~FArchiveFileWriterMac()
	{
		Close();
	}

	virtual void Seek( INT InPos )
	{
		Flush();
		if (fseek(File, InPos, SEEK_SET) != 0)
		{
			ArIsError = 1;
			Error->Logf(TEXT("fseek failed to pos %d (errno %d)"), InPos, errno);
		}
		Pos = InPos;
	}

	virtual INT Tell()
	{
		return Pos;
	}

	virtual UBOOL Close()
	{
		Flush();
		if (File)
		{
			fclose(File);
			File = NULL;
		}
		return !ArIsError;
	}

	virtual void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy = Min(Length, (INT)sizeof(Buffer) - BufferCount);
		memcpy(Buffer + BufferCount, V, Copy);
		BufferCount += Copy;
		Length -= Copy;
		V = (BYTE*)V + Copy;
		if (BufferCount == (INT)sizeof(Buffer))
		{
			Flush();
			if (Length >= (INT)sizeof(Buffer))
			{
				if (fwrite(V, Length, 1, File) != 1)
				{
					ArIsError = 1;
					Error->Logf(TEXT("fwrite failed: Length=%d errno=%d"), Length, errno);
				}
				Length = 0;
			}
			if (Length > 0)
			{
				memcpy(Buffer, V, Length);
				BufferCount = Length;
			}
		}
	}

	virtual void Flush()
	{
		if (BufferCount > 0)
		{
			if (fwrite(Buffer, BufferCount, 1, File) != 1)
			{
				ArIsError = 1;
				Error->Logf(TEXT("fwrite flush failed: BufferCount=%d errno=%d"), BufferCount, errno);
			}
			BufferCount = 0;
		}
	}

protected:
	FILE*          File;
	FOutputDevice* Error;
	INT            Pos;
	INT            BufferCount;
	BYTE           Buffer[4096];
};

/*-----------------------------------------------------------------------------
	FFileManagerMac implementation
-----------------------------------------------------------------------------*/

static inline FString MacNormalizePath(const TCHAR* InPath)
{
	if (!InPath)
	{
		return FString(TEXT(""));
	}
	FString Result(InPath);
	for (INT i = 0; i < Result.Len(); ++i)
	{
		if (Result[i] == TEXT('\\'))
		{
			Result[i] = TEXT('/');
		}
	}
	return Result;
}

void FFileManagerMac::Init(UBOOL Startup)
{
	FFileManagerGeneric::Init(Startup);
}

UBOOL FFileManagerMac::SetDefaultDirectory()
{
	return chdir(TCHAR_TO_ANSI(appBaseDir())) == 0;
}

UBOOL FFileManagerMac::SetCurDirectory(const TCHAR* Directory)
{
	FString NormDir = MacNormalizePath(Directory);
	return chdir(TCHAR_TO_ANSI(*NormDir)) == 0;
}

FString FFileManagerMac::GetCurrentDirectory()
{
	char Buffer[PATH_MAX];
	if (getcwd(Buffer, sizeof(Buffer)))
	{
		return FString(ANSI_TO_TCHAR(Buffer));
	}
	return FString(TEXT(""));
}

FArchive* FFileManagerMac::CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error )
{
	FString NormFilename = MacNormalizePath(InFilename);
	FTCHARToANSI AnsiPath(*NormFilename);
	struct stat St;
	if (stat(AnsiPath, &St) != 0 || S_ISDIR(St.st_mode))
	{
		if (Flags & FILEREAD_NoFail)
		{
			appErrorf(TEXT("Failed to read file: %s"), *NormFilename);
		}
		return NULL;
	}
	FILE* File = fopen(AnsiPath, "rb");
	if (!File)
	{
		if (Flags & FILEREAD_NoFail)
		{
			appErrorf(TEXT("Failed to read file: %s"), *NormFilename);
		}
		return NULL;
	}
	fseek(File, 0, SEEK_END);
	long Size = ftell(File);
	fseek(File, 0, SEEK_SET);
	return new FArchiveFileReaderMac(File, Error, (INT)Size);
}

FArchive* FFileManagerMac::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	(void)MaxFileSize;
	FString NormFilename = MacNormalizePath(Filename);
	if ((Flags & FILEWRITE_NoReplaceExisting) && FileSize(*NormFilename) >= 0)
	{
		return NULL;
	}
	FTCHARToANSI AnsiPath(*NormFilename);
	const char* Mode = (Flags & FILEWRITE_Append) ? "ab" : "wb";
	FILE* File = fopen(AnsiPath, Mode);
	if (!File)
	{
		if (Flags & FILEWRITE_NoFail)
		{
			appErrorf(TEXT("Failed to write file: %s"), *NormFilename);
		}
		return NULL;
	}
	return new FArchiveFileWriterMac(File, Error);
}

INT FFileManagerMac::FileSize( const TCHAR* Filename )
{
	FString NormFilename = MacNormalizePath(Filename);
	FTCHARToANSI AnsiPath(*NormFilename);
	struct stat St;
	if (stat(AnsiPath, &St) == 0 && !S_ISDIR(St.st_mode))
	{
		return (INT)St.st_size;
	}
	return INDEX_NONE;
}

INT FFileManagerMac::UncompressedFileSize( const TCHAR* Filename )
{
	// default to not using the uncompressed file size
	INT Result = -1;
	// check for the presence of a .uncompressed_size manifest file which mirrors the Filename
	// if it exists, then the file was fully compressed, so we get the original uncompressed size for it
	FString UnCompressedSizeFilename = FString(Filename) + FString(TEXT(".uncompressed_size"));
	if( FileSize(*UnCompressedSizeFilename) != -1 )
	{
		FString SizeString;
		appLoadFileToString(SizeString, *UnCompressedSizeFilename);
		if( SizeString.Len() )
		{
			Result = appAtoi(*SizeString);
		}
	}
	return Result;
}

DWORD FFileManagerMac::Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
	FString NormDest = MacNormalizePath(DestFile);
	FString NormSrc = MacNormalizePath(SrcFile);
	return FFileManagerGeneric::Copy(*NormDest, *NormSrc, ReplaceExisting, EvenIfReadOnly, Attributes, Progress);
}

UBOOL FFileManagerMac::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	(void)EvenReadOnly;
	FString NormFilename = MacNormalizePath(Filename);
	int Err = unlink(TCHAR_TO_ANSI(*NormFilename));
	return Err == 0 || (errno == ENOENT && !RequireExists);
}

UBOOL FFileManagerMac::IsReadOnly( const TCHAR* Filename )
{
	FString NormFilename = MacNormalizePath(Filename);
	return access(TCHAR_TO_ANSI(*NormFilename), W_OK) != 0;
}

UBOOL FFileManagerMac::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	(void)EvenIfReadOnly; (void)Attributes;
	FString NormDest = MacNormalizePath(Dest);
	FString NormSrc = MacNormalizePath(Src);
	if (!Replace && FileSize(*NormDest) >= 0)
	{
		return FALSE;
	}
	return rename(TCHAR_TO_ANSI(*NormSrc), TCHAR_TO_ANSI(*NormDest)) == 0;
}

UBOOL FFileManagerMac::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	FString NormPath = MacNormalizePath(Path);
	if (Tree)
	{
		return FFileManagerGeneric::MakeDirectory(*NormPath, Tree);
	}
	return mkdir(TCHAR_TO_ANSI(*NormPath), 0755) == 0 || errno == EEXIST;
}

UBOOL FFileManagerMac::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	FString NormPath = MacNormalizePath(Path);
	if (Tree)
	{
		return FFileManagerGeneric::DeleteDirectory(*NormPath, RequireExists, Tree);
	}
	return rmdir(TCHAR_TO_ANSI(*NormPath)) == 0 || (errno == ENOENT && !RequireExists);
}

void FFileManagerMac::FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	FString PathStr = MacNormalizePath(Filename);
	INT LastSlash = PathStr.InStr(TEXT("/"), TRUE);
	FString DirPath = (LastSlash != INDEX_NONE) ? PathStr.Left(LastSlash) : TEXT(".");
	FString Pattern = (LastSlash != INDEX_NONE) ? PathStr.Mid(LastSlash + 1) : PathStr;

	FTCHARToANSI AnsiDirPath(*DirPath);
	DIR* Dir = opendir(AnsiDirPath);
	if (!Dir)
	{
		return;
	}

	FTCHARToANSI AnsiPattern(*Pattern);
	struct dirent* Entry = NULL;
	while ((Entry = readdir(Dir)) != NULL)
	{
		if (strcmp(Entry->d_name, ".") == 0 || strcmp(Entry->d_name, "..") == 0)
		{
			continue;
		}

		UBOOL bIsDir = (Entry->d_type == DT_DIR);
		if (Entry->d_type == DT_UNKNOWN || Entry->d_type == DT_LNK)
		{
			FString FullPath = DirPath * ANSI_TO_TCHAR(Entry->d_name);
			struct stat St;
			if (stat(TCHAR_TO_ANSI(*MacNormalizePath(*FullPath)), &St) == 0)
			{
				bIsDir = S_ISDIR(St.st_mode);
			}
		}

		if ((bIsDir && Directories) || (!bIsDir && Files))
		{
			if (strcmp(AnsiPattern, "*.*") == 0 || strcmp(AnsiPattern, "*") == 0 || fnmatch(AnsiPattern, Entry->d_name, FNM_CASEFOLD) == 0)
			{
				Result.AddItem(ANSI_TO_TCHAR(Entry->d_name));
			}
		}
	}
	closedir(Dir);
}

DOUBLE FFileManagerMac::GetFileAgeSeconds( const TCHAR* Filename )
{
	FString NormFilename = MacNormalizePath(Filename);
	struct stat St;
	if (stat(TCHAR_TO_ANSI(*NormFilename), &St) == 0)
	{
		time_t Now = time(NULL);
		return difftime(Now, St.st_mtime);
	}
	return -1.0;
}

DOUBLE FFileManagerMac::GetFileTimestamp( const TCHAR* Filename )
{
	FString NormFilename = MacNormalizePath(Filename);
	struct stat St;
	if (stat(TCHAR_TO_ANSI(*NormFilename), &St) == 0)
	{
		return (DOUBLE)St.st_mtime;
	}
	return -1.0;
}

UBOOL FFileManagerMac::GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	FString NormFilename = MacNormalizePath(Filename);
	struct stat St;
	if (stat(TCHAR_TO_ANSI(*NormFilename), &St) == 0)
	{
		struct tm TM;
		localtime_r(&St.st_mtime, &TM);
		Timestamp.Year      = TM.tm_year + 1900;
		Timestamp.Month     = TM.tm_mon + 1;
		Timestamp.Day       = TM.tm_mday;
		Timestamp.Hour      = TM.tm_hour;
		Timestamp.Minute    = TM.tm_min;
		Timestamp.Second    = TM.tm_sec;
		Timestamp.DayOfWeek = TM.tm_wday;
		return TRUE;
	}
	return FALSE;
}

UBOOL FFileManagerMac::TouchFile(const TCHAR* Filename)
{
	FString NormFilename = MacNormalizePath(Filename);
	return utimes(TCHAR_TO_ANSI(*NormFilename), NULL) == 0;
}

FString FFileManagerMac::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	return FString(AbsolutePath);
}

FString FFileManagerMac::ConvertToRelativePath( const TCHAR* Filename )
{
	return FFileManagerGeneric::ConvertToRelativePath(Filename);
}

FString FFileManagerMac::ConvertToAbsolutePath( const TCHAR* Filename )
{
	return FFileManagerGeneric::ConvertToAbsolutePath(Filename);
}

UBOOL FFileManagerMac::GetDiskFreeSpace(const TCHAR* Drive, QWORD& FreeBytesToCaller, QWORD& TotalBytes, QWORD& FreeBytes )
{
	struct statvfs St;
	if (statvfs(TCHAR_TO_ANSI(Drive), &St) == 0)
	{
		FreeBytesToCaller = (QWORD)St.f_bavail * (QWORD)St.f_frsize;
		FreeBytes         = (QWORD)St.f_bfree * (QWORD)St.f_frsize;
		TotalBytes        = (QWORD)St.f_blocks * (QWORD)St.f_frsize;
		return TRUE;
	}
	return FALSE;
}

#endif // PLATFORM_MACOSX
