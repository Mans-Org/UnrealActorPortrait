// Mans Isaksson 2020

#include "SActorPortrait.h"
#include "ActorPortraitModule.h"
#include "ActorPortraitInterface.h"
#include "ActorPortraitScene.h"

#include "Components/LineBatchComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMaterialLibrary.h"

#include "Engine/World.h"
#include "Engine/GameEngine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/Package.h"

#include "EngineUtils.h"
#include "UnrealEngine.h"
#include "DrawDebugHelpers.h"
#include "SceneInterface.h"

#define LOCTEXT_NAMESPACE "SActorPortrait"

class FActorPortraitSceneSet
{
	TMap<UWorld*, TWeakPtr<SActorPortrait>> PortraitWorlds;

public:
	FORCEINLINE void Add(UWorld* World, TWeakPtr<SActorPortrait> PortraitWidget)
	{
		PortraitWorlds.Add(World, PortraitWidget);
	}

	FORCEINLINE void Remove(UWorld* World)
	{
		PortraitWorlds.Remove(World);
	}

	FORCEINLINE bool Contains(UWorld* World) const
	{
		return PortraitWorlds.Contains(World);
	}

	FORCEINLINE TWeakPtr<SActorPortrait> FindPortraitWidgetFromWorld(UWorld* World)
	{
		auto* PortraitWidget = PortraitWorlds.Find(World);
		if (PortraitWidget)
		{
			return *PortraitWidget;
		}
		return TWeakPtr<SActorPortrait>(nullptr);
	}
} PortraitWorlds;

void SActorPortrait::Construct(const FArguments& InArgs)
{
	PortraitUserData               = InArgs._PortraitUserData;
	ColorAndOpacity                = InArgs._ColorAndOpacity;
	PortraitCameraSettings         = InArgs._PortraitCameraSettings;
	PostProcessingSettings         = InArgs._PostProcessingSettings;
	PortraitSize                   = InArgs._PortraitSize;
	RenderResolutionOverride       = InArgs._RenderResolutionOverride;
	ResolutionScale                = InArgs._ResolutionScale;
	MouseCaptureMode               = InArgs._MouseCaptureMode;
	bLockDuringCapture             = InArgs._bLockDuringCapture;
	bTickWorld                     = InArgs._bTickWorld;
	bRealTime                      = InArgs._bRealTime;
	bShouldShowMouseCursor         = InArgs._bShouldShowMouseCursor;
	CaptureSource                  = InArgs._CaptureSource;
	RenderMaterial                 = InArgs._RenderMaterial;
	RenderMaterialTextureParameter = InArgs._RenderMaterialTextureParameter;

	OnInputTouchEvent              = InArgs._OnInputTouchEvent;
	OnTouchGestureEvent            = InArgs._OnTouchGestureEvent;
	OnInputMotionEvent             = InArgs._OnInputMotionEvent;
	OnInputKeyEvent                = InArgs._OnInputKeyEvent;
	OnInputAxisEvent               = InArgs._OnInputAxisEvent;
	OnReceiveFocusEvent            = InArgs._OnReceiveFocusEvent;
	OnLostFocusEvent               = InArgs._OnLostFocusEvent;
	OnCapturedMouseMoveEvent       = InArgs._OnCapturedMouseMoveEvent;
	OnMouseMoveEvent               = InArgs._OnMouseMoveEvent;
	OnMouseEnterEvent              = InArgs._OnMouseEnterEvent;
	OnMouseLeaveEvent              = InArgs._OnMouseLeaveEvent;
	OnSpawnPortraitActorEvent      = InArgs._OnSpawnPortraitActorEvent;
	PreSpawnPortraitActorEvent     = InArgs._PreSpawnPortraitActorEvent;
	PostCameraResetEvent           = InArgs._PostCameraResetEvent;

	SetContent(InArgs._Content.Widget);

	RecreatePortraitScene(InArgs._WorldAsset, InArgs._PortraitActorClass, InArgs._PortraitActorTransform, InArgs._SkySphereClass, InArgs._DirectionalLightTemplate, InArgs._SkyLightTemplate, InArgs._OwningGameInstance);
}

SActorPortrait::~SActorPortrait()
{
	UMaterialInstanceDynamic* RenderMaterialInstance = Cast<UMaterialInstanceDynamic>(Brush.GetResourceObject());
	if (RenderMaterialInstance != nullptr)
	{
		RenderMaterialInstance->RemoveFromRoot();
		RenderMaterialInstance->MarkAsGarbage();
	}

	PortraitWorlds.Remove(GetPortraitWorld());
	PortraitScene.Reset();
}

void SActorPortrait::Tick(const FGeometry& AllottedGeometry, const double CurrentTime, const float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SActorPortrait_Tick);

	UpdateCachedGeometry(AllottedGeometry);

	// If real-time, always re-draw the portrait
	const bool bIsRealTime = bRealTime.Get();
	if (bIsRealTime)
	{
		MarkRenderStateDirty();
	}

	UWorld* PortraitWorld = GetPortraitWorld();
	if (!PortraitWorld)
	{
		return;
	}

	PortraitWorld->SetShouldTick(bTickWorld.Get());

	#if WITH_EDITOR // Ticking is handled by UGameEngine::Tick in non-editor builds
	PortraitScene->EditorTick(DeltaTime);
	#endif

	USceneCaptureComponent2D* CaptureComponent = GetCaptureComponent();

	if (bRenderStateDirty && IsValid(CaptureComponent))
	{
		const FIntPoint NewRenderSize = GetRenderSizeXY();
		if (CaptureComponent->TextureTarget == nullptr || NewRenderSize.X != CaptureComponent->TextureTarget->SizeX || NewRenderSize.Y != CaptureComponent->TextureTarget->SizeY)
		{
			ResizeRenderTarget(NewRenderSize);

			if (PortraitCameraSettings.Get().bResetCameraOnViewportResize)
				bCameraNeedsReset = true;
		}
		
		CaptureComponent->PostProcessSettings = ViewInfo.PostProcessSettings;
		CaptureComponent->PostProcessBlendWeight = ViewInfo.PostProcessBlendWeight;
		CaptureComponent->CaptureSource = CaptureSource.Get();
		CaptureComponent->bCaptureEveryFrame = bIsRealTime; // Improves performance to have this true if we're capturing every frame

		ViewInfo.AspectRatio = NewRenderSize.X > 0 && NewRenderSize.Y > 0 ? (float)NewRenderSize.X / (float)NewRenderSize.Y : 1.f;
		ViewInfo.bConstrainAspectRatio = false;
		ViewInfo.PostProcessBlendWeight = 1.f;
		ViewInfo.PostProcessSettings    = PostProcessingSettings.Get();

		// Disable Vignette on mobile since it causes the image to turn dark
#if (PLATFORM_ANDROID || PLATFORM_IOS)
		ViewInfo.PostProcessSettings.bOverride_VignetteIntensity = true;
		ViewInfo.PostProcessSettings.VignetteIntensity           = 0.f;
#endif

		if (FSceneInterface* Scene = PortraitWorld->Scene)
		{
			LastFrameNumber = Scene->GetFrameNumber();
		}
	}

	const bool bFlushViewInfoToCaptureComponent = IsValid(CaptureComponent) && (bCameraNeedsReset || bRenderStateDirty);

	if (bCameraNeedsReset)
	{
		ResetCamera();
	}

	bRenderStateDirty = false;
	bCameraNeedsReset = false;

	if (bFlushViewInfoToCaptureComponent)
	{
		CaptureComponent->SetCameraView(ViewInfo);
	}
}

int32 SActorPortrait::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (FSceneInterface* Scene = PortraitScene->GetScene())
	{
		if (LastFrameNumber == Scene->GetFrameNumber())
		{
			GetPortraitWorld()->SendAllEndOfFrameUpdates();
			Scene->IncrementFrameNumber();
			PortraitScene->UpdateCaptureComponentCaptureContents();
		}
	}

	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);

	const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle) * Brush.GetTint(InWidgetStyle));
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &Brush, DrawEffects, FinalColorAndOpacity);

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bIsEnabled);
}

FVector2D SActorPortrait::ComputeDesiredSize(float LayoutScaleMultiplier) const 
{ 
	TOptional<FIntPoint> ResolutionOverride = RenderResolutionOverride.Get();
	if (ResolutionOverride.IsSet())
	{
		return ResolutionOverride.GetValue();
	}
	else
	{
		return PortraitSize.Get() * LayoutScaleMultiplier;
	}
}

