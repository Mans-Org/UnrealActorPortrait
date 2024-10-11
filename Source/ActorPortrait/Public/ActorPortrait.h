// Mans Isaksson 2020

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Input/Reply.h"
#include "Components/ContentWidget.h"
#include "ActorPortraitSettings.h"
#include "ActorPortrait.generated.h"

UCLASS()
class ACTORPORTRAIT_API UActorPortrait : public UContentWidget
{
	GENERATED_BODY()
public:
	
	DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(FEventReply, FOnMotionEvent, const FGeometry&, Geometry, const FMotionEvent&, MotionEvent);
	DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(FEventReply, FOnKeyEvent, const FGeometry&, Geometry, const FKeyEvent&, KeyEvent, EInputEvent, InputEvent);
	DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(FEventReply, FOnAnalogEvent, const FGeometry&, Geometry, const FAnalogInputEvent&, AnalogInputEvent);
	DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(FEventReply, FOnFocusEvent, const FGeometry&, Geometry, const FFocusEvent&, FocusEvent);
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnNoReplyFocusEvent, const FGeometry&, Geometry, const FFocusEvent&, FocusEvent);
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnNoReplyPointerEvent, const FGeometry&, Geometry, const FPointerEvent&, PointerEvent);
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpawnPortraitActor, AActor*, Actor);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCameraReset);

private:
	TSharedPtr<class SActorPortrait> ViewportWidget;

	struct FPropertyDirtyFlags
	{
		uint8 bBackgroundWorldDirty:1;
		uint8 bActorClassDirty:1;
		uint8 bActorTransformDirty:1;
		uint8 bSkySphereClassDirty:1;
		uint8 bCameraSettingsDirty:1;
		uint8 bPostProcessingDirty:1;
		uint8 bLightTemplatesDirty:1;
		uint8 bCubemapDirty:1;
		uint8 bUserDataDirty:1;

		void ClearFlags() { FMemory::Memzero(*this); }

		bool IsSkySphereDirty() const { return bUserDataDirty || bSkySphereClassDirty || bCubemapDirty; }

		bool CameraNeedsReset() const { return bUserDataDirty || bCameraSettingsDirty || bActorTransformDirty || bActorClassDirty || bBackgroundWorldDirty; }

		FPropertyDirtyFlags()
		{
			ClearFlags();
		}
	} DirtyFlags;

protected:

	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, ColorAndOpacity);

public:

	/** Color and opacity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=(sRGB="true"))
	FLinearColor ColorAndOpacity;

	/** A bindable delegate for the ColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ColorAndOpacityDelegate;

	/** Default desired size of the portrait */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FVector2D PortraitSize;

	// Custom material to use when rendering the portrait texture
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", AdvancedDisplay, meta=(DisplayThumbnail="true"))
	UMaterialInterface* RenderMaterial;

	// The name of the dynamic texture parameter in the RenderMaterial (Ignored if RenderMaterial is null)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", AdvancedDisplay)
	FName TexureParameter;


	// Toggles the portrait in the designer, useful for improving designer performance
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait")
	bool bShowInDesigner;

	// The actor class to use for the portrait actor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait")
	TSubclassOf<AActor> ActorClass;

	// Initial transform of the actor in the portrait
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait")
	FTransform ActorTransform;

	// The default camera settings for the portrait
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait", meta=(ShowOnlyInnerProperties))
	FPortraitCameraSettings PortraitCameraSettings;

	// Reference to the portrait scene directional light component
	UPROPERTY(EditAnywhere, Instanced, Category="Portrait", meta = (ShowOnlyInnerProperties, NoResetToDefault, DisplayName = "Directional Light Component"))
	class UDirectionalLightComponent* DirectionalLightComponentTemplate;
	
	// Reference to the portrait scene sky light component
	UPROPERTY(EditAnywhere, Instanced, Category="Portrait", meta = (ShowOnlyInnerProperties, NoResetToDefault, DisplayName = "Sky Light Component"))
	class USkyLightComponent* SkyLightComponentTemplate;

	UPROPERTY(VisibleAnywhere, Instanced, BlueprintReadOnly, Category="Portrait", meta = (ShowOnlyInnerProperties, NoResetToDefault))
	class UObject* UserData;


	// This controls whether to tick the portrait world (useful if you want to tick animations or particle effects)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait|Ticking")
	bool bTickWorld;

	// Whether to redraw the portrait each frame
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait|Rendering")
	bool bIsRealTime;

	// The capture source used by the portrait to render the scene
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait|Rendering")
	TEnumAsByte<enum ESceneCaptureSource> CaptureSource;

	// Post processing settings to use for the capture component.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait|Rendering")
	FPostProcessSettings PostProcessingSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_RenderResolutionOverride:1;

	// Override portrait render resolution. Will also override PortraitSize.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait|Rendering", meta=(EditCondition = "bOverride_RenderResolutionOverride"))
	FIntPoint RenderResolutionOverride;

	// Resolution scale of the render target, applied both with ResolutionOverride and when rendering the portrait normally.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portrait|Rendering")
	float ResolutionScale;

	// Whether to lock the mouse to the portrait when clicking and dragging across the portrait
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Input")
	bool bLockMouseDuringCapture;

	// Whether to enable simple default controls, allowing the user to zoom/rotate the portrait scene
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Input")
	bool bUseDefaultInput;

	// Whether to rotate the portrait actor instead of the camera. The actor will be rotated around the orbit origin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Input")
	bool bRotateActor;

	// Which input settings to use with the default input system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Input", meta=(EditCondition="bUseDefaultInput"))
	FPortraitInputSettings PortraitInputSettings;


	// The actor to use as a sky sphere for the portrait scene (Default BP_ActorPortrait_SkySphere). Set to None for no sky-sphere
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portrait", AdvancedDisplay)
	TSubclassOf<AActor> SkySphereClass;

	// User specified data class, by default passed to the sky-sphere to modify environement values
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portrait", AdvancedDisplay)
	TSubclassOf<UObject> UserDataClass;

	// What world to use as a backdrop for this portrait
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portrait", AdvancedDisplay)
	TSoftObjectPtr<UWorld> BackgroundWorldAsset;

