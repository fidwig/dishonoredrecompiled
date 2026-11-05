/*=============================================================================
	UnMacClient.h: macOS platform client and viewport interface.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNMACCLIENT_H__
#define __UNMACCLIENT_H__

#if PLATFORM_MACOSX

class UMacClient;

/**
 * macOS platform viewport implementation.
 * Wraps FViewportFrame and FViewport.
 */
class FMacViewport : public FViewportFrame, public FViewport
{
public:
	UMacClient* Client;
	FString Name;

	FMacViewport(UMacClient* InClient, FViewportClient* InViewportClient, const TCHAR* InName, UINT InSizeX, UINT InSizeY, UBOOL InFullscreen);
	virtual ~FMacViewport();

	void Destroy();

	// FViewportFrame interface
	virtual FViewport* GetViewport() { return this; }
	virtual void Resize(UINT NewSizeX, UINT NewSizeY, UBOOL NewFullscreen, INT InPosX = -1, INT InPosY = -1);

	// FViewport interface
	virtual void* GetWindow() { return NULL; }
	virtual FViewportFrame* GetViewportFrame() { return this; }
	virtual UBOOL CaptureJoystickInput(UBOOL Capture) { return TRUE; }
	virtual UBOOL KeyState(FName Key) const { return FALSE; }
	virtual INT GetMouseX() { return 0; }
	virtual INT GetMouseY() { return 0; }
	virtual void GetMousePos( FIntPoint& MousePosition ) { MousePosition = FIntPoint(0, 0); }
	virtual void SetMouse(INT x, INT y) {}
	virtual void ProcessInput( FLOAT DeltaTime ) {}
	virtual void InvalidateDisplay() {}
};

/**
 * macOS platform client implementation.
 */
class UMacClient : public UClient
{
	DECLARE_CLASS_INTRINSIC(UMacClient, UClient, CLASS_Transient|CLASS_Config, Engine)

	// Static variables.
	static TArray<FMacViewport*> Viewports;

	// Variables.
	UEngine* Engine;

	UMacClient();
	void StaticConstructor();

	// UClient interface.
	virtual void Init(UEngine* InEngine);
	virtual void Tick(FLOAT DeltaTime);
	virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar=*GLog);
	virtual void AllowMessageProcessing(UBOOL InValue);
	virtual FViewportFrame* CreateViewportFrame(FViewportClient* ViewportClient, const TCHAR* InName, UINT SizeX, UINT SizeY, UBOOL Fullscreen = 0);
	virtual FViewport* CreateWindowChildViewport(FViewportClient* ViewportClient, void* ParentWindow, UINT SizeX=0, UINT SizeY=0, INT InPosX = -1, INT InPosY = -1);
	virtual void CloseViewport(FViewport* Viewport);
	virtual class UAudioDevice* GetAudioDevice();
	virtual void ForceClearForceFeedback();
	virtual FName GetVirtualKeyName(INT KeyCode) const;
};

#endif // PLATFORM_MACOSX
#endif // __UNMACCLIENT_H__