FReply SActorPortrait::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled().PreventThrottling();
	++NumTouches;

	UpdateCachedGeometry(MyGeometry);
	UpdateCachedCursorPos(MyGeometry, TouchEvent);

	const FVector2D TouchPosition = CachedCursorPos;

	if (OnInputTouchEvent.IsBound())
	{
		CurrentReplyState = OnInputTouchEvent.Execute(MyGeometry, TouchEvent);

		if (CurrentReplyState.IsEventHandled())
		{
			const bool bTemporaryCapture = MouseCaptureMode.Get() == EMouseCaptureMode::CaptureDuringMouseDown;
			if (bTemporaryCapture)
			{
				CurrentReplyState = AcquireFocusAndCapture(FIntPoint(TouchEvent.GetScreenSpacePosition().X, TouchEvent.GetScreenSpacePosition().Y), EFocusCause::Mouse);
			}
		}
	}
	else
	{
		CurrentReplyState = FReply::Unhandled();
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	UpdateCachedGeometry(MyGeometry);
	UpdateCachedCursorPos(MyGeometry, TouchEvent);

	if (OnInputTouchEvent.IsBound())
	{
		CurrentReplyState = OnInputTouchEvent.Execute(MyGeometry, TouchEvent);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	UpdateCachedGeometry(MyGeometry);
	UpdateCachedCursorPos(MyGeometry, TouchEvent);

	if (--NumTouches <= 0)
	{
		CachedCursorPos = FIntPoint(-1, -1);
	}

	if (OnInputTouchEvent.IsBound())
	{
		CurrentReplyState = OnInputTouchEvent.Execute(MyGeometry, TouchEvent);
	}

	if (MouseCaptureMode.Get() == EMouseCaptureMode::CaptureDuringMouseDown)
	{
		CurrentReplyState.ReleaseMouseCapture();
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	UpdateCachedCursorPos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if (OnInputTouchEvent.IsBound())
	{
		CurrentReplyState = OnInputTouchEvent.Execute(MyGeometry, TouchEvent);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	UpdateCachedCursorPos(MyGeometry, TouchEvent);
	UpdateCachedGeometry(MyGeometry);

	if (OnInputTouchEvent.IsBound())
	{
		CurrentReplyState = OnInputTouchEvent.Execute(MyGeometry, TouchEvent);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	UpdateCachedGeometry(MyGeometry);
	UpdateCachedCursorPos(MyGeometry, GestureEvent);

	if (OnTouchGestureEvent.IsBound())
	{
		CurrentReplyState = OnTouchGestureEvent.Execute(MyGeometry, GestureEvent);
	}

	return CurrentReplyState;
}

TOptional<TSharedRef<SWidget>> SActorPortrait::OnMapCursor(const FCursorReply& CursorReply) const
{
	/*if (OnMapCursorEvent.IsBound())
		return OnMapCursorEvent.Execute(CursorReply);*/
	
	return TOptional<TSharedRef<SWidget>>();
}

FReply SActorPortrait::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Start a new reply state
	// Prevent throttling when interacting with the portrait so we can move around in it
	CurrentReplyState = FReply::Handled().PreventThrottling();

	UpdateCachedGeometry(InGeometry);
	UpdateCachedCursorPos(InGeometry, InMouseEvent);

	// If we're obtaining focus, we have to copy the modifier key states prior to processing this mouse button event, as this is the only point at which the mouse down
	// event is processed when focus initially changes and the modifier keys need to be in-place to detect any unique drag-like events.
	if (FSlateApplication::Get().GetUserFocusedWidget(0).Get() != this)
	{
		ApplyModifierKeys(FSlateApplication::Get().GetModifierKeys());
	}

	const bool bTemporaryCapture = 
		MouseCaptureMode.Get() == EMouseCaptureMode::CaptureDuringMouseDown ||
		(MouseCaptureMode.Get() == EMouseCaptureMode::CaptureDuringRightMouseDown && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton);

	// Process primary input if we already have capture, or we are permanent capture that doesn't consume the mouse down.
	const bool bProcessInputPrimary = HasMouseCapture() || (MouseCaptureMode.Get() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
	const bool bAnyMenuWasVisible = FSlateApplication::Get().AnyMenusVisible();

	// Process the mouse event
	if (bTemporaryCapture || bProcessInputPrimary)
	{
		if (OnInputKeyEvent.IsBound())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(InMouseEvent.GetEffectingButton(), CharacterCode, KeyCode);
			const auto KeyEvent = FKeyEvent(InMouseEvent.GetEffectingButton(), InMouseEvent.GetModifierKeys(), InMouseEvent.GetUserIndex(), InMouseEvent.IsRepeat(), CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0);

			CurrentReplyState = OnInputKeyEvent.Execute(InGeometry, KeyEvent, IE_Pressed);
		}
		else
		{
			CurrentReplyState = FReply::Unhandled();
		}
	}

	// a new menu was opened if there was previously not a menu visible but now there is
	const bool bNewMenuWasOpened = !bAnyMenuWasVisible && FSlateApplication::Get().AnyMenusVisible();

	const bool bPermanentCapture =
		(MouseCaptureMode.Get() == EMouseCaptureMode::CapturePermanently) ||
		(MouseCaptureMode.Get() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);

	if (FSlateApplication::Get().IsActive() && !bIgnoreInput.Get() &&
		!bNewMenuWasOpened && // We should not focus the portrait if a menu was opened as it would close the menu
		(bPermanentCapture || bTemporaryCapture))
	{
		CurrentReplyState = AcquireFocusAndCapture(FIntPoint(InMouseEvent.GetScreenSpacePosition().X, InMouseEvent.GetScreenSpacePosition().Y), EFocusCause::Mouse);
	}

	// Re-set prevent throttling here as it can get reset when inside of InputKey()
	CurrentReplyState.PreventThrottling();

	return CurrentReplyState;
}

FReply SActorPortrait::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	UpdateCachedGeometry(InGeometry);
	UpdateCachedCursorPos(InGeometry, InMouseEvent);

	bool bCursorVisible = true;
	bool bReleaseMouseCapture = true;

	if (OnInputKeyEvent.IsBound())
	{
		const uint32* CharacterCode;
		const uint32* KeyCode;
		FInputKeyManager::Get().GetCodesFromKey(InMouseEvent.GetEffectingButton(), CharacterCode, KeyCode);
		const auto KeyEvent = FKeyEvent(InMouseEvent.GetEffectingButton(), InMouseEvent.GetModifierKeys(), InMouseEvent.GetUserIndex(), InMouseEvent.IsRepeat(), CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0);
		
		CurrentReplyState =	OnInputKeyEvent.Execute(InGeometry, KeyEvent, IE_Released);
	}

	bCursorVisible = GetCursor().Get(EMouseCursor::Default) != EMouseCursor::None;

	bReleaseMouseCapture =
		bCursorVisible ||
		MouseCaptureMode.Get() == EMouseCaptureMode::CaptureDuringMouseDown ||
		(MouseCaptureMode.Get() == EMouseCaptureMode::CaptureDuringRightMouseDown && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton);

	if (bReleaseMouseCapture)
	{
		// On mouse up outside of the portrait or if the cursor is visible in game, we should make sure the mouse is no longer captured
		// as long as the left or right mouse buttons are not still down
		if (!InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			if (bCursorHiddenDueToCapture)
			{
				bCursorHiddenDueToCapture = false;
				CurrentReplyState.SetMousePos(MousePosBeforeHiddenDueToCapture);
				MousePosBeforeHiddenDueToCapture = FIntPoint(-1, -1);
			}

			CurrentReplyState.ReleaseMouseCapture();

			if (bCursorVisible)
			{
				CurrentReplyState.ReleaseMouseLock();
			}
		}
	}

	return CurrentReplyState;
}

void SActorPortrait::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateCachedCursorPos(MyGeometry, MouseEvent);
	
	if (OnMouseEnterEvent.IsBound())
		OnMouseEnterEvent.Execute(MyGeometry, MouseEvent);
}

void SActorPortrait::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if (OnMouseLeaveEvent.IsBound())
		OnMouseLeaveEvent.Execute(CachedGeometry, MouseEvent);
}

