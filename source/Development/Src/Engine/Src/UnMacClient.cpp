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

static BOOL g_bCursorHiddenInOS = NO;

static void MacHideCursor()
{
	if (!g_bCursorHiddenInOS)
	{
		[NSCursor hide];
		g_bCursorHiddenInOS = YES;
	}
}

static void MacUnhideCursor()
{
	if (g_bCursorHiddenInOS)
	{
		[NSCursor unhide];
		g_bCursorHiddenInOS = NO;
	}
}

static FName MacKeyCodeToFName(unsigned short KeyCode)
{
	switch (KeyCode)
	{
		case 0x00: return KEY_A;
		case 0x01: return KEY_S;
		case 0x02: return KEY_D;
		case 0x03: return KEY_F;
		case 0x04: return KEY_H;
		case 0x05: return KEY_G;
		case 0x06: return KEY_Z;
		case 0x07: return KEY_X;
		case 0x08: return KEY_C;
		case 0x09: return KEY_V;
		case 0x0B: return KEY_B;
		case 0x0C: return KEY_Q;
		case 0x0D: return KEY_W;
		case 0x0E: return KEY_E;
		case 0x0F: return KEY_R;
		case 0x10: return KEY_Y;
		case 0x11: return KEY_T;
		case 0x12: return KEY_One;
		case 0x13: return KEY_Two;
		case 0x14: return KEY_Three;
		case 0x15: return KEY_Four;
		case 0x16: return KEY_Six;
		case 0x17: return KEY_Five;
		case 0x18: return KEY_Equals;
		case 0x19: return KEY_Nine;
		case 0x1A: return KEY_Seven;
		case 0x1B: return KEY_Underscore;
		case 0x1C: return KEY_Eight;
		case 0x1D: return KEY_Zero;
		case 0x1E: return KEY_RightBracket;
		case 0x1F: return KEY_O;
		case 0x20: return KEY_U;
		case 0x21: return KEY_LeftBracket;
		case 0x22: return KEY_I;
		case 0x23: return KEY_P;
		case 0x24: return KEY_Enter;
		case 0x25: return KEY_L;
		case 0x26: return KEY_J;
		case 0x27: return KEY_Quote;
		case 0x28: return KEY_K;
		case 0x29: return KEY_Semicolon;
		case 0x2A: return KEY_Backslash;
		case 0x2B: return KEY_Comma;
		case 0x2C: return KEY_Slash;
		case 0x2D: return KEY_N;
		case 0x2E: return KEY_M;
		case 0x2F: return KEY_Period;
		case 0x30: return KEY_Tab;
		case 0x31: return KEY_SpaceBar;
		case 0x32: return KEY_Tilde;
		case 0x33: return KEY_BackSpace;
		case 0x35: return KEY_Escape;
		case 0x41: return KEY_Decimal;
		case 0x43: return KEY_Multiply;
		case 0x45: return KEY_Add;
		case 0x4B: return KEY_Divide;
		case 0x4C: return KEY_Enter;
		case 0x4E: return KEY_Subtract;
		case 0x51: return KEY_Equals;
		case 0x52: return KEY_NumPadZero;
		case 0x53: return KEY_NumPadOne;
		case 0x54: return KEY_NumPadTwo;
		case 0x55: return KEY_NumPadThree;
		case 0x56: return KEY_NumPadFour;
		case 0x57: return KEY_NumPadFive;
		case 0x58: return KEY_NumPadSix;
		case 0x59: return KEY_NumPadSeven;
		case 0x5B: return KEY_NumPadEight;
		case 0x5C: return KEY_NumPadNine;
		case 0x60: return KEY_F5;
		case 0x61: return KEY_F6;
		case 0x62: return KEY_F7;
		case 0x63: return KEY_F3;
		case 0x64: return KEY_F8;
		case 0x65: return KEY_F9;
		case 0x67: return KEY_F11;
		case 0x6D: return KEY_F10;
		case 0x6F: return KEY_F12;
		case 0x73: return KEY_Home;
		case 0x74: return KEY_PageUp;
		case 0x75: return KEY_Delete;
		case 0x76: return KEY_F4;
		case 0x77: return KEY_End;
		case 0x78: return KEY_F2;
		case 0x79: return KEY_PageDown;
		case 0x7A: return KEY_F1;
		case 0x7B: return KEY_Left;
		case 0x7C: return KEY_Right;
		case 0x7D: return KEY_Down;
		case 0x7E: return KEY_Up;
		default:   return NAME_None;
	}
}

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
@property (nonatomic, assign) FMacViewport* Viewport;
@property (nonatomic, strong) NSTrackingArea* TrackingArea;
- (void)handleMouseMotion:(NSEvent *)event;
@end

