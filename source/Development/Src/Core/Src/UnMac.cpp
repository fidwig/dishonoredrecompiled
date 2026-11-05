/*=============================================================================
	UnMac.cpp: Unreal Mac platform implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if PLATFORM_MACOSX

#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <unistd.h>
#include <sched.h>

DOUBLE appInitTiming()
{
	mach_timebase_info_data_t Info;
	mach_timebase_info(&Info);
	GSecondsPerCycle = ((DOUBLE)Info.numer / (DOUBLE)Info.denom) * 1.0e-9;
	return appSeconds();
}

void appPlatformPreInit()
{
	appInitTiming();
}

void appPlatformInit()
{
}

void appSleep(FLOAT Seconds)
{
	if (Seconds <= 0.0f)
	{
		sched_yield();
	}
	else
	{
		usleep((useconds_t)(Seconds * 1000000.0f));
	}
}

void appOutputDebugString(const TCHAR* Message)
{
	if (Message)
	{
		fputws(Message, stderr);
	}
}

void appGetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, INT ResultLength)
{
	if (!VariableName || !Result || ResultLength <= 0) return;
	const char* AnsiName = TCHAR_TO_ANSI(VariableName);
	const char* Value = getenv(AnsiName);
	if (Value)
	{
		appStrncpy(Result, ANSI_TO_TCHAR(Value), ResultLength);
	}
	else
	{
		Result[0] = 0;
	}
}

void appMacWideCharToMultiByte(DWORD CodePage, const TCHAR* Source, INT SourceLen, ANSICHAR* Dest, INT DestLen)
{
	(void)CodePage;
	(void)SourceLen;
	if (!Source || !Dest || DestLen <= 0) return;
	size_t Converted = wcstombs(Dest, Source, (size_t)DestLen);
	if (Converted == (size_t)-1)
	{
		INT i = 0;
		for (; Source[i] && i < DestLen - 1; ++i)
		{
			Dest[i] = (ANSICHAR)(Source[i] & 0xFF);
		}
		Dest[i] = 0;
	}
	else if (Converted < (size_t)DestLen)
	{
		Dest[Converted] = 0;
	}
	else
	{
		Dest[DestLen - 1] = 0;
	}
}

void appMacMultiByteToWideChar(DWORD CodePage, const ANSICHAR* Source, TCHAR* Dest, INT DestLen)
{
	(void)CodePage;
	if (!Source || !Dest || DestLen <= 0) return;
	size_t Converted = mbstowcs(Dest, Source, (size_t)DestLen);
	if (Converted == (size_t)-1)
	{
		INT i = 0;
		for (; Source[i] && i < DestLen - 1; ++i)
		{
			Dest[i] = (TCHAR)(BYTE)Source[i];
		}
		Dest[i] = 0;
	}
	else if (Converted < (size_t)DestLen)
	{
		Dest[Converted] = 0;
	}
	else
	{
		Dest[DestLen - 1] = 0;
	}
}

INT appGetVarArgs( TCHAR* Dest, SIZE_T DestSize, INT Count, const TCHAR*& Fmt, va_list ArgPtr )
{
	(void)Count;
	if (!Dest || DestSize == 0 || !Fmt) return 0;

	// Convert Windows-style format specifiers to POSIX-style for wchar_t vswprintf
	TCHAR ConvertedFmt[4096];
	TCHAR* Out = ConvertedFmt;
	const TCHAR* In = Fmt;
	const TCHAR* End = ConvertedFmt + ARRAY_COUNT(ConvertedFmt) - 4;

	while (*In && Out < End)
	{
		if (*In == '%')
		{
			*Out++ = *In++;
			if (*In == '%')
			{
				*Out++ = *In++;
				continue;
			}
			const TCHAR* Start = In;
			while (*In && !wcschr(TEXT("sdifFeEgGoxXucpnS"), *In))
			{
				In++;
			}
			if (*In)
			{
				const TCHAR* P = Start;
				while (P < In)
				{
					if (P + 2 < In && P[0] == 'I' && P[1] == '6' && P[2] == '4')
					{
						*Out++ = 'l';
						*Out++ = 'l';
						P += 3;
					}
					else if (P + 2 < In && P[0] == 'I' && P[1] == '3' && P[2] == '2')
					{
						P += 3;
					}
					else
					{
						*Out++ = *P++;
					}
				}
				if (*In == 's' && (P == Start || *(In - 1) != 'l'))
				{
					*Out++ = 'l';
					*Out++ = 's';
				}
				else if (*In == 'S')
				{
					*Out++ = 's';
				}
				else
				{
					*Out++ = *In;
				}
				In++;
				continue;
			}
		}
		*Out++ = *In++;
	}
	*Out = 0;

	INT Result = vswprintf( Dest, DestSize, ConvertedFmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}

INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T DestSize, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr )
{
	(void)Count;
	INT Result = vsnprintf( Dest, DestSize, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}

VARARG_BODY( UBOOL VARARGS, appMsgf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) )
{
	(void)Type;
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
	wprintf(TEXT("%ls\n"), TempStr);
	return TRUE;
}

void appSendNotificationString(const ANSICHAR* Message)
{
	(void)Message;
}

void appOnFailSHAVerification(const TCHAR* Filename, DWORD Flags)
{
	(void)Filename; (void)Flags;
}

void appHandleIOFailure(const TCHAR* Filename)
{
	appErrorf(TEXT("I/O failure with file '%ls'"), Filename ? Filename : TEXT("unknown"));
}

void appRequestExit(UBOOL Force)
{
	if (Force)
	{
		exit(1);
	}
	else
	{
		GIsRequestingExit = 1;
	}
}

const TCHAR* appBaseDir()
{
	static TCHAR BaseDir[MAX_PATH] = TEXT("");
	if (!BaseDir[0])
	{
		char Buffer[MAX_PATH];
		uint32_t Size = sizeof(Buffer);
		if (_NSGetExecutablePath(Buffer, &Size) == 0)
		{
			char Resolved[PATH_MAX];
			if (realpath(Buffer, Resolved))
			{
				char* Slash = strrchr(Resolved, '/');
				if (Slash)
				{
					*(Slash + 1) = '\0';
				}
				appStrncpy(BaseDir, ANSI_TO_TCHAR(Resolved), ARRAY_COUNT(BaseDir));
			}
		}
		if (!BaseDir[0])
		{
			getcwd(Buffer, sizeof(Buffer));
			appStrncpy(BaseDir, ANSI_TO_TCHAR(Buffer), ARRAY_COUNT(BaseDir));
			appStrcat(BaseDir, TEXT("/"));
		}
	}
	return BaseDir;
}

FGuid appCreateGuid()
{
	static INT GuidCounter = 1;
	FGuid Result;
	Result.A = (DWORD)mach_absolute_time();
	Result.B = (DWORD)getpid();
	Result.C = GuidCounter++;
	Result.D = (DWORD)rand();
	return Result;
}

void appLaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	(void)URL; (void)Parms;
	if (Error) *Error = TEXT("Not supported on macOS");
}

FString appGetLanguageExt(void)
{
	return FString(TEXT("INT"));
}

FString appGetLanguageExt(UBOOL bDisallowDiskAccess)
{
	(void)bDisallowDiskAccess;
	return FString(TEXT("INT"));
}

void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	struct timeval TV;
	gettimeofday(&TV, NULL);
	struct tm TM;
	localtime_r(&TV.tv_sec, &TM);
	Year      = TM.tm_year + 1900;
	Month     = TM.tm_mon + 1;
	DayOfWeek = TM.tm_wday;
	Day       = TM.tm_mday;
	Hour      = TM.tm_hour;
	Min       = TM.tm_min;
	Sec       = TM.tm_sec;
	MSec      = (INT)(TV.tv_usec / 1000);
}

void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	struct timeval TV;
	gettimeofday(&TV, NULL);
	struct tm TM;
	gmtime_r(&TV.tv_sec, &TM);
	Year      = TM.tm_year + 1900;
	Month     = TM.tm_mon + 1;
	DayOfWeek = TM.tm_wday;
	Day       = TM.tm_mday;
	Hour      = TM.tm_hour;
	Min       = TM.tm_min;
	Sec       = TM.tm_sec;
	MSec      = (INT)(TV.tv_usec / 1000);
}

void appBeginNamedEvent(const FColor& Color, const TCHAR* Text)
{
	(void)Color; (void)Text;
}

void appEndNamedEvent()
{
}

UBOOL appInitStackWalking()
{
	return TRUE;
}

void appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, void* Context )
{
	(void)Context;
	if (!BackTrace || MaxDepth == 0) return;
	void* Ptrs[128];
	DWORD Depth = (DWORD)backtrace(Ptrs, (int)Min<DWORD>(MaxDepth, 128));
	for (DWORD i = 0; i < Depth; i++)
	{
		BackTrace[i] = (QWORD)(uintptr_t)Ptrs[i];
	}
	for (DWORD i = Depth; i < MaxDepth; i++)
	{
		BackTrace[i] = 0;
	}
}

UBOOL appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags )
{
	(void)VerbosityFlags;
	Dl_info Info;
	if (dladdr((void*)(uintptr_t)ProgramCounter, &Info) && Info.dli_sname)
	{
		snprintf(HumanReadableString, HumanReadableStringSize, "%s (0x%llx in %s)", Info.dli_sname, ProgramCounter, Info.dli_fname ? Info.dli_fname : "?");
		return TRUE;
	}
	snprintf(HumanReadableString, HumanReadableStringSize, "0x%llx", ProgramCounter);
	return FALSE;
}

FProgramCounterSymbolInfo appProgramCounterToSymbolInfo( QWORD ProgramCounter )
{
	FProgramCounterSymbolInfo Result;
	memset(&Result, 0, sizeof(Result));
	Dl_info Info;
	if (dladdr((void*)(uintptr_t)ProgramCounter, &Info))
	{
		if (Info.dli_sname) strncpy(Result.FunctionName, Info.dli_sname, sizeof(Result.FunctionName) - 1);
		if (Info.dli_fname) strncpy(Result.ModuleName, Info.dli_fname, sizeof(Result.ModuleName) - 1);
	}
	return Result;
}

void appStackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, INT IgnoreCount, void* Context )
{
	QWORD BackTrace[32];
	appCaptureStackBackTrace(BackTrace, 32, Context);
	HumanReadableString[0] = 0;
	for (INT i = IgnoreCount; i < 32 && BackTrace[i]; i++)
	{
		ANSICHAR Line[512];
		appProgramCounterToHumanReadableString(BackTrace[i], Line, sizeof(Line));
		strncat(HumanReadableString, Line, HumanReadableStringSize - strlen(HumanReadableString) - 1);
		strncat(HumanReadableString, "\n", HumanReadableStringSize - strlen(HumanReadableString) - 1);
	}
}

void appDumpCallStackToLog( INT IgnoreCount )
{
	ANSICHAR Stack[4096];
	appStackWalkAndDump(Stack, sizeof(Stack), IgnoreCount + 1);
	debugf(TEXT("Callstack:\n%s"), ANSI_TO_TCHAR(Stack));
}

VARARG_BODY( void VARARGS, EdLoadErrorf, const TCHAR*, VARARG_EXTRA(INT Type) )
{
	(void)Type;
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
	GErrorHist[0] = 0;
	appStrncpy( GErrorHist, TempStr, ARRAY_COUNT(GErrorHist) );
}

void appSetConsoleInterruptHandler()
{
}

void appMacPumpMessages()
{
}

void appPlatformPostInit()
{
}

const TCHAR* appUserName()
{
	static TCHAR Result[256] = TEXT("");
	if (!Result[0])
	{
		const char* User = getenv("USER");
		if (!User) User = getlogin();
		if (User)
		{
			appStrncpy(Result, ANSI_TO_TCHAR(User), ARRAY_COUNT(Result));
		}
		else
		{
			appStrcpy(Result, TEXT("Corvo"));
		}
	}
	return Result;
}

const TCHAR* appComputerName()
{
	static TCHAR Result[256] = TEXT("");
	if (!Result[0])
	{
		char Hostname[256] = {0};
		if (gethostname(Hostname, sizeof(Hostname)) == 0)
		{
			appStrncpy(Result, ANSI_TO_TCHAR(Hostname), ARRAY_COUNT(Result));
		}
		else
		{
			appStrcpy(Result, TEXT("Mac"));
		}
	}
	return Result;
}

static TCHAR GShaderDir[1024] = TEXT("..") PATH_SEPARATOR TEXT("..") PATH_SEPARATOR TEXT("Engine") PATH_SEPARATOR TEXT("Shaders");
const TCHAR* appShaderDir()
{
	return GShaderDir;
}

void appSetShaderDir(const TCHAR* Where)
{
	if (Where)
	{
		appStrncpy(GShaderDir, Where, ARRAY_COUNT(GShaderDir));
	}
}

const TCHAR* appExecutableName()
{
	static TCHAR Result[512] = TEXT("");
	if (!Result[0])
	{
		char Buffer[PATH_MAX];
		uint32_t Size = sizeof(Buffer);
		if (_NSGetExecutablePath(Buffer, &Size) == 0)
		{
			char Resolved[PATH_MAX];
			if (realpath(Buffer, Resolved))
			{
				char* Slash = strrchr(Resolved, '/');
				const char* Name = Slash ? Slash + 1 : Resolved;
				appStrncpy(Result, ANSI_TO_TCHAR(Name), ARRAY_COUNT(Result));
			}
		}
		if (!Result[0])
		{
			appStrcpy(Result, TEXT("DishonoredGame"));
		}
	}
	return Result;
}

void* appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached, UBOOL bLaunchHidden, UBOOL bLaunchReallyHidden, DWORD* OutProcessID, INT PriorityModifier )
{
	(void)URL;
	(void)Parms;
	(void)bLaunchDetached;
	(void)bLaunchHidden;
	(void)bLaunchReallyHidden;
	(void)PriorityModifier;
	if (OutProcessID) *OutProcessID = 0;
	return NULL;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	(void)ProcHandle;
	if (ReturnCode) *ReturnCode = 0;
	return FALSE;
}

UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	(void)ProcessId;
	return FALSE;
}

static FString GClipboardString;

void appClipboardCopy( const TCHAR* Str )
{
	if (Str)
	{
		GClipboardString = Str;
	}
	else
	{
		GClipboardString.Empty();
	}
}

FString appClipboardPaste()
{
	return GClipboardString;
}

void appSleepInfinite()
{
	while (1)
	{
		pause();
	}
}

void appCleanFileCache()
{
}

void appShowSplash( const TCHAR* InSplashName )
{
	(void)InSplashName;
}

void appHideSplash()
{
}

void appShowGameWindow()
{
}

void appPreventScreenSaver()
{
}

void appUpdateMemoryChartStats( struct FMemoryChartEntry& MemoryEntry )
{
	(void)MemoryEntry;
}

void* appMacUTF32ToUTF16(const unsigned int* Source, unsigned short* Dest, int Length)
{
	if (!Source || !Dest) return Dest;
	for (int i = 0; i < Length; ++i)
	{
		unsigned int c = Source[i];
		Dest[i] = (c < 0x10000) ? (unsigned short)c : (unsigned short)'?';
	}
	return Dest;
}

#endif // PLATFORM_MACOSX
