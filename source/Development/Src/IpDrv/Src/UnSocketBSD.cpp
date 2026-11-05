/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "NetworkProfiler.h"

INT GLastSocketError = SE_NO_ERROR;

#if PLATFORM_MACOSX || PLATFORM_UNIX || IPHONE

FSocketSubsystemBSD SocketSubsystem;

void appSocketInit(UBOOL bIsEarlyInit)
{
	if (!bIsEarlyInit)
	{
		return;
	}

	GSocketSubsystem = &SocketSubsystem;
	GSocketSubsystemDebug = &SocketSubsystem;
	FString Error;
	if (GSocketSubsystem->Initialize(Error) == FALSE)
	{
		debugf(NAME_Init, TEXT("Failed to initialize socket subsystem: (%s)"), *Error);
	}
}

void appSocketExit(void)
{
	if (GSocketSubsystem != NULL)
	{
		GSocketSubsystem->Destroy();
	}

	if (GSocketSubsystemDebug != NULL && GSocketSubsystemDebug != GSocketSubsystem)
	{
		GSocketSubsystemDebug->Destroy();
	}
}

UBOOL FSocketBSD::Close(void)
{
	if (Socket != INVALID_SOCKET)
	{
		int Ret = close(Socket);
		Socket = INVALID_SOCKET;
		return Ret == 0;
	}
	return TRUE;
}

UBOOL FSocketBSD::Bind(const FInternetIpAddr& Addr)
{
	return bind(Socket, (const struct sockaddr*)Addr, sizeof(SOCKADDR_IN)) == 0;
}

UBOOL FSocketBSD::Connect(const FInternetIpAddr& Addr)
{
	INT Return = connect(Socket, (const struct sockaddr*)Addr, sizeof(SOCKADDR_IN));
	return Return == 0 || errno == EINPROGRESS || errno == EWOULDBLOCK;
}

UBOOL FSocketBSD::Listen(INT MaxBacklog)
{
	return listen(Socket, MaxBacklog) == 0;
}

UBOOL FSocketBSD::HasPendingConnection(UBOOL& bHasPendingConnection)
{
	bHasPendingConnection = FALSE;
	struct timeval Time = {0, 0};
	fd_set SocketSet;
	FD_ZERO(&SocketSet);
	FD_SET(Socket, &SocketSet);

	INT SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, &Time);
	if (SelectStatus > 0)
	{
		bHasPendingConnection = TRUE;
	}
	return SelectStatus >= 0;
}

UBOOL FSocketBSD::HasPendingData(UINT& PendingDataSize)
{
	PendingDataSize = 0;
	struct timeval Time = {0, 0};
	fd_set SocketSet;
	FD_ZERO(&SocketSet);
	FD_SET(Socket, &SocketSet);

	INT SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, &Time);
	if (SelectStatus > 0)
	{
		int PendingBytes = 0;
		if (ioctl(Socket, FIONREAD, &PendingBytes) == 0)
		{
			PendingDataSize = (UINT)PendingBytes;
			return TRUE;
		}
	}
	return FALSE;
}

FSocket* FSocketBSD::Accept(const FString& SocketDescription)
{
	SOCKET NewSocket = accept(Socket, NULL, NULL);
	if (NewSocket != INVALID_SOCKET)
	{
		return new FSocketBSD(NewSocket, SocketType, SocketDescription);
	}
	return NULL;
}

FSocket* FSocketBSD::Accept(FInternetIpAddr& OutAddr, const FString& SocketDescription)
{
	socklen_t SizeOf = sizeof(SOCKADDR_IN);
	SOCKET NewSocket = accept(Socket, (struct sockaddr*)OutAddr, &SizeOf);
	if (NewSocket != INVALID_SOCKET)
	{
		return new FSocketBSD(NewSocket, SocketType, SocketDescription);
	}
	return NULL;
}

UBOOL FSocketBSD::SendTo(const BYTE* Data, INT Count, INT& BytesSent, const FInternetIpAddr& Destination)
{
	ssize_t Result = sendto(Socket, (const char*)Data, Count, MSG_NOSIGNAL, (const struct sockaddr*)Destination, sizeof(SOCKADDR_IN));
	BytesSent = (INT)Result;
	NETWORK_PROFILER(FSocket::SendTo(Data, Count, BytesSent, Destination));
	return BytesSent >= 0;
}

