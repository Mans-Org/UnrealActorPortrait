// Mans Isaksson 2020

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "ActorPortraitSettings.h"
#include "ActorPortraitCompatibilityLayer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/PimplPtr.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USceneCaptureComponent2D;
class UGameInstance;

class ACTORPORTRAIT_API SActorPortrait : public SCompoundWidget, public FGCObject
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FMotionEventHandler, const FGeometry& /*Geometry*/, const FMotionEvent& /*MotionEvent*/);
	DECLARE_DELEGATE_RetVal_ThreeParams(FReply, FKeyEventHandler, const FGeometry& /*Geometry*/, const FKeyEvent& /*KeyEvent*/, EInputEvent /*InputEvent*/);
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FAnalogEventHandler, const FGeometry& /*Geometry*/, const FAnalogInputEvent& /*AnalogInputEvent*/);
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FFocusEventHandler, const FGeometry& /*Geometry*/, const FFocusEvent& /*FocusEvent*/);
	DECLARE_DELEGATE_TwoParams(FNoReplyFocusEventHandler, const FGeometry& /*Geometry*/, const FFocusEvent& /*FocusEvent*/);
	DECLARE_DELEGATE_OneParam(FOnSpawnPortraitActor, AActor* /*Actor*/);
	DECLARE_DELEGATE(FPostCameraReset);

private:

	// Slate arguments
	TObjectPtr<UObject> PortraitUserData = nullptr;
	TObjectPtr<UMaterialInterface> RenderMaterial = nullptr;
	FName RenderMaterialTextureParameter = NAME_None;

	// Slate attributes
	TAttribute<FPortraitCameraSettings> PortraitCameraSettings;
	TAttribute<FPostProcessSettings> PostProcessingSettings;
	TAttribute<FSlateColor> ColorAndOpacity;
	TAttribute<FVector2D> PortraitSize;
	TAttribute<TOptional<FIntPoint>> RenderResolutionOverride;
	TAttribute<float> ResolutionScale;
	TAttribute<EMouseCaptureMode> MouseCaptureMode;
	TAttribute<bool> bLockDuringCapture;
	TAttribute<bool> bIgnoreInput;
	TAttribute<bool> bTickWorld;
	TAttribute<bool> bRealTime;
	TAttribute<bool> bShouldShowMouseCursor;
	TAttribute<ESceneCaptureSource> CaptureSource;

	TPimplPtr<class FActorPortraitScene> PortraitScene = nullptr;
	TObjectPtr<AActor> SkySphereActor = nullptr;
	TObjectPtr<AActor> PortraitActor = nullptr;

	/* Brush used to draw the capture component render target */
	FSlateBrush Brush;

	/* Stored view information */
	FMinimalViewInfo ViewInfo;
	FVector OrbitOrigin = FVector::ZeroVector;
	
	/* Current size used to render the portrait */
	FIntPoint RenderSize = FIntPoint(0, 0);

	/* True if scene needs recapture (ignored if real-time) */
	bool bRenderStateDirty = false;

	/* True if ResetCamera needs to be called next frame */
	bool bCameraNeedsReset = false;

	/* Keep track of the last rendered frame */
	uint32 LastFrameNumber = 0;

	/* Input Events */
	FPointerEventHandler			OnInputTouchEvent;
	FPointerEventHandler			OnTouchGestureEvent;
	FMotionEventHandler				OnInputMotionEvent;
	FKeyEventHandler				OnInputKeyEvent;
	FAnalogEventHandler				OnInputAxisEvent;
	FFocusEventHandler				OnReceiveFocusEvent;
	FNoReplyFocusEventHandler		OnLostFocusEvent;
	FNoReplyPointerEventHandler		OnCapturedMouseMoveEvent;
	FNoReplyPointerEventHandler		OnMouseMoveEvent;
	FNoReplyPointerEventHandler		OnMouseEnterEvent;
	FNoReplyPointerEventHandler		OnMouseLeaveEvent;
	FOnSpawnPortraitActor           OnSpawnPortraitActorEvent;
	FOnSpawnPortraitActor           PreSpawnPortraitActorEvent;
	FPostCameraReset                PostCameraResetEvent;

	/** An intermediate reply state that is reset whenever an input event is generated */
	FReply CurrentReplyState = FReply::Unhandled();
	/** The last known mouse position in local space, -1, -1 if unknown */
	FIntPoint CachedCursorPos = FIntPoint(-1, -1);
	/** The last known geometry info */
	FGeometry CachedGeometry;
	/** Has the CachedGeometry been set at least once? */
	bool bHasValidCachedGeometry = false;
	/** The number of input samples in X since input was was last processed */
	int32 NumMouseSamplesX = 0;
	/** The number of input samples in Y since input was was last processed */
	int32 NumMouseSamplesY = 0;
	/** User index supplied by mouse events accumulated into NumMouseSamplesX and NumMouseSamplesY */
	int32 MouseDeltaUserIndex = 0;
	/** The current mouse delta */
	FIntPoint MouseDelta = FIntPoint(0, 0);
	/** Whether or not the cursor is hidden when the viewport captures the mouse */
	bool bCursorHiddenDueToCapture = false;
	/** Position the cursor was at when we hid it due to capture, so we can put it back afterwards */
	FIntPoint MousePosBeforeHiddenDueToCapture = FIntPoint(0, 0);
	/** Tracks the number of touches currently active on the viewport */
	int32 NumTouches = 0;
	/** true if we had Capture when deactivated */
	bool bShouldCaptureMouseOnActivate = false;