public:

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnInputTouchEvent;
	
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnTouchGestureEvent;
	
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnMotionEvent OnInputMotionEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnKeyEvent OnInputKeyEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnAnalogEvent OnInputAxisEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnFocusEvent OnReceiveFocusEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnNoReplyFocusEvent OnLostFocusEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnNoReplyPointerEvent OnCapturedMouseMoveEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnNoReplyPointerEvent OnMouseMoveEvent;
	
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnNoReplyPointerEvent OnMouseEnterEvent;
		
	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnNoReplyPointerEvent OnMouseLeaveEvent;

	// Called after the portrait actor is spawned. 
	// WARNING: Is also called in the editor! Should only be used for to set up initial actor state, such as enabling/dsiabling ticking, 
	// initializing variables, or calling functions directly on the actor.
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnSpawnPortraitActor OnSpawnPortraitActorEvent;

	// Called right before the portrait actor is spawned e.g. before BeginPlay gets called.
	// WARNING: Is not only called in the editor, but als also before called before any components have been registered!
	// You cannot modify default component behaviour with this event, for that you need to use OnSpawnPortraitActorEvent, or change the default settings in your blueprint.
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnSpawnPortraitActor PreSpawnPortraitActorEvent;

	// Invoked when the camera is reset. Can be useful if you want to tweak the auto-generated location of the camera.
	// It can also be used to initialize custom camera settings as the camera reset has to be delayed until the widget is first rendered.
	// This is due to the camera framing needing to know the size of the widget before it can frame the actor.
	UPROPERTY(BlueprintAssignable, Category=Events)
	FOnCameraReset PostCameraResetEvent;