UBOOL FSocketBSD::Send(const BYTE* Data, INT Count, INT& BytesSent)
{
	ssize_t Result;
	do
	{
		Result = send(Socket, (const char*)Data, Count, MSG_NOSIGNAL);
	} while (Result < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN));

	BytesSent = (INT)Result;
	NETWORK_PROFILER(FSocket::Send(Data, Count, BytesSent));
	return BytesSent >= 0;
}

UBOOL FSocketBSD::RecvFrom(BYTE* Data, INT BufferSize, INT& BytesRead, FInternetIpAddr& Source)
{
	socklen_t Size = sizeof(SOCKADDR_IN);
	ssize_t Result = recvfrom(Socket, (char*)Data, BufferSize, 0, (struct sockaddr*)Source, &Size);
	BytesRead = (INT)Result;
	NETWORK_PROFILER(FSocket::RecvFrom(Data, BufferSize, BytesRead, Source));
	return BytesRead >= 0;
}

UBOOL FSocketBSD::Recv(BYTE* Data, INT BufferSize, INT& BytesRead)
{
	ssize_t Result = recv(Socket, (char*)Data, BufferSize, 0);
	BytesRead = (INT)Result;
	NETWORK_PROFILER(FSocket::Recv(Data, BufferSize, BytesRead));
	return BytesRead >= 0;
}

ESocketConnectionState FSocketBSD::GetConnectionState(void)
{
	struct timeval Time = {0, 0};
	fd_set ReadSet, WriteSet, ErrorSet;
	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);
	FD_ZERO(&ErrorSet);
	FD_SET(Socket, &ReadSet);
	FD_SET(Socket, &WriteSet);
	FD_SET(Socket, &ErrorSet);

	INT SelectStatus = select(Socket + 1, &ReadSet, &WriteSet, &ErrorSet, &Time);
	if (SelectStatus > 0)
	{
		if (FD_ISSET(Socket, &ErrorSet))
		{
			return SCS_ConnectionError;
		}
		if (FD_ISSET(Socket, &WriteSet))
		{
			return SCS_Connected;
		}
	}
	else if (SelectStatus == 0)
	{
		return SCS_NotConnected;
	}
	return SCS_ConnectionError;
}

FInternetIpAddr FSocketBSD::GetAddress(void)
{
	FInternetIpAddr Addr;
	socklen_t Size = sizeof(SOCKADDR_IN);
	if (getsockname(Socket, (struct sockaddr*)Addr, &Size) != 0)
	{
		debugf(NAME_Error, TEXT("Failed to read address for socket (%s)"), GSocketSubsystem->GetSocketError());
	}
	return Addr;
}

UBOOL FSocketBSD::SetNonBlocking(UBOOL bIsNonBlocking)
{
	int Flags = fcntl(Socket, F_GETFL, 0);
	if (Flags != -1)
	{
		Flags = bIsNonBlocking ? (Flags | O_NONBLOCK) : (Flags & ~O_NONBLOCK);
		return fcntl(Socket, F_SETFL, Flags) == 0;
	}
	return FALSE;
}

UBOOL FSocketBSD::SetBroadcast(UBOOL bAllowBroadcast)
{
	int Opt = bAllowBroadcast ? 1 : 0;
	return setsockopt(Socket, SOL_SOCKET, SO_BROADCAST, &Opt, sizeof(Opt)) == 0;
}

UBOOL FSocketBSD::SetReuseAddr(UBOOL bAllowReuse)
{
	int Opt = bAllowReuse ? 1 : 0;
	return setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &Opt, sizeof(Opt)) == 0;
}

UBOOL FSocketBSD::SetLinger(UBOOL bShouldLinger, INT Timeout)
{
	struct linger ling;
	ling.l_onoff = bShouldLinger ? 1 : 0;
	ling.l_linger = Timeout;
	return setsockopt(Socket, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == 0;
}

UBOOL FSocketBSD::SetRecvErr(UBOOL bUseErrorQueue)
{
	(void)bUseErrorQueue;
	return TRUE;
}

UBOOL FSocketBSD::SetSendBufferSize(INT Size, INT& NewSize)
{
	socklen_t SizeSize = sizeof(INT);
	UBOOL bOk = setsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &Size, sizeof(INT)) == 0;
	getsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &NewSize, &SizeSize);
	return bOk;
}