public:

	SLATE_BEGIN_ARGS(SActorPortrait)
		: _WorldAsset(nullptr)
		, _PortraitActorClass(nullptr)
		, _PortraitActorTransform(FTransform::Identity)
		, _DirectionalLightTemplate(nullptr)
		, _SkyLightTemplate(nullptr)
		, _SkySphereClass(nullptr)
		, _OwningGameInstance(nullptr)
		, _PortraitUserData(nullptr)
		, _RenderMaterial(nullptr)
		, _RenderMaterialTextureParameter(NAME_None)
		, _ColorAndOpacity(FLinearColor::White)
		, _PortraitCameraSettings(FPortraitCameraSettings())
		, _PostProcessingSettings(FPostProcessSettings())
		, _PortraitSize(FVector2D(320.0f, 240.0f))
		, _RenderResolutionOverride(TOptional<FIntPoint>())
		, _ResolutionScale(1.f)
		, _MouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown)
		, _bLockDuringCapture(true)
		, _bIgnoreInput(false)
		, _bTickWorld(true)
		, _bRealTime(true)
		, _bShouldShowMouseCursor(true)
		, _CaptureSource(ESceneCaptureSource::SCS_FinalColorHDR)
	{
	}

		SLATE_DEFAULT_SLOT(FArguments, Content)

		/** World Asset Path */
		SLATE_ARGUMENT(TSoftObjectPtr<UWorld>, WorldAsset)

		/** Portrait Actor Class */
		SLATE_ARGUMENT(TSubclassOf<AActor>, PortraitActorClass)
		
		/** Portrait Actor Class */
		SLATE_ARGUMENT(FTransform, PortraitActorTransform)

		/** Portrait world directional light template object */
		SLATE_ARGUMENT(UDirectionalLightComponent*, DirectionalLightTemplate)

		/** Portrait world sky light template object */
		SLATE_ARGUMENT(USkyLightComponent*, SkyLightTemplate)

		/** Portrait sky sphere class */
		SLATE_ARGUMENT(TSubclassOf<AActor>, SkySphereClass)

		/** The owning game instance of this portrait world (nullptr if none) */
		SLATE_ARGUMENT(UGameInstance*, OwningGameInstance)

		/** Arbitrary user data object, passed to IActorPortraitInterface when updating sky sphere */
		SLATE_ARGUMENT(UObject*, PortraitUserData)

		/** Optional material to use to render the portrait */
		SLATE_ARGUMENT(UMaterialInterface*, RenderMaterial)

		/** If using RenderMaterial, this texture parameter will be used to draw the portrait texture */
		SLATE_ARGUMENT(FName, RenderMaterialTextureParameter)

		/** Color and opacity */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)

		/** Portrait Camera settings */
		SLATE_ATTRIBUTE(FPortraitCameraSettings, PortraitCameraSettings)

		/** Portrait post processing settings */
		SLATE_ATTRIBUTE(FPostProcessSettings, PostProcessingSettings)

		/** Desired portrait size */
		SLATE_ATTRIBUTE(FVector2D, PortraitSize)

		/** Optional resulution override for the portrait render target. Will override PortraitSize if set */
		SLATE_ATTRIBUTE(TOptional<FIntPoint>, RenderResolutionOverride)

		/** Resolution scale, applied both with ResolutionOverride and when rendering the portrait normally */
		SLATE_ATTRIBUTE(float, ResolutionScale)

		/** Portrait Mouse capture mode */
		SLATE_ATTRIBUTE(EMouseCaptureMode, MouseCaptureMode)

		/** Lock mouse to portrait during mouse capture */
		SLATE_ATTRIBUTE(bool, bLockDuringCapture)

		/** Ignore any input send to the portrait */
		SLATE_ATTRIBUTE(bool, bIgnoreInput)

		/** Whether to tick the portrait world */
		SLATE_ATTRIBUTE(bool, bTickWorld)

		/** Whether to update the portrait in real-time (useful if you want to tick animations or particle effects) */
		SLATE_ATTRIBUTE(bool, bRealTime)

		/** Whether the mouse cursor should be shown by default when focusing this widget */
		SLATE_ATTRIBUTE(bool, bShouldShowMouseCursor)

		/** Which scene to use as source for the portrait capture */
		SLATE_ATTRIBUTE(ESceneCaptureSource, CaptureSource)


		/** Invoked when touch event occurs on the portrait */
		SLATE_EVENT(FPointerEventHandler, OnInputTouchEvent)

		/** Invoked when gesture event occurs on the portrait */
		SLATE_EVENT(FPointerEventHandler, OnTouchGestureEvent)

		/** Invoked when motion event occurs on the portrait */
		SLATE_EVENT(FMotionEventHandler, OnInputMotionEvent)
		
		/** Invoked when key event occurs while portrait is in focus */
		SLATE_EVENT(FKeyEventHandler, OnInputKeyEvent)
		
		/** Invoked when axis event occurs while portrait is in focus */
		SLATE_EVENT(FAnalogEventHandler, OnInputAxisEvent)
		
		/** Invoked when portrait gains focus */
		SLATE_EVENT(FFocusEventHandler, OnReceiveFocusEvent)
		
		/** Invoked when portrait lose focus */
		SLATE_EVENT(FNoReplyFocusEventHandler, OnLostFocusEvent)
		
		/** Invoked when mouse movement occurs on the portrait, while mouse is in captured state */
		SLATE_EVENT(FNoReplyPointerEventHandler, OnCapturedMouseMoveEvent)
		
		/** Invoked when mouse movement occurs on the portrait */
		SLATE_EVENT(FNoReplyPointerEventHandler, OnMouseMoveEvent)
		
		/** Invoked when mouse enters the portrait */
		SLATE_EVENT(FNoReplyPointerEventHandler, OnMouseEnterEvent)
		
		/** Invoked when mouse leaves the portrait */
		SLATE_EVENT(FNoReplyPointerEventHandler, OnMouseLeaveEvent)

		/** Invoked after the portrait actor is spawned */
		SLATE_EVENT(FOnSpawnPortraitActor, OnSpawnPortraitActorEvent)

		/** Invoked right before the portrait actor is spawned. Useful for running pre begin play setup of the actor */
		SLATE_EVENT(FOnSpawnPortraitActor, PreSpawnPortraitActorEvent)

		/* 
		 * Invoked when the camera is reset. Can be useful if you want to tweak the auto-generated location of the camera.
		 * It can also be used to initialize custom camera settings as the camera reset has to be delayed until the widget is first rendered.
		 * This is due to the camera framing needing to know the size of the widget before it can frame the actor.
		 */
		SLATE_EVENT(FPostCameraReset, PostCameraResetEvent)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SActorPortrait();