@implementation FMacContentView
- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
	return YES;
}

- (BOOL)canBecomeKeyView
{
	return YES;
}

- (void)updateTrackingAreas
{
	[super updateTrackingAreas];
	if (self.TrackingArea)
	{
		[self removeTrackingArea:self.TrackingArea];
	}
	NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
	                                NSTrackingMouseMoved |
	                                NSTrackingActiveAlways |
	                                NSTrackingInVisibleRect;
	self.TrackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
	                                                 options:options
	                                                   owner:self
	                                                userInfo:nil];
	[self addTrackingArea:self.TrackingArea];
}

- (void)mouseDown:(NSEvent *)event
{
	if (!self.Viewport) return;
	[[self window] makeFirstResponder:self];
	if (!self.Viewport->bCapturingMouseInput)
	{
		self.Viewport->SetMouseCapture(TRUE);
	}
	self.Viewport->InputKey(KEY_LeftMouseButton, IE_Pressed);
}

- (void)mouseUp:(NSEvent *)event
{
	if (!self.Viewport) return;
	self.Viewport->InputKey(KEY_LeftMouseButton, IE_Released);
}

- (void)rightMouseDown:(NSEvent *)event
{
	if (!self.Viewport) return;
	[[self window] makeFirstResponder:self];
	if (!self.Viewport->bCapturingMouseInput)
	{
		self.Viewport->SetMouseCapture(TRUE);
	}
	self.Viewport->InputKey(KEY_RightMouseButton, IE_Pressed);
}

- (void)rightMouseUp:(NSEvent *)event
{
	if (!self.Viewport) return;
	self.Viewport->InputKey(KEY_RightMouseButton, IE_Released);
}

- (void)otherMouseDown:(NSEvent *)event
{
	if (!self.Viewport) return;
	FName key = ([event buttonNumber] == 2) ? KEY_MiddleMouseButton :
	            ([event buttonNumber] == 3) ? KEY_ThumbMouseButton : KEY_ThumbMouseButton2;
	self.Viewport->InputKey(key, IE_Pressed);
}

- (void)otherMouseUp:(NSEvent *)event
{
	if (!self.Viewport) return;
	FName key = ([event buttonNumber] == 2) ? KEY_MiddleMouseButton :
	            ([event buttonNumber] == 3) ? KEY_ThumbMouseButton : KEY_ThumbMouseButton2;
	self.Viewport->InputKey(key, IE_Released);
}

- (void)mouseMoved:(NSEvent *)event
{
	[self handleMouseMotion:event];
}

- (void)mouseDragged:(NSEvent *)event
{
	[self handleMouseMotion:event];
}

- (void)rightMouseDragged:(NSEvent *)event
{
	[self handleMouseMotion:event];
}

- (void)otherMouseDragged:(NSEvent *)event
{
	[self handleMouseMotion:event];
}

- (void)handleMouseMotion:(NSEvent *)event
{
	if (!self.Viewport) return;

	if (self.Viewport->bCapturingMouseInput)
	{
		NSWindow* win = [self window];
		if (win && ![win isKeyWindow] && ![win isMainWindow])
		{
			self.Viewport->OnWindowLostFocus();
			return;
		}
	}

	CGFloat dx = [event deltaX];
	CGFloat dy = [event deltaY];

	NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
	self.Viewport->MousePositionX = (INT)loc.x;
	self.Viewport->MousePositionY = (INT)loc.y;

	if (self.Viewport->bCapturingMouseInput)
	{
		self.Viewport->AccumulatedMouseDeltaX += (FLOAT)dx;
		self.Viewport->AccumulatedMouseDeltaY += (FLOAT)dy;
	}
	else
	{
		self.Viewport->MouseMove((INT)loc.x, (INT)loc.y);
	}
}

- (void)scrollWheel:(NSEvent *)event
{
	if (!self.Viewport) return;
	CGFloat dy = [event scrollingDeltaY];
	if (dy > 0.05f)
	{
		self.Viewport->InputKey(KEY_MouseScrollUp, IE_Pressed);
		self.Viewport->InputKey(KEY_MouseScrollUp, IE_Released);
	}
	else if (dy < -0.05f)
	{
		self.Viewport->InputKey(KEY_MouseScrollDown, IE_Pressed);
		self.Viewport->InputKey(KEY_MouseScrollDown, IE_Released);
	}
}