FReply SActorPortrait::OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedGeometry(InGeometry);
	UpdateCachedCursorPos(InGeometry, InMouseEvent);

	const bool bHasCapture = HasMouseCapture();

	if (bHasCapture && OnCapturedMouseMoveEvent.IsBound())
	{
		OnCapturedMouseMoveEvent.Execute(InGeometry, InMouseEvent);
	}
	else if (OnMouseMoveEvent.IsBound())
	{
		OnMouseMoveEvent.Execute(InGeometry, InMouseEvent);
	}

	if (bHasCapture)
	{
		// Accumulate delta changes to mouse movement. Depending on the sample frequency of a mouse we may get many per frame.
		const FVector2D CursorDelta = InMouseEvent.GetCursorDelta();
		MouseDelta.X += CursorDelta.X;
		++NumMouseSamplesX;

		MouseDelta.Y -= CursorDelta.Y;
		++NumMouseSamplesY;

		MouseDeltaUserIndex = InMouseEvent.GetUserIndex();
	}

	if (bCursorHiddenDueToCapture)
	{
		// If hidden during capture, don't actually move the cursor
		FVector2D RevertedCursorPos(MousePosBeforeHiddenDueToCapture.X, MousePosBeforeHiddenDueToCapture.Y);
		FSlateApplication::Get().SetCursorPos(RevertedCursorPos);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Handled();

	UpdateCachedGeometry(InGeometry);
	UpdateCachedCursorPos(InGeometry, InMouseEvent);

	// The portrait accepts two different keys depending on the direction of scroll.
	const FKey PortraitKey = InMouseEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;
	const uint32* CharacterCode;
	const uint32* KeyCode;
	FInputKeyManager::Get().GetCodesFromKey(PortraitKey, CharacterCode, KeyCode);

	// Pressed and released should be sent
	if (OnInputKeyEvent.IsBound())
	{
		const auto KeyEvent = FKeyEvent(PortraitKey, InMouseEvent.GetModifierKeys(), InMouseEvent.GetUserIndex(), InMouseEvent.IsRepeat(), CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0);
		OnInputKeyEvent.Execute(InGeometry, KeyEvent, IE_Pressed);
		OnInputKeyEvent.Execute(InGeometry, KeyEvent, IE_Released);
	}

	if (OnInputAxisEvent.IsBound())
	{
		OnInputAxisEvent.Execute(InGeometry, FAnalogInputEvent(EKeys::MouseWheelAxis, InMouseEvent.GetModifierKeys(), 0, InMouseEvent.IsRepeat(), 
			CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0, InMouseEvent.GetWheelDelta()));
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled(); 

	// Note: When double-clicking, the following message sequence is sent:
	//	WM_*BUTTONDOWN
	//	WM_*BUTTONUP
	//	WM_*BUTTONDBLCLK	(Needs to set the KeyStates[*] to true)
	//	WM_*BUTTONUP

	UpdateCachedGeometry(InGeometry);
	UpdateCachedCursorPos(InGeometry, InMouseEvent);

	if (OnInputKeyEvent.IsBound())
	{
		const uint32* CharacterCode;
		const uint32* KeyCode;
		FInputKeyManager::Get().GetCodesFromKey(InMouseEvent.GetEffectingButton(), CharacterCode, KeyCode);
		const auto KeyEvent = FKeyEvent(InMouseEvent.GetEffectingButton(), InMouseEvent.GetModifierKeys(), InMouseEvent.GetUserIndex(), InMouseEvent.IsRepeat(), CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0);
		CurrentReplyState = OnInputKeyEvent.Execute(InGeometry, KeyEvent, IE_DoubleClick);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	if (InKeyEvent.GetKey().IsValid() && OnInputKeyEvent.IsBound())
	{
		CurrentReplyState = OnInputKeyEvent.Execute(InGeometry, InKeyEvent, InKeyEvent.IsRepeat() ? IE_Repeat : IE_Pressed);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	if (InKeyEvent.GetKey().IsValid() && OnInputKeyEvent.IsBound())
	{
		CurrentReplyState = OnInputKeyEvent.Execute(InGeometry, InKeyEvent, IE_Released);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	if (InAnalogInputEvent.GetKey().IsValid() && OnInputAxisEvent.IsBound())
	{
		CurrentReplyState = OnInputAxisEvent.Execute(MyGeometry, InAnalogInputEvent);
	}

	return CurrentReplyState;
}

FReply SActorPortrait::OnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	// TODO: Why is this commented out?
	/*if (OnInputCharEvent.IsBound())
	{
		CurrentReplyState = OnInputCharEvent.Execute(InCharacterEvent.GetUserIndex(), InCharacterEvent.GetCharacter());
	}*/

	return CurrentReplyState;
}

FReply SActorPortrait::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	CurrentReplyState = FReply::Handled();

	if (InFocusEvent.GetUser() == FSlateApplication::Get().GetUserIndexForKeyboard())
	{
		if (OnReceiveFocusEvent.IsBound())
			CurrentReplyState = OnReceiveFocusEvent.Execute(MyGeometry, InFocusEvent);

		const bool bPermanentCapture = InFocusEvent.GetCause() == EFocusCause::Mouse 
			&& (MouseCaptureMode.Get() == EMouseCaptureMode::CapturePermanently 
			|| MouseCaptureMode.Get() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);

		if (FSlateApplication::Get().IsActive() && bPermanentCapture && !bIgnoreInput.Get())
		{
			return AcquireFocusAndCapture(GetSizeXY() / 2, EFocusCause::Mouse);
		}
	}

	return CurrentReplyState;
}

void SActorPortrait::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	// If the focus loss event isn't the for the primary 'keyboard' user, don't worry about it.
	if (InFocusEvent.GetUser() != FSlateApplication::Get().GetUserIndexForKeyboard())
	{
		return;
	}

	bShouldCaptureMouseOnActivate = false;
	bCursorHiddenDueToCapture = false;

	if (OnLostFocusEvent.IsBound())
		OnLostFocusEvent.Execute(CachedGeometry, InFocusEvent);
}

FReply SActorPortrait::OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& MotionEvent)
{
	// Start a new reply state
	CurrentReplyState = FReply::Unhandled();

	if (OnInputMotionEvent.IsBound())
	{
		CurrentReplyState = OnInputMotionEvent.Execute(MyGeometry, MotionEvent);
	}

	return CurrentReplyState;
}


FCursorReply SActorPortrait::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (bCursorHiddenDueToCapture)
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}

	return FCursorReply::Cursor(GetCursor().Get(EMouseCursor::Default));
}

void SActorPortrait::OnFinishedPointerInput()
{
	ProcessAccumulatedPointerInput();
}

void SActorPortrait::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SkySphereActor);
	Collector.AddReferencedObject(PortraitActor);
	Collector.AddReferencedObject(PortraitUserData);
	Collector.AddReferencedObject(RenderMaterial);
	Brush.AddReferencedObjects(Collector);
}

FString SActorPortrait::GetReferencerName() const
{
	return FString::Printf(TEXT("SActorPortrait_%s"), *GetReadableLocation());
}

void SActorPortrait::SetContent(TSharedPtr<SWidget> InContent)
{
	ChildSlot
	[
		InContent.IsValid() ? InContent.ToSharedRef() : (TSharedRef<SWidget>)SNullWidget::NullWidget
	];
}

const TSharedPtr<SWidget> SActorPortrait::GetContent() const 
{ 
	return ChildSlot.GetWidget(); 
}

void SActorPortrait::SetPortraitUserData(UObject* InPortraitUserData, bool bRecaptureSky)
{
	PortraitUserData = InPortraitUserData;
	if (bRecaptureSky)
	{
		RecaptureSky();
	}
}

void SActorPortrait::SetRenderMaterial(UMaterialInterface* InRenderMaterial, FName InRenderMaterialTextureParameter)
{
	if (RenderMaterial != InRenderMaterial || InRenderMaterialTextureParameter != RenderMaterialTextureParameter)
	{
		RenderMaterial = InRenderMaterial;
		RenderMaterialTextureParameter = InRenderMaterialTextureParameter;
		RecreateRenderMaterial();
	}
}

void SActorPortrait::SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	SetAttribute(ColorAndOpacity, InColorAndOpacity, EInvalidateWidgetReason::Paint);
}

void SActorPortrait::SetPortraitCameraSettings(const TAttribute<FPortraitCameraSettings>& InPortraitCameraSettings, bool bResetCamera)
{
	PortraitCameraSettings = InPortraitCameraSettings;
	if (bResetCamera)
	{
		ResetCamera();
	}
}