public:

	UActorPortrait();

	// Returns the underlying world used to draw the portrait actor
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Scene")
	UWorld* GetPortraitWorld() const;

	// Returns sky sphere actor from the portrait world, nullptr if there is none
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Scene")
	AActor* GetPortraitSkySphereActor() const;

	// Returns portrait actor from the portrait world
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Scene")
	AActor* GetPortraitActor() const;

	// Returns the default directional light component spawned by the portrait scene
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Scene")
	UDirectionalLightComponent* GetDirectionalLightComponent() const;

	// Returns the default sky light component spawned by the portrait scene
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Scene")
	USkyLightComponent* GetSkyLightComponent() const;

	// Returns the capture component used to render the portrait world
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Scene")
	class USceneCaptureComponent2D* GetCaptureComponent() const;

	
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColorAndOpacity(FLinearColor InColorAndOpacity);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetOpacity(float InOpacity);

	/** 
	* Sets the render material used to draw the underlying render texture
	* 
	* @param InRenderMaterial  The material used to draw the underlying render texture
	* @param InTexureParameter The name of the texture parameter in the render material used to feed in the render texture into the material
	*/ 
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetRenderMaterial(UMaterialInterface* InRenderMaterial, FName InTexureParameter);

	// Set whether the underlying portrait world should tick
	UFUNCTION(BlueprintCallable, Category="Portrait Widget|Ticking")
	void SetTickWorld(bool bInTickWorld);

	// Set whether this portrait should update in real time. If true, the portrait will re-draw whenever the widget is updated
	UFUNCTION(BlueprintCallable, Category="Portrait Widget|Rendering")
	void SetIsRealTime(bool bInIsRealTime);

	// Set the capture source used by the capture component to draw the portrait world
	UFUNCTION(BlueprintCallable, Category="Portrait Widget|Rendering")
	void SetCaptureSource(ESceneCaptureSource InCaptureSource);


	/** 
	* Set currently viewed actor class. If the class is different from the currently viewed actor class, the actor will be re-created. 
	* 
	* @param NewActorClass  The new actor class to use for the portrait actor
	* @param bResetCamera   Whether to reset the automatic camera framing if the actor is re-created
	*/
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Scene")
	void SetPortraitActorClass(TSubclassOf<AActor> NewActorClass, bool bResetCamera = true);

	// Set the post processing settings used by the capture component to draw the portrait world
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Scene")
	void SetPortraitPostProcessSettings(FPostProcessSettings NewPostProcessingSettings);

	/** 
	* Set the actor class to use for the sky sphere. If the class is different from the current sky sphere class, the sky sphere will be re-created. 
	* 
	* @param NewSkySphereClass  The new actor class to use for the sky sphere
	*/
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Scene")
	void SetPortraitSkySphereClass(TSubclassOf<AActor> NewSkySphereClass);

	/** 
	* Set the object class to use for the user data. If the class is different from the current user data class, the sky sphere will be re-created.
	* 
	* @param NewActorClass  The new object class to use for the user data
	*/
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Scene")
	void SetPortraitUserDataClass(TSubclassOf<UObject> InUserDataClass);

	/** 
	* Set the world asset to use for the background world. 
	* IMPORTANT: If the world asset changes, the portrait scene will be re-created, incuding the portrait actor, and any other actors in the world.
	* 
	* @param NewBackgroundWorldAsset  The new asset to use for the background, nullptr if no background world should be used
	*/
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Scene")
	void SetPortraitBackgroundWorld(TSoftObjectPtr<UWorld> NewBackgroundWorldAsset);

	// Applies any changes to the user data. Will cause the sky to be re-captured
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Scene")
	void ApplyUserData();

	
	// Whether to lock the mouse in place while pressing and draging on the actor portrait
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Input")
	void SetLockMouseDuringCapture(bool bInLockMouseDuringCapture);

	// Whether to use the default input system in the actor portrait to rotate the camera/actor when the user interacts with the portrait widget
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Input")
	void SetUseDefaultInput(bool bInUseDefaultInput);

	/** 
	* Set the camera settings used for the automatic camera framing.
	* 
	* @param InPortraitCameraSettings  The new asset to use for the background, nullptr if no background world should be used
	* @param bResetCamera              Whether to reset the automatic camera framing
	*/
	UFUNCTION(BlueprintCallable, Category="Portrait Widget|Camera")
	void SetPortraitCameraSettings(FPortraitCameraSettings InPortraitCameraSettings, bool bResetCamera = true);

	// Manually set the camera location and rotation
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void SetCameraTransform(FVector Location, FRotator Rotation);

	// Get the point at which the camera/actor will rotate around when using camera orbit/actor rotation
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portrait Widget|Camera")
	FVector GetCameraOrbitOrigin() const;

	// Set the point at which the camera/actor will rotate around when using camera orbit/actor rotation
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void SetCameraOrbitOrigin(FVector NewOrbitOrigin) const;
	
	// Orbit the camera around the camera origin
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void OrbitCamera(float OrbitX, float OrbitY);

	// Add/subtract camera zoom. In perspective zooming moves the camera along its forward axis, in orthographic it instead changes the OrthoWidth
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void ZoomCamera(float DeltaZoom);

	// Set the current camera zoom. In perspective this sets the camera position along its forward axis, in orthographic it instead sets the OrthoWidth
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void SetCameraZoom(float NewZoom);

	// Resets the camera auto framing
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void ResetCamera();

	// Rotate the actor given an X/Y input
	UFUNCTION(BlueprintCallable, Category = "Portrait Widget|Camera")
	void RotateActor(float RotateX, float RotateY);

	// Returns tue if called fom an actor or a component which is in a actor protrait scene.
	UFUNCTION(BlueprintPure, Category = "Portrait Widget", meta=(WorldContext="WorldContextObject"))
	static bool IsInPortraitScene(UObject* WorldContextObject);

	// If called fom an actor or a component which is in a actor protrait scene, it will return the SceneCaptureComponent2D used to render this portrait.
	UFUNCTION(BlueprintPure, Category = "Portrait Widget", meta=(WorldContext="WorldContextObject"))
	static USceneCaptureComponent2D* GetPortraitCaptureComponent(UObject* WorldContextObject);

	// If called fom an actor or a component which is in a actor protrait scene, it will return the DirectionalLightComponent used light up the scene.
	UFUNCTION(BlueprintPure, Category = "Portrait Widget", meta=(WorldContext="WorldContextObject"))
	static UDirectionalLightComponent* GetPortraitDirectionalLightComponent(UObject* WorldContextObject);

	// If called fom an actor or a component which is in a actor protrait scene, it will return the USkyLightComponent used light up the scene.
	UFUNCTION(BlueprintPure, Category = "Portrait Widget", meta=(WorldContext="WorldContextObject"))
	static USkyLightComponent* GetPortraitSkyLightComponent(UObject* WorldContextObject);

protected:

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PreEditChange(class FEditPropertyChain& EditPropertyChain) override;
#endif
	//~ End UObject interface

	// Begin UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

	// Begin UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual void OnDesignerChanged(const FDesignerChangedEventArgs& EventArgs) override;
#endif
	// End of UWidget interface

private:

	void RecreateUserData();

	UGameInstance* GetOwningGameInstance() const;
};
