/*=============================================================================
	Mac.h: Unreal definitions for Apple Clang running under macOS (Apple Silicon / arm64).
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __INC_MAC_H__
#define __INC_MAC_H__

#ifndef PLATFORM_MACOSX
#define PLATFORM_MACOSX 1
#endif

#ifndef PLATFORM_UNIX
#define PLATFORM_UNIX 1
#endif

#ifndef PLATFORM_DESKTOP
#define PLATFORM_DESKTOP 1
#endif

#define __INTEL_BYTE_ORDER__ 1
#define __APPLE_BYTE_ORDER__ 1

/*----------------------------------------------------------------------------
	Standard C / POSIX includes.
----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <wchar.h>
#include <wctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <dlfcn.h>
#include <signal.h>
#include <limits.h>
#include <alloca.h>

#if !defined(DISHONORED_LAYOUT_CHECKS) || !DISHONORED_LAYOUT_CHECKS
// Disable 32-bit Win32 PDB static_assert layout assertions on 64-bit macOS
#define static_assert(...)
#endif

#include "UMemoryDefines.h"

/*----------------------------------------------------------------------------
	Platform specific types.
----------------------------------------------------------------------------*/

// Unsigned base types.
typedef uint8_t		BYTE;		// 8-bit  unsigned.
typedef uint16_t	WORD;		// 16-bit unsigned.
typedef uint32_t	UINT;		// 32-bit unsigned.
typedef uint32_t	DWORD;		// 32-bit unsigned.
typedef uint64_t	QWORD;		// 64-bit unsigned.

// Signed base types.
typedef int8_t		SBYTE;		// 8-bit  signed.
typedef int16_t		SWORD;		// 16-bit signed.
typedef int32_t 	INT;		// 32-bit signed.
typedef int32_t		LONG;		// 32-bit signed.
typedef int64_t		SQWORD;		// 64-bit signed.

// Character types.
typedef char		ANSICHAR;	// An ANSI character.
typedef uint16_t	UNICHAR;	// A unicode character (UTF-16, 2 bytes).
typedef wchar_t		WCHAR;		// Wide character.
typedef wchar_t		TCHAR;		// Engine text character (UCS-4, 4 bytes on Mac).

#define _TCHAR_DEFINED 1
#define TCHAR_IS_4_BYTES 1

// Other base types.
typedef void		VOID;
typedef uint32_t	UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float		FLOAT;		// 32-bit IEEE floating point.
typedef double		DOUBLE;		// 64-bit IEEE double.

typedef intptr_t	PTRINT;		// Integer large enough to hold a pointer.
typedef uintptr_t	UPTRINT;	// Unsigned integer large enough to hold a pointer.
typedef size_t		SIZE_T;		// Memory size type.

// Bitfield type.
typedef uint32_t	BITFIELD;	// For bitfields.

/** Represents a serializable object pointer in UnrealScript. Always 64-bits. */
typedef uint64_t	ScriptPointerType;

#define DECLARE_UINT64(x)	x##ULL

/*----------------------------------------------------------------------------
	Windows compatibility shims for cross-platform UE3 code.
----------------------------------------------------------------------------*/

typedef void*		HANDLE;
typedef void*		HINSTANCE;
typedef void*		HMODULE;
typedef void*		HWND;
typedef long		HRESULT;
typedef intptr_t	LRESULT;
typedef uintptr_t	WPARAM;
typedef intptr_t	LPARAM;
#define APIENTRY

typedef struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT;

inline LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }

#define S_OK		((HRESULT)0L)
#define S_FALSE		((HRESULT)1L)
#define FAILED(hr)	(((HRESULT)(hr)) < 0)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MAX_PATH	1024

#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif

#define CP_ACP		0
#define CP_OEMCP	1
#define CP_UTF8		65001

/*----------------------------------------------------------------------------
	Calling conventions and compiler macros.
----------------------------------------------------------------------------*/

