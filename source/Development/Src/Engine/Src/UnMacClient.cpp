/*=============================================================================
	UnMacClient.cpp: macOS platform client and viewport implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if PLATFORM_MACOSX

#include "UnMacClient.h"

/*-----------------------------------------------------------------------------
	FMacViewport
-----------------------------------------------------------------------------*/

FMacViewport::FMacViewport(UMacClient* InClient, FViewportClient* InViewportClient, const TCHAR* InName, UINT InSizeX, UINT InSizeY, UBOOL InFullscreen)
	: FViewport(InViewportClient)
	, Client(InClient)
	, Name(InName)
{
	Resize(InSizeX, InSizeY, InFullscreen);
}

FMacViewport::~FMacViewport()
{
	Destroy();
}

void FMacViewport::Destroy()
{
	if( ViewportClient )
	{
		ViewportClient = NULL;
		UpdateViewportRHI(TRUE, 0, 0, FALSE);
	}
}

void FMacViewport::Resize(UINT NewSizeX, UINT NewSizeY, UBOOL NewFullscreen, INT InPosX, INT InPosY)
{
	UpdateViewportRHI(FALSE, NewSizeX, NewSizeY, NewFullscreen);
}

/*-----------------------------------------------------------------------------
	UMacClient
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UMacClient);

TArray<FMacViewport*> UMacClient::Viewports;

UMacClient::UMacClient()
	: Engine(NULL)
{
}

void UMacClient::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UMacClient, Engine ) );
}

void UMacClient::Init(UEngine* InEngine)
{
	Engine = InEngine;
	debugf(NAME_Init, TEXT("MacClient initialized."));
}

void UMacClient::Tick(FLOAT DeltaTime)
{
	extern void appMacPumpMessages();
	appMacPumpMessages();
}

UBOOL UMacClient::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	return FALSE;
}

void UMacClient::AllowMessageProcessing(UBOOL InValue)
{
}

FViewportFrame* UMacClient::CreateViewportFrame(FViewportClient* ViewportClient, const TCHAR* InName, UINT SizeX, UINT SizeY, UBOOL Fullscreen)
{
	FMacViewport* Viewport = new FMacViewport(this, ViewportClient, InName, SizeX, SizeY, Fullscreen);
	Viewports.AddItem(Viewport);
	return Viewport;
}

FViewport* UMacClient::CreateWindowChildViewport(FViewportClient* ViewportClient, void* ParentWindow, UINT SizeX, UINT SizeY, INT InPosX, INT InPosY)
{
	return CreateViewportFrame(ViewportClient, TEXT("Child"), SizeX, SizeY, FALSE)->GetViewport();
}

void UMacClient::CloseViewport(FViewport* Viewport)
{
	FMacViewport* MacViewport = (FMacViewport*)Viewport;
	Viewports.RemoveItem(MacViewport);
	MacViewport->Destroy();
	delete MacViewport;
}

class UAudioDevice* UMacClient::GetAudioDevice()
{
	return NULL;
}

void UMacClient::ForceClearForceFeedback()
{
}

FName UMacClient::GetVirtualKeyName(INT KeyCode) const
{
	return NAME_None;
}

#endif // PLATFORM_MACOSX