public:
	// ~Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double CurrentTime, const float DeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent) override;
	virtual FReply OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent) override;
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& CharacterEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual void OnFinishedPointerInput() override;
	// ~End SWidget interface

	// ~Begin GCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// ~End GCObject interface

	void SetContent(TSharedPtr<SWidget> InContent);

	const TSharedPtr<SWidget> GetContent() const;

	void SetPortraitUserData(UObject* InPortraitUserData, bool bRecaptureSky = true);

	void SetRenderMaterial(UMaterialInterface* InRenderMaterial, FName InRenderMaterialTextureParameter);

	void SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity);

	void SetPortraitCameraSettings(const TAttribute<FPortraitCameraSettings>& InPortraitCameraSettings, bool bResetCamera);

	void SetPostProcessSettings(const TAttribute<FPostProcessSettings>& NewPostProcessSettings);

	void SetMouseCaptureMode(const TAttribute<EMouseCaptureMode>& InMouseCaptureMode);

	void SetLockDuringCapture(const TAttribute<bool>& InLockDuringCapture);

	void SetIgnoreInput(const TAttribute<bool>& InIgnoreInput);

	void SetRealTime(const TAttribute<bool>& InRealTime);

	void SetTickWorld(const TAttribute<bool>& InTickWorld);

	void SetCaptureSource(const TAttribute<ESceneCaptureSource>& InCaptrueSource);

	void SetPortraitSize(const TAttribute<FVector2D>& InPortraitSize);

	void SetRenderResolutionOverride(const TAttribute<TOptional<FIntPoint>>& InRenderResolutionOverride);

	void SetResolutionScale(const TAttribute<float>& InResolutionScale);

	/** Reset the camera by recalculating the camera auto-framing */
	void ResetCamera();

	/** Rotate the actor given an X/Y input */
	void RotateActor(float RotateX, float RotateY);

	/** Get current orbit point for the camera */
	const FVector& GetCameraOrbitOrigin() const;

	/** Sets the current orbit point for the camera */
	void SetCameraOrbitOrigin(const FVector& NewOrbitOrigin);

	/** Orbit the camera around the orbit origin */
	void OrbitCamera(float OrbitX, float OrbitY);

	/** Add/subtract camera zoom (In perspective zooming moves the camera along its forward axis, in orthographic it instead changes the OrthoWidth) */
	void ZoomCamera(float ZoomDelta);

	/** Set the current camera zoom (In perspective this sets the camera position along its forward axis, in orthographic it instead sets the OrthoWidth) */
	void SetCameraZoom(float NewZoom);

	/** Manually set the camera transform */
	void SetCameraTransform(const FTransform& Transform);

	/** Set the camera projection, optionally resets the camera auto-framing based on the new camera projection */
	void SetCameraProjection(ECameraProjectionMode::Type ProjectionMode, float FOV, float OrthoWidth, bool bResetCamera);

	/** Sets the transform of the portrait actor, optionally resets the camera auto-framing based on the new actor transform */
	void SetPortraitActorTransform(const FTransform& Transform, bool bResetCamera);

	/** Applies the settings if the DirectionalLightTemplate onto the directional light in the portrait scene */
	void ApplyDirectionalLightTemplate(UDirectionalLightComponent* DirectionalLightTemplate);
	
	/** Applies the settings if the SkyLightTemplate onto the sky light in the portrait scene */
	void ApplySkyLightTemplate(USkyLightComponent* SkyLightTemplate);

	/** Force recaptures the scene sky light and cube-maps */
	void RecaptureSky();

	/** Will recreate the underlying portrait scene and all its actors (including the portrait actor) */
	void RecreatePortraitScene(const TSoftObjectPtr<UWorld>& WorldAsset, TSubclassOf<AActor> ActorClass, const FTransform& ActorTransform, TSubclassOf<AActor> SkySphereClass, UDirectionalLightComponent* DirectionalLightTemplate, USkyLightComponent* SkyLightTemplate, UGameInstance* OwningGameInstance);

	/** Will recreate the portrait actor with the new actor class */
	void RecreatePortraitActor(TSubclassOf<AActor> ActorClass, const FTransform& ActorTransform, bool bResetCamera);

	/** Will recreate the sky sphere */
	void RecreateSkySphere(TSubclassOf<AActor> InSkySphereClass, bool bRecaptureSky);

	/** Force updates the scene capture next draw */
	FORCEINLINE void MarkRenderStateDirty() { bRenderStateDirty = true; }
	
	/** Force resets the camera on the next widget update */
	FORCEINLINE void MarkCameraNeedsReset() { bCameraNeedsReset = true; }

	/** Returns the underlying world used to draw the portrait actor */
	UWorld* GetPortraitWorld() const;

	/** Returns sky sphere actor from the portrait world, nullptr if there is none */
	AActor* GetSkySphereActor() const;

	/** Returns portrait actor from the portrait world */
	AActor* GetPortraitActor() const;

	/** Returns the capture component used to render the portrait world */
	USceneCaptureComponent2D* GetCaptureComponent() const;

	/** Returns the default directional light component spawned by the portrait scene */
	UDirectionalLightComponent* GetDirectionalLightComponent() const;

	/** Returns the default sky light component spawned by the portrait scene */
	USkyLightComponent* GetSkyLightComponent() const;

	/** Returns the XY dimentions of the portrait */
	FIntPoint GetSizeXY() const;

	/** Returns the XY dimentions of the underlying render target */
	FIntPoint GetRenderSizeXY() const;

	/** Returns true if the world is part of an actor portrait widget */
	static bool IsPortraitWorld(UWorld* World);

	/** Returns the actor portrait widget which owns the supplied world */
	static TWeakPtr<SActorPortrait> FindPortraitWidgetFromWorld(UWorld* World);