void SActorPortrait::SetPostProcessSettings(const TAttribute<FPostProcessSettings> &NewPostProcessSettings)
{
	PostProcessingSettings = NewPostProcessSettings;
	MarkRenderStateDirty();
}

void SActorPortrait::SetMouseCaptureMode(const TAttribute<EMouseCaptureMode>& InMouseCaptureMode)
{
	MouseCaptureMode = InMouseCaptureMode;
}

void SActorPortrait::SetLockDuringCapture(const TAttribute<bool>& InLockDuringCapture)
{
	bLockDuringCapture = InLockDuringCapture;
}

void SActorPortrait::SetIgnoreInput(const TAttribute<bool>& InIgnoreInput)
{
	bIgnoreInput = InIgnoreInput;
}

void SActorPortrait::SetRealTime(const TAttribute<bool>& InRealTime)
{
	bRealTime = InRealTime;
}

void SActorPortrait::SetTickWorld(const TAttribute<bool>& InTickWorld)
{
	bTickWorld = InTickWorld;

	if (UWorld* PortraitWorld = GetPortraitWorld())
		PortraitWorld->SetShouldTick(bTickWorld.Get());
}

void SActorPortrait::SetCaptureSource(const TAttribute<ESceneCaptureSource>& InCaptrueSource)
{
	SetAttributeWithSideEffect(CaptureSource, InCaptrueSource, &SActorPortrait::MarkRenderStateDirty);
}

void SActorPortrait::SetPortraitSize(const TAttribute<FVector2D>& InPortraitSize)
{
	SetAttribute(PortraitSize, InPortraitSize, EInvalidateWidgetReason::Layout);
}

void SActorPortrait::SetRenderResolutionOverride(const TAttribute<TOptional<FIntPoint>>& InRenderResolutionOverride)
{
	SetAttributeWithSideEffect(RenderResolutionOverride, InRenderResolutionOverride, [&]() 
	{ 
		MarkRenderStateDirty();

		// Render resolution override also affects layout, need to anvalidate it.
		Invalidate(EInvalidateWidgetReason::Layout);
	});
}

void SActorPortrait::SetResolutionScale(const TAttribute<float>& InResolutionScale)
{
	SetAttributeWithSideEffect(ResolutionScale, InResolutionScale, &SActorPortrait::MarkRenderStateDirty);
}