#define VARARGS
#define CDECL
#define STDCALL
#define FORCEINLINE		inline __attribute__((always_inline))
#define FORCENOINLINE	__attribute__((noinline))
#define ZEROARRAY
#define RESTRICT		__restrict__
#define ASSUME(expr)
#define OVERRIDE		override

#define COMPILER "Compiled with Apple Clang"
#define COMPILER_SUPPORTS_NOOP 0

// Alignment macros
#define GCC_PACK(n)		__attribute__((packed, aligned(n)))
#define GCC_ALIGN(n)	__attribute__((aligned(n)))
#define MS_ALIGN(n)		__attribute__((aligned(n)))

enum { DEFAULT_ALIGNMENT = 16 };
#define RENDER_DATA_ALIGNMENT DEFAULT_ALIGNMENT

#define PRAGMA_DISABLE_OPTIMIZATION _Pragma("clang optimize off")
#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("clang optimize on")

#define SUPPORTS_VARIADIC_MACROS 1

#ifndef NULL
#define NULL 0
#endif

/*----------------------------------------------------------------------------
	Math functions.
----------------------------------------------------------------------------*/

FORCEINLINE INT appRound( FLOAT F ) { return (INT)roundf(F); }
FORCEINLINE INT appFloor( FLOAT F ) { return (INT)floorf(F); }
FORCEINLINE INT appCeil( FLOAT F )  { return (INT)ceilf(F); }
FORCEINLINE INT appTrunc( FLOAT F ) { return (INT)truncf(F); }
FORCEINLINE FLOAT appTruncFloat( FLOAT F ) { return truncf(F); }