private:

	void RecreateRenderMaterial();

	void ResizeRenderTarget(const FIntPoint& NewRenderSize);

	void ProcessAccumulatedPointerInput();

	void UpdateCachedCursorPos(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);

	void UpdateCachedGeometry(const FGeometry& InGeometry);

	void ApplyModifierKeys(const FModifierKeysState& InKeysState);

	/** Utility function to create an FReply that properly gets Focus and capture based on the settings*/
	FReply AcquireFocusAndCapture(FIntPoint MousePosition, EFocusCause FocusCause = EFocusCause::SetDirectly);

	template<typename TargetValueType, typename SourceValueType, typename Ret, typename... ParamTypes>
	bool SetAttributeWithSideEffect(TAttribute<TargetValueType>& TargetValue, const TAttribute<SourceValueType>& SourceValue, Ret (SActorPortrait::*SideEffectCallable)(ParamTypes...), ParamTypes... Params)
	{
		if (!TargetValue.IdenticalTo(SourceValue))
		{
			TargetValue = SourceValue;
			(this->*SideEffectCallable)(Params...);
			return true;
		}

		return false;
	}

	template<typename TargetValueType, typename SourceValueType, typename Callable>
	bool SetAttributeWithSideEffect(TAttribute<TargetValueType>& TargetValue, const TAttribute<SourceValueType>& SourceValue, Callable SideEffectCallable)
	{
		if (!TargetValue.IdenticalTo(SourceValue))
		{
			TargetValue = SourceValue;
			SideEffectCallable();
			return true;
		}

		return false;
	}
};