void SActorPortrait::ResetCamera()
{
	UWorld* PortraitWorld = GetPortraitWorld();
	if (!PortraitWorld || !IsValid(PortraitActor))
	{
		return;
	}

	if (!bHasValidCachedGeometry)
	{
		return;
	}

	OrbitOrigin = FVector::ZeroVector;

	// Clear any debug lines that may have been drawn by enabling CameraSettings.bDrawDebug
	{
		if (PortraitWorld->LineBatcher)
			PortraitWorld->LineBatcher->Flush();
		if (PortraitWorld->PersistentLineBatcher)
			PortraitWorld->PersistentLineBatcher->Flush();
		if (PortraitWorld->ForegroundLineBatcher)
			PortraitWorld->ForegroundLineBatcher->Flush();
	}

	const FTransform& ActorTransform = PortraitActor->GetActorTransform();

	const FPortraitCameraSettings CameraSettings = PortraitCameraSettings.Get();

	const bool bIsPerspective = CameraSettings.ProjectionType == ECameraProjectionMode::Perspective;
	const bool bAutoFrameCamera = !(CameraSettings.bOverride_CustomCameraLocation ||
									CameraSettings.bOverride_CustomCameraRotation ||
									(!bIsPerspective && CameraSettings.bOverride_CustomOrthoWidth));

	FMinimalViewInfo NewViewInfo;
	NewViewInfo.ProjectionMode = CameraSettings.ProjectionType;

	if (bAutoFrameCamera)
	{
		const auto CameraRotation = CameraSettings.CameraRotationOffset.Quaternion() * CameraSettings.CameraOrbitRotation.Quaternion();

		const FIntPoint CurrentRenderSize = GetRenderSizeXY();
		const float AspectRatio = CurrentRenderSize.X > 0 && CurrentRenderSize.Y > 0 
			? (float)CurrentRenderSize.X / (float)CurrentRenderSize.Y 
			: 1.f;

		const static auto CalcActorLocalBounds = [](AActor* InActor, const FPortraitCameraSettings& InCameraSettings)->FBox
		{
			const static auto IsBlacklisted = [](UPrimitiveComponent* PrimitiveComponent, const TSet<UClass*>& Blacklist)->bool
			{
				for (UClass* BlacklistedClass : Blacklist)
				{
					if (PrimitiveComponent->IsA(BlacklistedClass))
						return true;
				}
				return false;
			};

			const static auto CalcPrimitiveBounds = [](UPrimitiveComponent* InPrimitiveComponent)
			{
				FBox OutBounds(EForceInit::ForceInit);
				if (InPrimitiveComponent->bUseAttachParentBound && InPrimitiveComponent->GetAttachParent() != nullptr)
					return OutBounds;

				const static auto CalcSkinnedMeshLocalBounds = [](USkinnedMeshComponent* SkinnedMeshComponent)
				{
					const auto LODIndex = 0;

					const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(SkinnedMeshComponent->GetSkinnedAsset());
					if (!IsValid(SkeletalMesh) 
						|| !SkeletalMesh->GetResourceForRendering()
						|| !SkeletalMesh->GetResourceForRendering()->LODRenderData.IsValidIndex(LODIndex))
						return SkinnedMeshComponent->CalcBounds(FTransform::Identity).GetBox();

					TArray<FVector3f> VertexPositions;
					TArray<FMatrix44f> CachedRefToLocals;
					SkinnedMeshComponent->CacheRefToLocalMatrices(CachedRefToLocals); 

					const FSkeletalMeshLODRenderData& SkelMeshLODData = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];
					const FSkinWeightVertexBuffer* SkinWeightBuffer = SkinnedMeshComponent->GetSkinWeightBuffer(LODIndex);
					if (!SkinWeightBuffer)
						return SkinnedMeshComponent->CalcBounds(FTransform::Identity).GetBox();

					USkinnedMeshComponent::ComputeSkinnedPositions(SkinnedMeshComponent, VertexPositions, CachedRefToLocals, SkelMeshLODData, *SkinWeightBuffer);

					FBox Bounds(EForceInit::ForceInit);
					for (const auto& Position : VertexPositions)
						Bounds += FVector(Position.X, Position.Y, Position.Z);

					return Bounds;
				};

				if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkeletalMeshComponent>(InPrimitiveComponent))
					OutBounds = CalcSkinnedMeshLocalBounds(SkinnedMeshComponent);
				else
					OutBounds = InPrimitiveComponent->CalcBounds(FTransform::Identity).GetBox();

				const FTransform& ActorTransform = InPrimitiveComponent->GetOwner()->GetActorTransform();
				const FTransform& ComponentTransform = InPrimitiveComponent->GetComponentTransform();
				const FTransform ComponentActorSpaceTransform = ActorTransform.GetRelativeTransform(ComponentTransform);

				return OutBounds.TransformBy(ComponentActorSpaceTransform);
			};

			FBox Box(EForceInit::ForceInit);
			for (UActorComponent* ActorComponent : InActor->GetComponents())
			{
				UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ActorComponent);
				if (PrimComp && PrimComp->IsRegistered()
					&& (PrimComp->IsVisible() || InCameraSettings.bIncludeHiddenComponentsInBounds)
					&& !IsBlacklisted(PrimComp, InCameraSettings.ComponentBoundsBlacklist))
				{
					Box += CalcPrimitiveBounds(PrimComp);
				}
			}

			return Box;
		};

		const FBox    LocalBoundingBox  = CameraSettings.bOverride_CustomActorBounds ? CameraSettings.CustomActorBounds : CalcActorLocalBounds(PortraitActor, CameraSettings);
		const FVector LocalBoundsExtent = LocalBoundingBox.GetExtent();
		const FVector LocalBoundsOrigin = LocalBoundingBox.GetCenter();
		const FVector LocalBoundsMin    = LocalBoundsOrigin - LocalBoundsExtent;
		const FVector LocalBoundsMax    = LocalBoundsOrigin + LocalBoundsExtent;
		
		OrbitOrigin = ActorTransform.TransformPosition(LocalBoundsOrigin);

		typedef TArray<FVector, TInlineAllocator<8>> FBoundsVertices;

		const FBoundsVertices BoundsVertices =
		{
			ActorTransform.TransformPosition({ LocalBoundsMin.X, LocalBoundsMin.Y, LocalBoundsMin.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMin.X, LocalBoundsMax.Y, LocalBoundsMin.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMax.X, LocalBoundsMax.Y, LocalBoundsMin.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMax.X, LocalBoundsMin.Y, LocalBoundsMin.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMin.X, LocalBoundsMin.Y, LocalBoundsMax.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMin.X, LocalBoundsMax.Y, LocalBoundsMax.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMax.X, LocalBoundsMax.Y, LocalBoundsMax.Z }),
			ActorTransform.TransformPosition({ LocalBoundsMax.X, LocalBoundsMin.Y, LocalBoundsMax.Z }),
		};

		if (CameraSettings.bDrawDebug)
		{
			const TArray<TPair<const FVector&, const FVector&>> BoundingBoxEdges =
			{
				TPair<const FVector&, const FVector&>(BoundsVertices[0], BoundsVertices[1]),
				TPair<const FVector&, const FVector&>(BoundsVertices[1], BoundsVertices[2]),
				TPair<const FVector&, const FVector&>(BoundsVertices[2], BoundsVertices[3]),
				TPair<const FVector&, const FVector&>(BoundsVertices[3], BoundsVertices[0]),

				TPair<const FVector&, const FVector&>(BoundsVertices[4], BoundsVertices[5]),
				TPair<const FVector&, const FVector&>(BoundsVertices[5], BoundsVertices[6]),
				TPair<const FVector&, const FVector&>(BoundsVertices[6], BoundsVertices[7]),
				TPair<const FVector&, const FVector&>(BoundsVertices[7], BoundsVertices[4]),

				TPair<const FVector&, const FVector&>(BoundsVertices[0], BoundsVertices[4]),
				TPair<const FVector&, const FVector&>(BoundsVertices[1], BoundsVertices[5]),
				TPair<const FVector&, const FVector&>(BoundsVertices[2], BoundsVertices[6]),
				TPair<const FVector&, const FVector&>(BoundsVertices[3], BoundsVertices[7]),
			};

			for (const auto& Edge : BoundingBoxEdges)
			{
				DrawDebugLine(PortraitWorld, Edge.Key, Edge.Value, FColor::Blue, true, -1, -1);
			}
		}

		const FBoundsVertices BoundsVerticesInCameraSpace =
		{
			CameraRotation.UnrotateVector(BoundsVertices[0]),
			CameraRotation.UnrotateVector(BoundsVertices[1]),
			CameraRotation.UnrotateVector(BoundsVertices[2]),
			CameraRotation.UnrotateVector(BoundsVertices[3]),
			CameraRotation.UnrotateVector(BoundsVertices[4]),
			CameraRotation.UnrotateVector(BoundsVertices[5]),
			CameraRotation.UnrotateVector(BoundsVertices[6]),
			CameraRotation.UnrotateVector(BoundsVertices[7]),
		};

		if (bIsPerspective)
		{
			const static auto CalculatePerspectiveViewLocation = [](float InAspectRatio, const FPortraitCameraSettings& InCameraSettings, const FBoundsVertices& InCameraSpaceBoundsVertices)->FVector
			{
				/*
				* Algorighm for calculating the perspective camera location
				* 
				*    Camera
				*      []
				*     /  \
				*    /    \
				*   /      \
				*  c1      c2
				* 
				* p1 *-----* p1
				*    |     |
				* p3 *-----* p4
				* 
				* This algorithm works by building a set of all possible combination of points e.g. [(p1,p2),(p1,p3) ..., (p3, p4)].
				* We then loop through all pair of points and calculate the requred position of the camera to frame those two points. We 
				* then pick the point that moves the camera furthest back.
				* We do this for both the horizontal and vertical component and then merge the results into a final camera location.
				*/

				const FVector LeftCameraFrustrumDir   = FVector::ForwardVector.RotateAngleAxis(InCameraSettings.CameraFOV * 0.5f, -FVector::UpVector);
				const FVector RightCameraFrustrumDir  = LeftCameraFrustrumDir * FVector(1, -1, 1);
				const FVector TopCameraFrustrumDir    = FVector::ForwardVector.RotateAngleAxis((InCameraSettings.CameraFOV * 0.5f) / InAspectRatio, -FVector::RightVector);
				const FVector BottomCameraFrustrumDir = TopCameraFrustrumDir * FVector(1, 1, -1);

				const static auto FrameCamera2D = [](const FVector2D& Point1, const FVector2D& Point2, 
					const FVector2D& LeftFrustrumEdgeDir, const FVector2D& RightFrustrumEdgeDir)->FVector2D
				{
					const FVector2D& LeftPoint = Point1.X < Point2.X ? Point1 : Point2;
					const FVector2D& RightPoint = Point1.X > Point2.X ? Point1 : Point2;

					// Intersection of 2D lines: https://stackoverflow.com/questions/4543506/algorithm-for-intersection-of-2-lines

					const float A1 = -LeftFrustrumEdgeDir.Y;
					const float B1 = LeftFrustrumEdgeDir.X;
					const float C1 = A1 * Point1.X + B1 * Point1.Y;

					const float A2 = -RightFrustrumEdgeDir.Y;
					const float B2 = RightFrustrumEdgeDir.X;
					const float C2 = A2 * Point2.X + B2 * Point2.Y;

					const float Determinant = A1 * B2 - A2 * B1;
					const FVector2D IntersectLocation = FMath::IsNearlyZero(Determinant) 
						? FVector2D::ZeroVector 
						: FVector2D((B2 * C1 - B1 * C2) / Determinant, (A1 * C2 - A2 * C1) / Determinant);

					// The points are too close together so the "optimal" location is behind the first point.
					// For now we return an "invalid location" since we are guaranteed to find a better location
					// later due to us checking a box (There will be a line parallel to this one which works and
					// will allow the camera to see both of these points).
					if (IntersectLocation.Y > FMath::Min(LeftPoint.Y, RightPoint.Y))
						return FVector2D(BIG_NUMBER);

					return IntersectLocation;
				};

				TPair<FVector, FVector> BestHorizontalPair = TPair<FVector, FVector>(FVector::ZeroVector, FVector::ZeroVector);
				TPair<FVector, FVector> BestVerticalPair   = TPair<FVector, FVector>(FVector::ZeroVector, FVector::ZeroVector);
				FVector2D BestHorizontalIntersectLocation  = FVector2D(BIG_NUMBER);
				FVector2D BestVerticalIntersectLocation    = FVector2D(BIG_NUMBER);
				for (int32 i = 0; i < 8; i++)
				{
					for (int32 j = i + 1; j < 8; j++)
					{
						const FVector& Point1 = InCameraSpaceBoundsVertices[i];
						const FVector& Point2 = InCameraSpaceBoundsVertices[j];

						// Horizontal Intersection
						{
							const FVector& LeftPoint = Point1.Y > Point2.Y ? Point2 : Point1;
							const FVector& RightPoint = Point1.Y > Point2.Y ? Point1 : Point2;

							const FVector2D HorizontalLocation = FrameCamera2D(FVector2D(LeftPoint.Y, LeftPoint.X), FVector2D(RightPoint.Y, RightPoint.X),
								FVector2D(LeftCameraFrustrumDir.Y, LeftCameraFrustrumDir.X), FVector2D(RightCameraFrustrumDir.Y, RightCameraFrustrumDir.X));

							if (HorizontalLocation.Y < BestHorizontalIntersectLocation.Y)
							{
								BestHorizontalIntersectLocation = HorizontalLocation;
								BestHorizontalPair = TPair<FVector, FVector>(Point1, Point2);
							}
						}

						// Vertical Intersection
						{
							const FVector& TopPoint = Point1.Z > Point2.Z ? Point2 : Point1;
							const FVector& BottomPoint = Point1.Z > Point2.Z ? Point1 : Point2;

							const FVector2D VerticalLocation = FrameCamera2D(FVector2D(TopPoint.Z, TopPoint.X), FVector2D(BottomPoint.Z, BottomPoint.X),
								FVector2D(BottomCameraFrustrumDir.Z, BottomCameraFrustrumDir.X), FVector2D(TopCameraFrustrumDir.Z, TopCameraFrustrumDir.X));
					
							if (VerticalLocation.Y < BestVerticalIntersectLocation.Y)
							{
								BestVerticalIntersectLocation = VerticalLocation;
								BestVerticalPair = TPair<FVector, FVector>(Point1, Point2);
							}
						}
					}
				}

				const FVector HorizontalCameraLocation = FVector(BestHorizontalIntersectLocation.Y, BestHorizontalIntersectLocation.X, 0.f);
				const FVector VerticalCameraLocation = FVector(BestVerticalIntersectLocation.Y, 0.f, BestVerticalIntersectLocation.X);
				const FVector NewCameraLocation = [&]() 
				{
					switch (InCameraSettings.CameraFitMode)
					{
					case EPortraitCameraFitMode::Fill:
						return FVector(FMath::Max(HorizontalCameraLocation.X, VerticalCameraLocation.X), HorizontalCameraLocation.Y, VerticalCameraLocation.Z);
					case EPortraitCameraFitMode::Fit:
						return FVector(FMath::Min(HorizontalCameraLocation.X, VerticalCameraLocation.X), HorizontalCameraLocation.Y, VerticalCameraLocation.Z);
					case EPortraitCameraFitMode::FitX:
						return FVector(HorizontalCameraLocation.X, HorizontalCameraLocation.Y, VerticalCameraLocation.Z);
					case EPortraitCameraFitMode::FitY:
						return FVector(VerticalCameraLocation.X, HorizontalCameraLocation.Y, VerticalCameraLocation.Z);
					}
					return FVector::ZeroVector;
				}();

				return NewCameraLocation;
			};

			FVector AutoLocation = CalculatePerspectiveViewLocation(AspectRatio, CameraSettings, BoundsVerticesInCameraSpace);
			AutoLocation.X = CameraSettings.bOverride_CameraDistanceOverride 
				? CameraSettings.CameraDistanceOverride
				: AutoLocation.X + CameraSettings.CameraDistanceOffset;

			NewViewInfo.Location = CameraRotation.RotateVector(AutoLocation);
			NewViewInfo.FOV      = CameraSettings.CameraFOV;
		}
		else
		{
			struct FOrthographicView
			{
				float OrthoWidth;
				FVector CameraLocation;
			};
			const static auto CalculateOrthographicView = [](float InAspectRatio, const FPortraitCameraSettings& InCameraSettings, const FBoundsVertices& InCameraSpaceBoundsVertices)->FOrthographicView
			{
				struct FBounds2D { FVector2D Min; FVector2D Max; };
				const auto ProjectedBounds2D = [&]()->FBounds2D
				{
					FVector2D OutMin(EForceInit::ForceInit);
					FVector2D OutMax(EForceInit::ForceInit);
					for (const auto &Vertex : InCameraSpaceBoundsVertices)
					{
						if (Vertex.Y < OutMin.X)
							OutMin.X = Vertex.Y;
						if (Vertex.Y > OutMax.X)
							OutMax.X = Vertex.Y;

						if (Vertex.Z < OutMin.Y)
							OutMin.Y = Vertex.Z;
						if (Vertex.Z > OutMax.Y)
							OutMax.Y = Vertex.Z;
					}

					return { OutMin, OutMax };
				}();

				const auto Bounds2DDimentions = FVector2D(FMath::Abs(ProjectedBounds2D.Max.X - ProjectedBounds2D.Min.X), FMath::Abs(ProjectedBounds2D.Max.Y - ProjectedBounds2D.Min.Y));

				const float OrthoWidth = [&]()->float
				{
					switch (InCameraSettings.CameraFitMode)
					{
					case EPortraitCameraFitMode::Fill:
						return FMath::Min(Bounds2DDimentions.X, Bounds2DDimentions.Y * InAspectRatio);
					case EPortraitCameraFitMode::Fit:
						return FMath::Max(Bounds2DDimentions.X, Bounds2DDimentions.Y * InAspectRatio);
					case EPortraitCameraFitMode::FitX:
						return Bounds2DDimentions.X;
					case EPortraitCameraFitMode::FitY:
						return Bounds2DDimentions.Y * InAspectRatio;
					}
					return 0.f;
				}();

				const FVector CameraLocation = FVector(-1000.f, // Make sure we're not clipping by moving it back an additional 1000cm
					(ProjectedBounds2D.Max.X + ProjectedBounds2D.Min.X) * 0.5f, 
					(ProjectedBounds2D.Max.Y + ProjectedBounds2D.Min.Y) * 0.5f
				);

				return { OrthoWidth, CameraLocation };
			};

			const FOrthographicView OrthographicView = CalculateOrthographicView(AspectRatio, CameraSettings, BoundsVerticesInCameraSpace);
			NewViewInfo.OrthoWidth = CameraSettings.bOverride_OrthoWidthOverride 
				? CameraSettings.OrthoWidthOverride
				: OrthographicView.OrthoWidth + CameraSettings.OrthoWidthOffset;
			NewViewInfo.Location   = CameraRotation.RotateVector(OrthographicView.CameraLocation);
		}

		NewViewInfo.Location += CameraRotation.RotateVector(CameraSettings.CameraPositionOffset);
		NewViewInfo.Rotation = CameraRotation.Rotator();
	}
	else
	{
		NewViewInfo.Location   = CameraSettings.CustomCameraLocation;
		NewViewInfo.Rotation   = CameraSettings.CustomCameraRotation;
		NewViewInfo.OrthoWidth = CameraSettings.CustomOrthoWidth;
	}

	if (CameraSettings.bOverride_CameraOrbitOriginOverride)
	{
		OrbitOrigin = ActorTransform.TransformPosition(CameraSettings.CameraOrbitOriginOverride);
	}

	if (CameraSettings.bOverride_CameraOrbitOriginOffset)
	{
		OrbitOrigin += ActorTransform.TransformVector(CameraSettings.CameraOrbitOriginOffset);
	}

	if (CameraSettings.bDrawDebug)
	{
		DrawDebugPoint(PortraitWorld, OrbitOrigin, 10.f, FColor::Red, true, -1.f, -1);
	}

	SetCameraTransform(FTransform(NewViewInfo.Rotation, NewViewInfo.Location));
	SetCameraProjection(NewViewInfo.ProjectionMode, NewViewInfo.FOV, NewViewInfo.OrthoWidth, false);

	PostCameraResetEvent.ExecuteIfBound();
}