UBOOL FSocketBSD::SetReceiveBufferSize(INT Size, INT& NewSize)
{
	socklen_t SizeSize = sizeof(INT);
	UBOOL bOk = setsockopt(Socket, SOL_SOCKET, SO_RCVBUF, &Size, sizeof(INT)) == 0;
	getsockopt(Socket, SOL_SOCKET, SO_RCVBUF, &NewSize, &SizeSize);
	return bOk;
}

INT FSocketBSD::GetPortNo(void)
{
	const FInternetIpAddr& Addr = GetAddress();
	return Addr.GetPort();
}

UBOOL FSocketSubsystemBSD::Initialize(FString& Error)
{
	(void)Error;
	if (bTriedToInit == FALSE)
	{
		bTriedToInit = TRUE;
		GIpDrvInitialized = TRUE;
		debugf(NAME_Init, TEXT("BSD Sockets initialized"));
	}
	return GIpDrvInitialized;
}

void FSocketSubsystemBSD::Destroy(void)
{
	GIpDrvInitialized = FALSE;
}

FSocket* FSocketSubsystemBSD::CreateDGramSocket(const FString& SocketDescription, UBOOL bForceUDP)
{
	(void)bForceUDP;
	SOCKET Sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	return Sock != INVALID_SOCKET ? new FSocketBSD(Sock, SOCKTYPE_Datagram, SocketDescription) : NULL;
}

FSocket* FSocketSubsystemBSD::CreateStreamSocket(const FString& SocketDescription)
{
	SOCKET Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	return Sock != INVALID_SOCKET ? new FSocketBSD(Sock, SOCKTYPE_Streaming, SocketDescription) : NULL;
}

void FSocketSubsystemBSD::DestroySocket(FSocket* Socket)
{
	delete Socket;
}

INT FSocketSubsystemBSD::GetLastErrorCode(void)
{
	if (GLastSocketError != SE_NO_ERROR)
	{
		const INT RetVal = GLastSocketError;
		GLastSocketError = SE_NO_ERROR;
		return RetVal;
	}
	return errno;
}

const TCHAR* FSocketSubsystemBSD::GetSocketError(INT Code)
{
	if (Code == -1)
	{
		Code = GetLastErrorCode();
	}
	switch (Code)
	{
		case SE_NO_ERROR: return TEXT("SE_NO_ERROR");
		case SE_EINTR: return TEXT("SE_EINTR");
		case SE_EBADF: return TEXT("SE_EBADF");
		case SE_EACCES: return TEXT("SE_EACCES");
		case SE_EFAULT: return TEXT("SE_EFAULT");
		case SE_EINVAL: return TEXT("SE_EINVAL");
		case SE_EMFILE: return TEXT("SE_EMFILE");
		case SE_EWOULDBLOCK: return TEXT("SE_EWOULDBLOCK");
		case SE_EINPROGRESS: return TEXT("SE_EINPROGRESS");
		case SE_EALREADY: return TEXT("SE_EALREADY");
		case SE_ENOTSOCK: return TEXT("SE_ENOTSOCK");
		case SE_EDESTADDRREQ: return TEXT("SE_EDESTADDRREQ");
		case SE_EMSGSIZE: return TEXT("SE_EMSGSIZE");
		case SE_EPROTOTYPE: return TEXT("SE_EPROTOTYPE");
		case SE_ENOPROTOOPT: return TEXT("SE_ENOPROTOOPT");
		case SE_EPROTONOSUPPORT: return TEXT("SE_EPROTONOSUPPORT");
		case SE_ESOCKTNOSUPPORT: return TEXT("SE_ESOCKTNOSUPPORT");
		case SE_EOPNOTSUPP: return TEXT("SE_EOPNOTSUPP");
		case SE_EPFNOSUPPORT: return TEXT("SE_EPFNOSUPPORT");
		case SE_EAFNOSUPPORT: return TEXT("SE_EAFNOSUPPORT");
		case SE_EADDRINUSE: return TEXT("SE_EADDRINUSE");
		case SE_EADDRNOTAVAIL: return TEXT("SE_EADDRNOTAVAIL");
		case SE_ENETDOWN: return TEXT("SE_ENETDOWN");
		case SE_ENETUNREACH: return TEXT("SE_ENETUNREACH");
		case SE_ENETRESET: return TEXT("SE_ENETRESET");
		case SE_ECONNABORTED: return TEXT("SE_ECONNABORTED");
		case SE_ECONNRESET: return TEXT("SE_ECONNRESET");
		case SE_ENOBUFS: return TEXT("SE_ENOBUFS");
		case SE_EISCONN: return TEXT("SE_EISCONN");
		case SE_ENOTCONN: return TEXT("SE_ENOTCONN");
		case SE_ESHUTDOWN: return TEXT("SE_ESHUTDOWN");
		case SE_ETOOMANYREFS: return TEXT("SE_ETOOMANYREFS");
		case SE_ETIMEDOUT: return TEXT("SE_ETIMEDOUT");
		case SE_ECONNREFUSED: return TEXT("SE_ECONNREFUSED");
		case SE_ELOOP: return TEXT("SE_ELOOP");
		case SE_ENAMETOOLONG: return TEXT("SE_ENAMETOOLONG");
		case SE_EHOSTDOWN: return TEXT("SE_EHOSTDOWN");
		case SE_EHOSTUNREACH: return TEXT("SE_EHOSTUNREACH");
		case SE_ENOTEMPTY: return TEXT("SE_ENOTEMPTY");
		default: return TEXT("Unknown socket error");
	}
}

