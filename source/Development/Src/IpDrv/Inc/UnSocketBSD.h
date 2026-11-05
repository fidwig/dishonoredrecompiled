/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _UN_SOCKET_BSD_H
#define _UN_SOCKET_BSD_H

#if WITH_UE3_NETWORKING

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/** Allows sockets to set custom errors, without overriding the entire socket subsystem */
extern INT GLastSocketError;

/** Platform specific errors mapped to our error codes */
enum ESocketErrors
{
	SE_NO_ERROR = 0,
	SE_EINTR = EINTR,
	SE_EBADF = EBADF,
	SE_EACCES = EACCES,
	SE_EFAULT = EFAULT,
	SE_EINVAL = EINVAL,
	SE_EMFILE = EMFILE,
	SE_EWOULDBLOCK = EWOULDBLOCK,
	SE_EINPROGRESS = EINPROGRESS,
	SE_EALREADY = EALREADY,
	SE_ENOTSOCK = ENOTSOCK,
	SE_EDESTADDRREQ = EDESTADDRREQ,
	SE_EMSGSIZE = EMSGSIZE,
	SE_EPROTOTYPE = EPROTOTYPE,
	SE_ENOPROTOOPT = ENOPROTOOPT,
	SE_EPROTONOSUPPORT = EPROTONOSUPPORT,
	SE_ESOCKTNOSUPPORT = ESOCKTNOSUPPORT,
	SE_EOPNOTSUPP = EOPNOTSUPP,
	SE_EPFNOSUPPORT = EPFNOSUPPORT,
	SE_EAFNOSUPPORT = EAFNOSUPPORT,
	SE_EADDRINUSE = EADDRINUSE,
	SE_EADDRNOTAVAIL = EADDRNOTAVAIL,
	SE_ENETDOWN = ENETDOWN,
	SE_ENETUNREACH = ENETUNREACH,
	SE_ENETRESET = ENETRESET,
	SE_ECONNABORTED = ECONNABORTED,
	SE_ECONNRESET = ECONNRESET,
	SE_ENOBUFS = ENOBUFS,
	SE_EISCONN = EISCONN,
	SE_ENOTCONN = ENOTCONN,
	SE_ESHUTDOWN = ESHUTDOWN,
	SE_ETOOMANYREFS = ETOOMANYREFS,
	SE_ETIMEDOUT = ETIMEDOUT,
	SE_ECONNREFUSED = ECONNREFUSED,
	SE_ELOOP = ELOOP,
	SE_ENAMETOOLONG = ENAMETOOLONG,
	SE_EHOSTDOWN = EHOSTDOWN,
	SE_EHOSTUNREACH = EHOSTUNREACH,
	SE_ENOTEMPTY = ENOTEMPTY,
	SE_EPROCLIM = 10067,
	SE_EUSERS = EUSERS,
	SE_EDQUOT = EDQUOT,
	SE_ESTALE = ESTALE,
	SE_EREMOTE = EREMOTE,
	SE_EDISCON = 10101,
	SE_SYSNOTREADY = 10091,
	SE_VERNOTSUPPORTED = 10092,
	SE_NOTINITIALISED = 10093,
	SE_HOST_NOT_FOUND = 11001,
	SE_TRY_AGAIN = 11002,
	SE_NO_RECOVERY = 11003,
	SE_NO_DATA = 11004,
	SE_UDP_ERR_PORT_UNREACH = ECONNRESET
};

/**
 * BSD socket class
 */
class FSocketBSD : public FSocket
{
protected:
	SOCKET Socket;

public:
	FSocketBSD(SOCKET InSocket, ESocketType InSocketType, const FString& InSocketDescription)
		: FSocket(InSocketType, InSocketDescription)
		, Socket(InSocket)
	{
	}

	virtual ~FSocketBSD(void)
	{
		Close();
	}

	virtual UBOOL Close(void);
	virtual UBOOL Bind(const FInternetIpAddr& Addr);
	virtual UBOOL Connect(const FInternetIpAddr& Addr);
	virtual UBOOL Listen(INT MaxBacklog);
	virtual UBOOL HasPendingConnection(UBOOL& bHasPendingConnection);
	virtual UBOOL HasPendingData(UINT& PendingDataSize);
	virtual FSocket* Accept(const FString& SocketDescription);
	virtual FSocket* Accept(FInternetIpAddr& OutAddr, const FString& SocketDescription);
	virtual UBOOL SendTo(const BYTE* Data, INT Count, INT& BytesSent, const FInternetIpAddr& Destination);
	virtual UBOOL Send(const BYTE* Data, INT Count, INT& BytesSent);
	virtual UBOOL RecvFrom(BYTE* Data, INT BufferSize, INT& BytesRead, FInternetIpAddr& Source);
	virtual UBOOL Recv(BYTE* Data, INT BufferSize, INT& BytesRead);
	virtual ESocketConnectionState GetConnectionState(void);
	virtual FInternetIpAddr GetAddress(void);
	virtual UBOOL SetNonBlocking(UBOOL bIsNonBlocking = TRUE);
	virtual UBOOL SetBroadcast(UBOOL bAllowBroadcast = TRUE);
	virtual UBOOL SetReuseAddr(UBOOL bAllowReuse = TRUE);
	virtual UBOOL SetLinger(UBOOL bShouldLinger = TRUE, INT Timeout = 0);
	virtual UBOOL SetRecvErr(UBOOL bUseErrorQueue = TRUE);
	virtual UBOOL SetSendBufferSize(INT Size, INT& NewSize);
	virtual UBOOL SetReceiveBufferSize(INT Size, INT& NewSize);
	virtual INT GetPortNo(void);
	virtual UBOOL GetErrorOriginatingAddress(FInternetIpAddr& FromAddr)
	{
		return TRUE;
	}
};

/**
 * BSD socket subsystem implementation
 */
class FSocketSubsystemBSD : public FSocketSubsystem
{
protected:
	UBOOL bTriedToInit;
	FCriticalSection HostByNameSynch;

public:
	FSocketSubsystemBSD(void)
		: bTriedToInit(FALSE)
	{
	}

	virtual ~FSocketSubsystemBSD(void)
	{
	}

	virtual UBOOL Initialize(FString& Error);
	virtual void Destroy(void);
	virtual FSocket* CreateDGramSocket(const FString& SocketDescription, UBOOL bForceUDP = FALSE);
	virtual FSocket* CreateStreamSocket(const FString& SocketDescription);
	virtual void DestroySocket(FSocket* Socket);
	virtual INT GetLastErrorCode(void);
	virtual const TCHAR* GetSocketError(INT Code = -1);
	virtual INT GetHostByName(ANSICHAR* HostName, FInternetIpAddr& Addr);
	virtual UBOOL GetHostName(FString& HostName);
	virtual UBOOL GetLocalHostAddr(FOutputDevice& Out, FInternetIpAddr& HostAddr);
};

#endif // WITH_UE3_NETWORKING

#endif // _UN_SOCKET_BSD_H