void SActorPortrait::RotateActor(float RotateX, float RotateY)
{
	if (IsValid(PortraitActor))
	{
		const FTransform OrbitTransform(OrbitOrigin);
		const FTransform ViewTransform(ViewInfo.Rotation);
		const FTransform ActorTransform = PortraitActor->GetActorTransform();
		const FTransform ActorRelativeTransform = ActorTransform.GetRelativeTransform(OrbitTransform);

		const FQuat ViewQuat = ViewInfo.Rotation.Quaternion();
		const FQuat XRot = FQuat(ViewQuat.GetUpVector(), RotateX);
		const FQuat YRot = FQuat(ViewQuat.GetRightVector(), RotateY);

		PortraitActor->SetActorTransform(
			ActorRelativeTransform 
			* FTransform(XRot * YRot) 
			* OrbitTransform
		);
	}
}

const FVector& SActorPortrait::GetCameraOrbitOrigin() const
{
	return OrbitOrigin;
}

void SActorPortrait::SetCameraOrbitOrigin(const FVector& NewOrbitOrigin)
{
	OrbitOrigin = NewOrbitOrigin;
}

void SActorPortrait::OrbitCamera(float OrbitX, float OrbitY)
{
	const FTransform OrbitTransform(OrbitOrigin);
	const FTransform ViewTransform(ViewInfo.Rotation, ViewInfo.Location);
	const FTransform RelativeViewTransform = ViewTransform.GetRelativeTransform(OrbitTransform);

	const FQuat ViewQuat = ViewInfo.Rotation.Quaternion();
	const FQuat XRot = FQuat(ViewQuat.GetUpVector(), OrbitX);
	const FQuat YRot = FQuat(ViewQuat.GetRightVector(), OrbitY);

	SetCameraTransform(
		RelativeViewTransform 
		* FTransform(XRot * YRot) 
		* OrbitTransform
	);
}