INT FSocketSubsystemBSD::GetHostByName(ANSICHAR* HostName, FInternetIpAddr& Addr)
{
	INT ErrorCode = 0;
	FScopeLock ScopeLock(&HostByNameSynch);
	HOSTENT* HostEnt = gethostbyname(HostName);
	if (HostEnt != NULL)
	{
		if (HostEnt->h_addrtype == AF_INET)
		{
			Addr.SetIp(*(struct in_addr*)(*HostEnt->h_addr_list));
		}
		else
		{
			ErrorCode = SE_HOST_NOT_FOUND;
		}
	}
	else
	{
		ErrorCode = h_errno;
	}
	return ErrorCode;
}

UBOOL FSocketSubsystemBSD::GetHostName(FString& HostName)
{
	ANSICHAR Buffer[256];
	UBOOL bRead = gethostname(Buffer, sizeof(Buffer)) == 0;
	if (bRead)
	{
		HostName = ANSI_TO_TCHAR(Buffer);
	}
	return bRead;
}

UBOOL FSocketSubsystemBSD::GetLocalHostAddr(FOutputDevice& Out, FInternetIpAddr& HostAddr)
{
	UBOOL CanBindAll = FALSE;
	HostAddr.SetAnyAddress();
	TCHAR Home[256] = TEXT("");
	FString HostName;
	if (GSocketSubsystem->GetHostName(HostName) == FALSE)
	{
		Out.Logf(TEXT("%s: gethostname failed (%s)"), SOCKET_API, GSocketSubsystem->GetSocketError());
	}
	if (Parse(appCmdLine(), TEXT("MULTIHOME="), Home, ARRAY_COUNT(Home)))
	{
		UBOOL bIsValid = FALSE;
		HostAddr.SetIp(Home, bIsValid);
		if (Home[0] == 0 || bIsValid == FALSE)
		{
			Out.Logf(TEXT("Invalid multihome IP address %s"), Home);
		}
	}
	else
	{
		if (GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*HostName), HostAddr) == 0)
		{
			if (!ParseParam(appCmdLine(), TEXT("PRIMARYNET")))
			{
				CanBindAll = TRUE;
			}
			static UBOOL First;
			if (!First)
			{
				First = TRUE;
				debugf(NAME_Init, TEXT("%s: I am %s (%s)"), SOCKET_API, *HostName, *HostAddr.ToString(TRUE));
			}
		}
		else
		{
			Out.Logf(TEXT("gethostbyname failed (%s)"), GSocketSubsystem->GetSocketError());
		}
	}
	return CanBindAll;
}

#endif // PLATFORM_MACOSX || PLATFORM_UNIX || IPHONE

#endif // WITH_UE3_NETWORKING