- (void)keyDown:(NSEvent *)event
{
	if (!self.Viewport) return;

	NSEventModifierFlags flags = [event modifierFlags];
	unsigned short kc = [event keyCode];

	// Handle Cmd+Q cleanly to quit
	if ((flags & NSEventModifierFlagCommand) != 0)
	{
		if ([[event charactersIgnoringModifiers] isEqualToString:@"q"])
		{
			appRequestExit(0);
			return;
		}
		[super keyDown:event];
		return;
	}

	// If Option/Alt or Control is held with Tab (e.g. window switcher), let the OS / switcher handle it
	if ((flags & (NSEventModifierFlagOption | NSEventModifierFlagControl)) != 0 && kc == 0x30)
	{
		[super keyDown:event];
		return;
	}

	FName keyName = MacKeyCodeToFName(kc);
	if (keyName != NAME_None)
	{
		debugf(TEXT("Cocoa keyDown: kc=0x%x, name=%s"), kc, *keyName.ToString());
		if (keyName == KEY_Escape && self.Viewport->bCapturingMouseInput)
		{
			self.Viewport->SetMouseCapture(FALSE);
		}

		EInputEvent eventType = [event isARepeat] ? IE_Repeat : IE_Pressed;
		self.Viewport->InputKey(keyName, eventType);
	}

	NSString* chars = [event characters];
	if (chars && [chars length] > 0)
	{
		for (NSUInteger i = 0; i < [chars length]; ++i)
		{
			unichar c = [chars characterAtIndex:i];
			if (c >= 32 && c != 127)
			{
				self.Viewport->InputChar((TCHAR)c);
			}
		}
	}
}

- (void)keyUp:(NSEvent *)event
{
	if (!self.Viewport) return;

	FName keyName = MacKeyCodeToFName([event keyCode]);
	if (keyName != NAME_None)
	{
		self.Viewport->InputKey(keyName, IE_Released);
	}
}

- (void)flagsChanged:(NSEvent *)event
{
	if (!self.Viewport) return;

	unsigned short kc = [event keyCode];
	NSEventModifierFlags flags = [event modifierFlags];
	FName keyName = NAME_None;
	BOOL isDown = NO;

	if (kc == 0x38) { keyName = KEY_LeftShift; isDown = (flags & NSEventModifierFlagShift) != 0; }
	else if (kc == 0x3C) { keyName = KEY_RightShift; isDown = (flags & NSEventModifierFlagShift) != 0; }
	else if (kc == 0x3B) { keyName = KEY_LeftControl; isDown = (flags & NSEventModifierFlagControl) != 0; }
	else if (kc == 0x3E) { keyName = KEY_RightControl; isDown = (flags & NSEventModifierFlagControl) != 0; }
	else if (kc == 0x3A) { keyName = KEY_LeftAlt; isDown = (flags & NSEventModifierFlagOption) != 0; }
	else if (kc == 0x3D) { keyName = KEY_RightAlt; isDown = (flags & NSEventModifierFlagOption) != 0; }
	else if (kc == 0x39) { keyName = KEY_CapsLock; isDown = (flags & NSEventModifierFlagCapsLock) != 0; }

	if (keyName != NAME_None)
	{
		self.Viewport->InputKey(keyName, isDown ? IE_Pressed : IE_Released);
	}
}
@end

@interface FMacWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) FMacViewport* Viewport;
@end