void SActorPortrait::ZoomCamera(float ZoomDelta)
{
	const auto CameraSettings = PortraitCameraSettings.Get();
	if (CameraSettings.ProjectionType == ECameraProjectionMode::Perspective)
	{
		const FTransform WorldToOrbitSpace = FTransform(ViewInfo.Rotation.GetInverse(), OrbitOrigin);
		FVector RelativeCameraLocation = WorldToOrbitSpace.InverseTransformPosition(ViewInfo.Location);
		
		FVector ZoomVector = RelativeCameraLocation.GetSafeNormal() * ZoomDelta;
		if (FVector::DotProduct(RelativeCameraLocation, ZoomVector) < 0.f)
		{
			ZoomVector = ZoomVector.GetClampedToMaxSize(RelativeCameraLocation.Size() - 0.01f); // Make sure we cannot zoom past the origin
		}

		RelativeCameraLocation = (RelativeCameraLocation + ZoomVector).GetClampedToSize(FMath::Max(0.001f, CameraSettings.MinZoomDistance), FMath::Max(0.001f, CameraSettings.MaxZoomDistance));
		RelativeCameraLocation = WorldToOrbitSpace.TransformPosition(RelativeCameraLocation);

		SetCameraTransform(FTransform(ViewInfo.Rotation, RelativeCameraLocation));
	}
	else
	{
		SetCameraProjection(ViewInfo.ProjectionMode, ViewInfo.DesiredFOV, FMath::Clamp(ViewInfo.OrthoWidth + ZoomDelta, CameraSettings.MinZoomDistance, CameraSettings.MaxZoomDistance), false);
	}
}

void SActorPortrait::SetCameraZoom(float NewZoom)
{
	const auto CameraSettings = PortraitCameraSettings.Get();
	if (CameraSettings.ProjectionType == ECameraProjectionMode::Perspective)
	{
		const FTransform WorldToOrbitSpace = FTransform(ViewInfo.Rotation.GetInverse(), OrbitOrigin);
		FVector RelativeCameraLocation = WorldToOrbitSpace.TransformPosition(ViewInfo.Location);
		RelativeCameraLocation.X = FMath::Clamp(-NewZoom, -CameraSettings.MaxZoomDistance, -CameraSettings.MinZoomDistance);
		RelativeCameraLocation = WorldToOrbitSpace.InverseTransformPosition(RelativeCameraLocation);

		SetCameraTransform(
			FTransform(FVector(-NewZoom, 0, 0))
			* FTransform(ViewInfo.Rotation)
			* FTransform(OrbitOrigin)
		);
	}
	else
	{
		SetCameraProjection(ViewInfo.ProjectionMode, ViewInfo.DesiredFOV, FMath::Clamp(NewZoom, CameraSettings.MinZoomDistance, CameraSettings.MaxZoomDistance), false);
	}
}

void SActorPortrait::SetCameraTransform(const FTransform& Transform)
{
	ViewInfo.Location = Transform.GetLocation();
	ViewInfo.Rotation = Transform.Rotator();

	MarkRenderStateDirty();
}

void SActorPortrait::SetCameraProjection(ECameraProjectionMode::Type ProjectionMode, float FOV, float OrthoWidth, bool bResetCamera)
{
	ViewInfo.ProjectionMode = ProjectionMode;
	ViewInfo.FOV            = FOV;
	ViewInfo.OrthoWidth     = OrthoWidth;

	MarkRenderStateDirty();

	if (bResetCamera)
	{
		ResetCamera();
	}
}

void SActorPortrait::SetPortraitActorTransform(const FTransform& Transform, bool bResetCamera)
{
	if (IsValid(PortraitActor))
	{
		PortraitActor->SetActorTransform(Transform);
	}

	if (bResetCamera)
	{
		ResetCamera();
	}
}

void SActorPortrait::ApplyDirectionalLightTemplate(UDirectionalLightComponent* DirectionalLightTemplate)
{
	PortraitScene->ApplyDirectionalLightTemplate(DirectionalLightTemplate);
	MarkRenderStateDirty();
}

void SActorPortrait::ApplySkyLightTemplate(USkyLightComponent* SkyLightTemplate)
{
	PortraitScene->ApplySkyLightTemplate(SkyLightTemplate);
	MarkRenderStateDirty();
}

void SActorPortrait::RecaptureSky()
{
	if (IsValid(SkySphereActor) && SkySphereActor->Implements<UActorPortraitInterface>())
	{
		IActorPortraitInterface::Execute_OnUpdatePortraitScene(SkySphereActor, PortraitUserData);
	}

	PortraitScene->UpdateSkyCaptureContents();

	MarkRenderStateDirty();
}

void SActorPortrait::RecreatePortraitScene(const TSoftObjectPtr<UWorld>& WorldAsset, TSubclassOf<AActor> ActorClass, const FTransform& ActorTransform, TSubclassOf<AActor> SkySphereClass, UDirectionalLightComponent* DirectionalLightTemplate, USkyLightComponent* SkyLightTemplate, UGameInstance* OwningGameInstance)
{
	if (PortraitScene.IsValid())
	{
		if (UWorld* PortraitWorld = PortraitScene->GetWorld())
		{
			PortraitWorlds.Remove(PortraitWorld);
		}
	}

	PortraitScene = MakePimpl<FActorPortraitScene>(WorldAsset, DirectionalLightTemplate, SkyLightTemplate, bTickWorld.Get(), OwningGameInstance);

	if (UWorld* PortraitWorld = PortraitScene->GetWorld())
	{
		PortraitWorlds.Add(PortraitWorld, SharedThis(this));
	}

	RecreatePortraitActor(ActorClass, ActorTransform, false);
	RecreateSkySphere(SkySphereClass, true);
	ResetCamera(); // Do this manually so we don't get a one frame delay on resetting the camera.
}

void SActorPortrait::RecreatePortraitActor(TSubclassOf<AActor> ActorClass, const FTransform& ActorTransform, bool bResetCamera)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SActorPortrait_RecreatePortraitActor);

	if (IsValid(PortraitActor))
	{
		PortraitActor->Destroy();
		PortraitActor = nullptr;
	}

	if (UClass* PortraitActorClass = ActorClass.Get())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bNoFail = true;
		SpawnParams.bDeferConstruction = true;
		PortraitActor = PortraitScene->SpawnPortraitActor(PortraitActorClass, ActorTransform, SpawnParams);

		PreSpawnPortraitActorEvent.ExecuteIfBound(PortraitActor);

		PortraitScene->FinishSpawningPortraitActor(PortraitActor, ActorTransform);
		
		OnSpawnPortraitActorEvent.ExecuteIfBound(PortraitActor);
	}

	MarkRenderStateDirty();

	if (bResetCamera)
	{
		ResetCamera();
	}
}

void SActorPortrait::RecreateSkySphere(TSubclassOf<AActor> InSkySphereClass, bool bRecaptureSky)
{
	UClass* SkySphereClass = InSkySphereClass.Get();
	if (IsValid(SkySphereActor) && (!SkySphereClass || SkySphereActor->GetClass() != SkySphereClass))
	{
		SkySphereActor->Destroy();
		SkySphereActor = nullptr;
	}

	if (!IsValid(SkySphereActor) && SkySphereClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bNoFail = true;
		SkySphereActor = PortraitScene->SpawnPortraitActor<AActor>(SkySphereClass, SpawnParams);
	}

	if (bRecaptureSky)
	{
		RecaptureSky();
	}

	MarkRenderStateDirty();
}

UWorld* SActorPortrait::GetPortraitWorld() const
{
	return PortraitScene->GetWorld(); 
}

AActor* SActorPortrait::GetSkySphereActor() const
{
	return SkySphereActor;
}

AActor* SActorPortrait::GetPortraitActor() const
{
	return PortraitActor;
}

USceneCaptureComponent2D* SActorPortrait::GetCaptureComponent() const
{
	return PortraitScene->GetCaptureComponent();
}

UDirectionalLightComponent* SActorPortrait::GetDirectionalLightComponent() const
{
	return PortraitScene->GetDirectionalLightComponent();
}

USkyLightComponent* SActorPortrait::GetSkyLightComponent() const
{
	return PortraitScene->GetSkyLightComponent();
}

FIntPoint SActorPortrait::GetSizeXY() const
{
	return CachedGeometry.GetLocalSize().IntPoint();
}

FIntPoint SActorPortrait::GetRenderSizeXY() const
{
	const TOptional<FIntPoint> RenderSizeOverride = RenderResolutionOverride.Get();
	return (RenderSizeOverride.IsSet() ? RenderSizeOverride.GetValue() : CachedGeometry.GetLocalSize().IntPoint()) * ResolutionScale.Get();
}

bool SActorPortrait::IsPortraitWorld(UWorld* World)
{
	return PortraitWorlds.Contains(World);
}

TWeakPtr<SActorPortrait> SActorPortrait::FindPortraitWidgetFromWorld(UWorld* World)
{
	return PortraitWorlds.FindPortraitWidgetFromWorld(World);
}