inline FLOAT appCopySign( FLOAT A, FLOAT B ) { return copysignf(A, B); }
inline FLOAT appExp( FLOAT Value ) { return expf(Value); }
inline FLOAT appLoge( FLOAT Value ) { return logf(Value); }
inline FLOAT appFmod( FLOAT Y, FLOAT X ) { return fmodf(Y, X); }
inline FLOAT appSin( FLOAT Value ) { return sinf(Value); }
inline FLOAT appAsin( FLOAT Value ) { return asinf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
inline FLOAT appCos( FLOAT Value ) { return cosf(Value); }
inline FLOAT appAcos( FLOAT Value ) { return acosf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
inline FLOAT appTan( FLOAT Value ) { return tanf(Value); }
inline FLOAT appAtan( FLOAT Value ) { return atanf(Value); }
inline FLOAT appAtan2( FLOAT Y, FLOAT X ) { return atan2f(Y, X); }
inline FLOAT appSqrt( FLOAT Value ) { return sqrtf(Value); }
inline FLOAT appPow( FLOAT A, FLOAT B ) { return powf(A, B); }
inline UBOOL appIsNaN( FLOAT A ) { return isnan(A) != 0; }
inline UBOOL appIsFinite( FLOAT A ) { return isfinite(A) != 0; }
inline FLOAT appInvSqrt( FLOAT F ) { return 1.0f / sqrtf(F); }
inline FLOAT appInvSqrtEst( FLOAT F ) { return 1.0f / sqrtf(F); }
inline FLOAT appFractional( FLOAT Value ) { return Value - truncf(Value); }

inline INT appRand() { return rand(); }
inline INT appRandRange(int Min, int Max) { return Min + (appRand() % (Max - Min + 1)); }
inline void appRandInit(INT Seed) { srand( Seed ); }
extern INT GSRandSeed;
inline void appSRandInit( INT Seed ) { GSRandSeed = Seed; }

inline DWORD appFloorLog2(DWORD Value)
{
	return Value ? (31 - __builtin_clz(Value)) : 0;
}

inline DWORD appCountLeadingZeros(DWORD Value)
{
	if (Value == 0) return 32;
	return __builtin_clz(Value);
}

/*----------------------------------------------------------------------------
	Timing functions.
----------------------------------------------------------------------------*/

#include <mach/mach_time.h>

#define DEFINED_appSeconds 1
#define DEFINED_appCycles  1

extern DOUBLE GSecondsPerCycle;

inline DWORD appCycles()
{
	return (DWORD)mach_absolute_time();
}

inline DOUBLE appSeconds()
{
	return (DOUBLE)mach_absolute_time() * GSecondsPerCycle;
}

// DLL / Dylib
#define DLLEXT TEXT(".dylib")
#define PATH(s) s
#define FORCE_ANSI_LOG 1

// Paths and lines
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("/")
#define appIsPathSeparator( Ch )	((Ch) == '/' || (Ch) == '\\')

/*----------------------------------------------------------------------------
	tchar.h / Wide-character compatibility for POSIX.
----------------------------------------------------------------------------*/

#define _tcslen       wcslen
#define _tcscmp       wcscmp
#define _tcsncmp      wcsncmp
#define _tcsicmp      wcscasecmp
#define _tcsnicmp     wcsncasecmp
#define _tcschr       wcschr
#define _tcsrchr      wcsrchr
#define _tcsstr       wcsstr
#define _tcscat       wcscat
#define _tcscpy       wcscpy
#define _tcsncpy      wcsncpy
#define _tcstod       wcstod
#define _tcstoul      wcstoul
#define _tcstoui64    wcstoull
#define _tcstoi64     wcstoll

inline int _tstoi( const wchar_t* str )       { return (int)wcstol(str, NULL, 10); }
inline int64_t _tstoi64( const wchar_t* str ) { return (int64_t)wcstoll(str, NULL, 10); }
inline float _tstof( const wchar_t* str )     { return wcstof(str, NULL); }

inline wchar_t* _tcsupr( wchar_t* str )
{
	if( str )
	{
		for( wchar_t* p = str; *p; ++p )
		{
			*p = towupper(*p);
		}
	}
	return str;
}

inline wchar_t* _tcslwr( wchar_t* str )
{
	if( str )
	{
		for( wchar_t* p = str; *p; ++p )
		{
			*p = towlower(*p);
		}
	}
	return str;
}

#define _stricmp      strcasecmp
#define _strnicmp     strncasecmp
#define _strdup       strdup
#define _wcsdup       wcsdup
#define _snprintf     snprintf
#define _vsnprintf    vsnprintf
#define _snwprintf    swprintf
#define _vsnwprintf   vswprintf
#define _stscanf      swscanf
#define _stscanf_s    swscanf

/*----------------------------------------------------------------------------
	Memory functions.
----------------------------------------------------------------------------*/

#define appAlloca(size) ((size == 0) ? 0 : alloca(((size) + 15) & ~15))

#include <malloc/malloc.h>

inline void* _aligned_malloc(size_t size, size_t alignment)
{
	void* ptr = NULL;
	if (alignment < sizeof(void*)) alignment = sizeof(void*);
	if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
	return ptr;
}

inline void _aligned_free(void* ptr)
{
	free(ptr);
}

inline void* _aligned_realloc(void* ptr, size_t size, size_t alignment)
{
	if (!ptr) return _aligned_malloc(size, alignment);
	if (size == 0) { _aligned_free(ptr); return NULL; }
	void* new_ptr = _aligned_malloc(size, alignment);
	if (new_ptr) {
		size_t old_size = malloc_size(ptr);
		memcpy(new_ptr, ptr, old_size < size ? old_size : size);
		_aligned_free(ptr);
	}
	return new_ptr;
}

struct FCompatibilityLevelInfo
{
	FCompatibilityLevelInfo(UINT InCompositeLevel, UINT InCPULevel, UINT InGPULevel)
	: CompositeLevel(InCompositeLevel), CPULevel(InCPULevel), GPULevel(InGPULevel) {}
	FCompatibilityLevelInfo() : CompositeLevel(3), CPULevel(3), GPULevel(3) {}
	UINT CompositeLevel;
	UINT CPULevel;
	UINT GPULevel;
	FCompatibilityLevelInfo ClampToValidRange() const { return *this; }
};

inline FCompatibilityLevelInfo appGetCompatibilityLevel()
{
	return FCompatibilityLevelInfo(3, 3, 3);
}

inline UBOOL appSetCompatibilityLevel(FCompatibilityLevelInfo Level, UBOOL bWriteToIni)
{
	(void)Level; (void)bWriteToIni;
	return TRUE;
}

FORCEINLINE void appMemoryBarrier()
{
	__sync_synchronize();
}

FORCEINLINE ScriptPointerType appPointerToSPtr(void* Pointer)
{
	return (ScriptPointerType)(uintptr_t)Pointer;
}

FORCEINLINE void* appSPtrToPointer(ScriptPointerType Value)
{
	return (void*)(uintptr_t)Value;
}

UBOOL appCanBreakLineAt( TCHAR Previous, TCHAR Current );
void appHandleIOFailure( const TCHAR* Filename );
void appMacPumpMessages();
void appShowGameWindow();
void appPreventScreenSaver();
void appShowSplash( const TCHAR* InSplashName );
void appHideSplash();
inline void appPushMarker( const TCHAR* MarkerName ) {}
inline void appPopMarker() {}

/*----------------------------------------------------------------------------
	String conversion helpers.
----------------------------------------------------------------------------*/

void appMacWideCharToMultiByte(DWORD CodePage, const TCHAR* Source, INT SourceLen, ANSICHAR* Dest, INT DestLen);
void appMacMultiByteToWideChar(DWORD CodePage, const ANSICHAR* Source, TCHAR* Dest, INT DestLen);
void* appMacUTF32ToUTF16(const unsigned int* Source, unsigned short* Dest, int Length);

#include "UnStringConv.h"

#define TCHAR_TO_ANSI(str) (ANSICHAR*)FTCHARToANSI((const TCHAR*)str)
#define TCHAR_TO_OEM(str)  (ANSICHAR*)FTCHARToOEM((const TCHAR*)str)
#define ANSI_TO_TCHAR(str) (TCHAR*)FANSIToTCHAR((const ANSICHAR*)str)
#define TCHAR_TO_UTF8(str) (ANSICHAR*)FTCHARToUTF8((const TCHAR*)str)
#define UTF8_TO_TCHAR(str) (TCHAR*)FUTF8ToTCHAR((const ANSICHAR*)str)

/*----------------------------------------------------------------------------
	Stack walking flags and functions.
----------------------------------------------------------------------------*/

typedef DWORD EVerbosityFlags;
#define VF_DISPLAY_BASIC	0x00000000
#define VF_DISPLAY_FILENAME 0x00000001
#define VF_DISPLAY_MODULE	0x00000002
#define VF_DISPLAY_ALL		0xffffffff

struct FProgramCounterSymbolInfo
{
	ANSICHAR	ModuleName[1024];
	ANSICHAR	FunctionName[1024];
	ANSICHAR	Filename[1024];
	INT			LineNumber;
	INT			SymbolDisplacement;
};

UBOOL appInitStackWalking();
void appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, void* Context = NULL );
UBOOL appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags = VF_DISPLAY_ALL );
FProgramCounterSymbolInfo appProgramCounterToSymbolInfo( QWORD ProgramCounter );
void appStackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, INT IgnoreCount = 0, void* Context = NULL );
void appDumpCallStackToLog( INT IgnoreCount = 2 );

/*----------------------------------------------------------------------------
	Variable argument helper functions.
----------------------------------------------------------------------------*/

INT appGetVarArgs( TCHAR* Dest, SIZE_T DestSize, INT Count, const TCHAR*& Fmt, va_list ArgPtr );
INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T DestSize, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr );

void appSetConsoleInterruptHandler();
void appGetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, INT ResultLength);

#endif // __INC_MAC_H__