@implementation FMacWindowDelegate
- (instancetype)init
{
	self = [super init];
	if (self)
	{
		[[NSNotificationCenter defaultCenter] addObserver:self
		                                         selector:@selector(appDidResignActive:)
		                                             name:NSApplicationDidResignActiveNotification
		                                           object:nil];
		[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
		                                                       selector:@selector(spaceChanged:)
		                                                           name:NSWorkspaceActiveSpaceDidChangeNotification
		                                                         object:nil];
	}
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

- (void)spaceChanged:(NSNotification *)notification
{
	debugf(TEXT("Cocoa: spaceChanged - releasing mouse and focus"));
	if (self.Viewport)
	{
		self.Viewport->OnWindowLostFocus();
	}
}

- (void)windowDidChangeOcclusionState:(NSNotification *)notification
{
	NSWindow* win = (NSWindow*)[notification object];
	if (win && !([win occlusionState] & NSWindowOcclusionStateVisible))
	{
		debugf(TEXT("Cocoa: windowDidChangeOcclusionState occluded - releasing mouse and focus"));
		if (self.Viewport)
		{
			self.Viewport->OnWindowLostFocus();
		}
	}
}

- (void)appDidResignActive:(NSNotification *)notification
{
	debugf(TEXT("Cocoa: appDidResignActive - releasing mouse and focus"));
	if (self.Viewport)
	{
		self.Viewport->OnWindowLostFocus();
	}
}

- (void)windowDidResignKey:(NSNotification *)notification
{
	debugf(TEXT("Cocoa: windowDidResignKey - releasing mouse and focus"));
	if (self.Viewport)
	{
		self.Viewport->OnWindowLostFocus();
	}
}

- (void)windowDidResignMain:(NSNotification *)notification
{
	debugf(TEXT("Cocoa: windowDidResignMain - releasing mouse and focus"));
	if (self.Viewport)
	{
		self.Viewport->OnWindowLostFocus();
	}
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	debugf(TEXT("Cocoa: windowDidBecomeKey - received focus"));
	if (self.Viewport)
	{
		self.Viewport->OnWindowReceivedFocus();
	}
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
	debugf(TEXT("Cocoa: windowShouldClose received, exiting"));
	appRequestExit(0);
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
	, WindowDelegate(NULL)
	, bCapturingMouseInput(FALSE)
	, bCursorVisible(TRUE)
	, bIsFocused(TRUE)
	, AccumulatedMouseDeltaX(0.0f)
	, AccumulatedMouseDeltaY(0.0f)
	, MousePositionX(0)
	, MousePositionY(0)
{
	Resize(InSizeX, InSizeY, InFullscreen);
}

FMacViewport::~FMacViewport()
{
	Destroy();
}

void FMacViewport::Destroy()
{
	SetMouseCapture(FALSE);
	CGAssociateMouseAndMouseCursorPosition(true);
	MacUnhideCursor();
	PressedKeys.Empty();

	if( ViewportClient )
	{
		ViewportClient = NULL;
		UpdateViewportRHI(TRUE, 0, 0, FALSE);
	}
	if( WindowDelegate )
	{
		@autoreleasepool {
			FMacWindowDelegate* Delegate = (FMacWindowDelegate*)CFBridgingRelease(WindowDelegate);
			Delegate.Viewport = NULL;
			WindowDelegate = NULL;
		}
	}
	if( WindowHandle )
	{
		@autoreleasepool {
			NSWindow* Window = (NSWindow*)CFBridgingRelease(WindowHandle);
			[Window setDelegate:nil];
			[Window close];
			WindowHandle = NULL;
		}
	}
}

void FMacViewport::InputKey(FName Key, EInputEvent Event, FLOAT AmountDepressed)
{
	if (Event == IE_Pressed)
	{
		PressedKeys.AddUniqueItem(Key);
	}
	else if (Event == IE_Released)
	{
		PressedKeys.RemoveItem(Key);
	}

	if (GetClient())
	{
		GetClient()->InputKey(this, 0, Key, Event, AmountDepressed, FALSE);
	}
}

void FMacViewport::InputChar(TCHAR Character)
{
	if (GetClient())
	{
		GetClient()->InputChar(this, 0, Character);
	}
}

void FMacViewport::MouseMove(INT X, INT Y)
{
	if (GetClient())
	{
		GetClient()->MouseMove(this, X, Y);
	}
}

UBOOL FMacViewport::KeyState(FName Key) const
{
	return PressedKeys.ContainsItem(Key);
}

void FMacViewport::SetMouse(INT x, INT y)
{
	MousePositionX = x;
	MousePositionY = y;
}

void FMacViewport::ProcessInput(FLOAT DeltaTime)
{
	DispatchMouseInput(DeltaTime);
}

void FMacViewport::CaptureMouse(UBOOL bCapture)
{
	SetMouseCapture(bCapture);
}

void FMacViewport::LockMouseToWindow(UBOOL bLock)
{
	SetMouseCapture(bLock);
}

void FMacViewport::ShowCursor(UBOOL bVisible)
{
	if (bCursorVisible != bVisible)
	{
		bCursorVisible = bVisible;
		if (bVisible)
		{
			MacUnhideCursor();
		}
		else
		{
			MacHideCursor();
		}
	}
}

void FMacViewport::SetMouseCapture(UBOOL bCapture)
{
	if (bCapturingMouseInput != bCapture)
	{
		bCapturingMouseInput = bCapture;
		if (bCapture && bIsFocused)
		{
			CGAssociateMouseAndMouseCursorPosition(false);
			MacHideCursor();
			bCursorVisible = FALSE;
		}
		else
		{
			CGAssociateMouseAndMouseCursorPosition(true);
			MacUnhideCursor();
			bCursorVisible = TRUE;
		}
	}
}

void FMacViewport::OnWindowLostFocus()
{
	bIsFocused = FALSE;
	bCapturingMouseInput = FALSE;

	// Release mouse capture immediately when out of focus!
	CGAssociateMouseAndMouseCursorPosition(true);
	MacUnhideCursor();
	bCursorVisible = TRUE;

	// Release all keys that were pressed so none get stuck down
	if (GetClient())
	{
		for (INT i = 0; i < PressedKeys.Num(); i++)
		{
			GetClient()->InputKey(this, 0, PressedKeys(i), IE_Released, 0.0f, FALSE);
		}
	}
	PressedKeys.Empty();

	AccumulatedMouseDeltaX = 0.0f;
	AccumulatedMouseDeltaY = 0.0f;

	if (GetClient())
	{
		GetClient()->LostFocus(this);
	}
}

void FMacViewport::OnWindowReceivedFocus()
{
	bIsFocused = TRUE;
	if (GetClient())
	{
		GetClient()->ReceivedFocus(this);
	}
	if (bCapturingMouseInput)
	{
		CGAssociateMouseAndMouseCursorPosition(false);
		MacHideCursor();
		bCursorVisible = FALSE;
	}
}

void FMacViewport::DispatchMouseInput(FLOAT DeltaTime)
{
	if (bCapturingMouseInput && bIsFocused && GetClient())
	{
		if (AccumulatedMouseDeltaX != 0.0f)
		{
			GetClient()->InputAxis(this, 0, KEY_MouseX, AccumulatedMouseDeltaX, DeltaTime, FALSE);
			AccumulatedMouseDeltaX = 0.0f;
		}
		if (AccumulatedMouseDeltaY != 0.0f)
		{
			GetClient()->InputAxis(this, 0, KEY_MouseY, AccumulatedMouseDeltaY, DeltaTime, FALSE);
			AccumulatedMouseDeltaY = 0.0f;
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
			[Window setReleasedWhenClosed:NO];
			[Window setHidesOnDeactivate:NO];
			[Window setCanHide:NO];
			[Window setCollectionBehavior:NSWindowCollectionBehaviorDefault | NSWindowCollectionBehaviorParticipatesInCycle | NSWindowCollectionBehaviorManaged];

			if( InPosX < 0 && InPosY < 0 )
			{
				[Window center];
			}

			FMacContentView* ContentView = [[FMacContentView alloc] initWithFrame:FrameRect];
			ContentView.Viewport = this;
			[ContentView setWantsLayer:YES];
			CAMetalLayer* MetalLayer = [CAMetalLayer layer];
			[MetalLayer setTransform:CATransform3DMakeScale(1.0, -1.0, 1.0)];
			[ContentView setLayer:MetalLayer];
			[Window setContentView:ContentView];

			FMacWindowDelegate* Delegate = [[FMacWindowDelegate alloc] init];
			Delegate.Viewport = this;
			[Window setDelegate:Delegate];
			WindowDelegate = (void*)CFBridgingRetain(Delegate);

			[Window makeFirstResponder:ContentView];
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

void UMacClient::FinishDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		for (INT ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
		{
			Viewports(ViewportIndex)->Destroy();
			delete Viewports(ViewportIndex);
		}
		Viewports.Empty();
	}
	Super::FinishDestroy();
}

void UMacClient::Init(UEngine* InEngine)
{
	Engine = InEngine;
	debugf(NAME_Init, TEXT("MacClient initialized."));
}

void UMacClient::Tick(FLOAT DeltaTime)
{
	appMacPumpCocoaEvents();
	for (INT ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
	{
		FMacViewport* VP = Viewports(ViewportIndex);
		if (VP && VP->bCapturingMouseInput)
		{
			NSWindow* Window = (__bridge NSWindow*)VP->WindowHandle;
			if (Window && ![Window isKeyWindow] && ![Window isMainWindow])
			{
				VP->OnWindowLostFocus();
			}
		}
		VP->DispatchMouseInput(DeltaTime);
	}
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
	return MacKeyCodeToFName((unsigned short)KeyCode);
}

#endif // PLATFORM_MACOSX
