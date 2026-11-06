/*=============================================================================
	UnMacClient.cpp: macOS platform client and viewport implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if PLATFORM_MACOSX

#define __NUMBERFORMATTING__ 1
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include "UnMacClient.h"

static void appMacPumpCocoaEvents()
{
	@autoreleasepool {
		if (NSApp != nil)
		{
			NSEvent* Event = nil;
			while ((Event = [NSApp nextEventMatchingMask:NSEventMaskAny
			                                   untilDate:[NSDate distantPast]
			                                      inMode:NSDefaultRunLoopMode
			                                     dequeue:YES]))
			{
				[NSApp sendEvent:Event];
			}
			[NSApp updateWindows];
		}
	}
}

@interface FMacContentView : NSView
@end

@implementation FMacContentView
- (BOOL)isFlipped
{
	return YES;
}
@end

/*-----------------------------------------------------------------------------
	FMacViewport
-----------------------------------------------------------------------------*/

FMacViewport::FMacViewport(UMacClient* InClient, FViewportClient* InViewportClient, const TCHAR* InName, UINT InSizeX, UINT InSizeY, UBOOL InFullscreen)
	: FViewport(InViewportClient)
	, Client(InClient)
	, Name(InName)
	, WindowHandle(NULL)
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
	if( WindowHandle )
	{
		@autoreleasepool {
			NSWindow* Window = (NSWindow*)CFBridgingRelease(WindowHandle);
			[Window close];
			WindowHandle = NULL;
		}
	}
}

void FMacViewport::Resize(UINT NewSizeX, UINT NewSizeY, UBOOL NewFullscreen, INT InPosX, INT InPosY)
{
	if( !WindowHandle )
	{
		@autoreleasepool {
			[NSApplication sharedApplication];
			[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

			CGFloat Width = (CGFloat)NewSizeX;
			CGFloat Height = (CGFloat)NewSizeY;
			CGFloat X = InPosX >= 0 ? (CGFloat)InPosX : 100.0;
			CGFloat Y = InPosY >= 0 ? (CGFloat)InPosY : 100.0;

			NSRect FrameRect = NSMakeRect(X, Y, Width, Height);
			NSWindowStyleMask StyleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

			NSWindow* Window = [[NSWindow alloc] initWithContentRect:FrameRect
			                                               styleMask:StyleMask
			                                                 backing:NSBackingStoreBuffered
			                                                   defer:NO];
			[Window setTitle:@"Dishonored"];
			[Window setAcceptsMouseMovedEvents:YES];

			if( InPosX < 0 && InPosY < 0 )
			{
				[Window center];
			}

			FMacContentView* ContentView = [[FMacContentView alloc] initWithFrame:FrameRect];
			[ContentView setWantsLayer:YES];
			CAMetalLayer* MetalLayer = [CAMetalLayer layer];
			[MetalLayer setGeometryFlipped:YES];
			[ContentView setLayer:MetalLayer];
			[Window setContentView:ContentView];

			[Window makeKeyAndOrderFront:nil];
			[NSApp activateIgnoringOtherApps:YES];

			WindowHandle = (void*)CFBridgingRetain(Window);
			debugf(NAME_Init, TEXT("Created native Cocoa NSWindow: %p for viewport %ux%u"), WindowHandle, NewSizeX, NewSizeY);
		}
	}
	else
	{
		@autoreleasepool {
			NSWindow* Window = (__bridge NSWindow*)WindowHandle;
			NSRect Frame = [Window frame];
			Frame.size.width = (CGFloat)NewSizeX;
			Frame.size.height = (CGFloat)NewSizeY;
			[Window setFrame:Frame display:YES animate:NO];
		}
	}

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
	appMacPumpCocoaEvents();
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