void SActorPortrait::RecreateRenderMaterial()
{
	if (!IsValid(GetCaptureComponent()) || !IsValid(GetCaptureComponent()->TextureTarget))
		return;

	UMaterialInstanceDynamic* RenderMaterialInstance = Cast<UMaterialInstanceDynamic>(Brush.GetResourceObject());
	if (RenderMaterialInstance)
	{
		Brush.SetResourceObject(nullptr);
		RenderMaterialInstance->MarkAsGarbage();
	}

	UMaterialInterface* RenderMat = RenderMaterial;
	const FName TextureParamName = RenderMaterialTextureParameter;

	RenderMaterialInstance = (RenderMat && TextureParamName != NAME_None) ? UMaterialInstanceDynamic::Create(RenderMat, GetTransientPackage(), NAME_None) : nullptr;

	if (RenderMaterialInstance)
	{
		RenderMaterialInstance->SetTextureParameterValue(TextureParamName, GetCaptureComponent()->TextureTarget);
		Brush.SetResourceObject(RenderMaterialInstance);
	}
	else
	{
		Brush.SetResourceObject(GetCaptureComponent()->TextureTarget);
	}
}

void SActorPortrait::ResizeRenderTarget(const FIntPoint& NewRenderSize)
{
	USceneCaptureComponent2D* CaptureComponent = GetCaptureComponent();
	if (!IsValid(CaptureComponent))
	{
		return;
	}

	if (!CaptureComponent->TextureTarget)
	{
		if (NewRenderSize.X > 0 && NewRenderSize.Y > 0)
		{
			UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(CaptureComponent, NAME_None, RF_Transient);
			NewRenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
			NewRenderTarget2D->ClearColor = FLinearColor::Black;
			NewRenderTarget2D->InitAutoFormat(NewRenderSize.X, NewRenderSize.Y);
			NewRenderTarget2D->UpdateResourceImmediate(true);

			CaptureComponent->TextureTarget = NewRenderTarget2D;
		}
		RecreateRenderMaterial();
	}
	else if (NewRenderSize.X > 0 && NewRenderSize.Y > 0)
	{
		CaptureComponent->TextureTarget->ResizeTarget(NewRenderSize.X, NewRenderSize.Y);
	}
}

void SActorPortrait::ProcessAccumulatedPointerInput()
{
	const bool bHasMouseCapture = HasMouseCapture();

	if (NumMouseSamplesX > 0 || NumMouseSamplesY > 0)
	{
		const float DeltaTime = FApp::GetDeltaTime();
		if (OnInputAxisEvent.IsBound())
		{
			FModifierKeysState ModifierKeysState = FSlateApplication::Get().GetModifierKeys();

			{
				const uint32* CharacterCode;
				const uint32* KeyCode;
				FInputKeyManager::Get().GetCodesFromKey(EKeys::MouseX, CharacterCode, KeyCode);
				OnInputAxisEvent.Execute(CachedGeometry, FAnalogInputEvent(EKeys::MouseX, ModifierKeysState, MouseDeltaUserIndex, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0, MouseDelta.X));
			}

			{
				const uint32* CharacterCode;
				const uint32* KeyCode;
				FInputKeyManager::Get().GetCodesFromKey(EKeys::MouseY, CharacterCode, KeyCode);
				OnInputAxisEvent.Execute(CachedGeometry, FAnalogInputEvent(EKeys::MouseY, ModifierKeysState, MouseDeltaUserIndex, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0, MouseDelta.Y));
			}
		}
	}

	if (bCursorHiddenDueToCapture)
	{
		switch (MouseCaptureMode.Get())
		{
		case EMouseCaptureMode::NoCapture:
		case EMouseCaptureMode::CaptureDuringMouseDown:
		case EMouseCaptureMode::CaptureDuringRightMouseDown:
			if (!bHasMouseCapture)
			{
				bool bShouldMouseBeVisible = GetCursor().Get(EMouseCursor::Default) != EMouseCursor::None;

				bShouldMouseBeVisible = bShouldMouseBeVisible && bShouldShowMouseCursor.Get();

				if (bShouldMouseBeVisible)
				{
					bCursorHiddenDueToCapture = false;
					CurrentReplyState.SetMousePos(MousePosBeforeHiddenDueToCapture);
					MousePosBeforeHiddenDueToCapture = FIntPoint(-1, -1);
				}
			}
			break;
		}
	}

	MouseDelta = FIntPoint::ZeroValue;
	NumMouseSamplesX = 0;
	NumMouseSamplesY = 0;
	MouseDeltaUserIndex = INDEX_NONE;
}

void SActorPortrait::UpdateCachedCursorPos(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D LocalPixelMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	LocalPixelMousePos.X *= CachedGeometry.Scale;
	LocalPixelMousePos.Y *= CachedGeometry.Scale;

	CachedCursorPos = LocalPixelMousePos.IntPoint();
}

void SActorPortrait::UpdateCachedGeometry(const FGeometry& InGeometry)
{
	CachedGeometry = InGeometry;
	if (!bHasValidCachedGeometry)
	{
		MarkCameraNeedsReset();
		MarkRenderStateDirty();
		bHasValidCachedGeometry = true;
	}
}

void SActorPortrait::ApplyModifierKeys(const FModifierKeysState& InKeysState)
{
	if (OnInputKeyEvent.IsBound())
	{
		if (InKeysState.IsLeftAltDown())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(EKeys::LeftAlt, CharacterCode, KeyCode);
			OnInputKeyEvent.Execute(CachedGeometry, FKeyEvent(EKeys::LeftAlt, InKeysState, 0, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0), IE_Pressed);
		}
		if (InKeysState.IsRightAltDown())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(EKeys::RightAlt, CharacterCode, KeyCode);
			OnInputKeyEvent.Execute(CachedGeometry, FKeyEvent(EKeys::RightAlt, InKeysState, 0, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0), IE_Pressed);

		}
		if (InKeysState.IsLeftControlDown())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(EKeys::LeftControl, CharacterCode, KeyCode);
			OnInputKeyEvent.Execute(CachedGeometry, FKeyEvent(EKeys::LeftControl, InKeysState, 0, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0), IE_Pressed);
		}
		if (InKeysState.IsRightControlDown())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(EKeys::RightControl, CharacterCode, KeyCode);
			OnInputKeyEvent.Execute(CachedGeometry, FKeyEvent(EKeys::RightControl, InKeysState, 0, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0), IE_Pressed);
		}
		if (InKeysState.IsLeftShiftDown())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(EKeys::LeftShift, CharacterCode, KeyCode);
			OnInputKeyEvent.Execute(CachedGeometry, FKeyEvent(EKeys::LeftShift, InKeysState, 0, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0), IE_Pressed);
		}
		if (InKeysState.IsRightShiftDown())
		{
			const uint32* CharacterCode;
			const uint32* KeyCode;
			FInputKeyManager::Get().GetCodesFromKey(EKeys::RightShift, CharacterCode, KeyCode);
			OnInputKeyEvent.Execute(CachedGeometry, FKeyEvent(EKeys::RightShift, InKeysState, 0, false, CharacterCode ? *CharacterCode : 0, KeyCode ? *KeyCode : 0), IE_Pressed);
		}
	}
}

FReply SActorPortrait::AcquireFocusAndCapture(FIntPoint MousePosition, EFocusCause FocusCause)
{
	bShouldCaptureMouseOnActivate = false;

	FReply ReplyState = FReply::Handled().PreventThrottling();

	TSharedRef<SActorPortrait> WidgetRef = SharedThis(this);

	// Mouse down should focus portrait for user input
	ReplyState.SetUserFocus(WidgetRef, FocusCause, true);

	ReplyState.CaptureMouse(WidgetRef);

	if (bLockDuringCapture.Get())
	{
		ReplyState.LockMouseToWidget(WidgetRef);
	}

	const bool bShouldMouseBeVisible = bShouldShowMouseCursor.Get();

	if (bLockDuringCapture.Get() && bShouldMouseBeVisible)
	{
		bCursorHiddenDueToCapture = true;
		MousePosBeforeHiddenDueToCapture = MousePosition;
	}

	if (bCursorHiddenDueToCapture || !bShouldMouseBeVisible)
	{
		ReplyState.UseHighPrecisionMouseMovement(WidgetRef);
	}

	return ReplyState;
}

#undef LOCTEXT_NAMESPACE
