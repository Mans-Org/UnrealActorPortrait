// Copyright Mans Isaksson. All Rights Reserved.

#include "ActorPortrait.h"
#include "SActorPortrait.h"
#include "ActorPortraitModule.h"
#include "ActorPortraitSettings.h"
#include "ActorPortraitInterface.h"
#include "ActorPortraitCompatibilityLayer.h"

#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/Widget.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AssertionMacros.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "ActorPortrait"

namespace ActorPortraitHelpers
{
	template<typename T>
	T* NewInstancedSubObj(UObject* Outer, UClass* Class = nullptr)
	{
		EObjectFlags PassObjFlags = Outer->GetMaskedFlags(RF_PropagateToSubObjects);
		if (Outer->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			PassObjFlags |= RF_ArchetypeObject;
		}

		if (Class == nullptr)
		{
			Class = T::StaticClass();
		}

		return NewObject<T>(Outer, Class, NAME_None, PassObjFlags, /*Template =*/nullptr);
	}

	const TCHAR* DefaultPortraitRenderMatPath = TEXT("/ActorPortrait/M_PortraitRenderMat");
	const TCHAR* DefaultTexureParameterName   = TEXT("RenderTexture");
	const TCHAR* DefaultSkySpherePath         = TEXT("/ActorPortrait/SkySphere/BP_ActorPortrait_SkySphere.BP_ActorPortrait_SkySphere_C");
}

UActorPortrait::UActorPortrait()
{
	ColorAndOpacity = FLinearColor::White;
	PortraitSize    = FVector2D(64.f, 64.f);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultRenderMaterial(ActorPortraitHelpers::DefaultPortraitRenderMatPath);
	RenderMaterial  = DefaultRenderMaterial.Object;
	TexureParameter = ActorPortraitHelpers::DefaultTexureParameterName;

	bShowInDesigner                   = true;
	DirectionalLightComponentTemplate = nullptr;
	SkyLightComponentTemplate         = nullptr;
	UserData                          = nullptr;

	bTickWorld    = true;
	bIsRealTime   = true;
	CaptureSource = ESceneCaptureSource::SCS_FinalColorHDR;
	
	// Disable auto exposure as it doesn't work well in a portrait scenario
	PostProcessingSettings.bOverride_AutoExposureMinBrightness = true;
	PostProcessingSettings.AutoExposureMinBrightness           = 1.f;
	PostProcessingSettings.bOverride_AutoExposureMaxBrightness = true;
	PostProcessingSettings.AutoExposureMaxBrightness           = 1.f;

	bOverride_RenderResolutionOverride = false;
	RenderResolutionOverride = FIntPoint(512, 512);
	ResolutionScale = 1.f;

	bLockMouseDuringCapture = false;
	bUseDefaultInput        = true;

	bRotateActor = false;
	
	static ConstructorHelpers::FClassFinder<AActor> DefaultSkySphere(ActorPortraitHelpers::DefaultSkySpherePath);
	SkySphereClass = DefaultSkySphere.Class;
	UserDataClass  = UPortraitEnvironmentSettings::StaticClass();
}

UWorld* UActorPortrait::GetPortraitWorld() const
{
	return ViewportWidget.IsValid() ? ViewportWidget->GetPortraitWorld() : nullptr;
}

AActor* UActorPortrait::GetPortraitSkySphereActor() const
{
	return ViewportWidget.IsValid() ? ViewportWidget->GetSkySphereActor() : nullptr;
}

AActor* UActorPortrait::GetPortraitActor() const
{
	return ViewportWidget.IsValid() ? ViewportWidget->GetPortraitActor() : nullptr;
}

USceneCaptureComponent2D* UActorPortrait::GetCaptureComponent() const
{
	return ViewportWidget.IsValid() ? ViewportWidget->GetCaptureComponent() : nullptr;
}

UDirectionalLightComponent* UActorPortrait::GetDirectionalLightComponent() const
{
	return ViewportWidget.IsValid() ? ViewportWidget->GetDirectionalLightComponent() : nullptr;
}

USkyLightComponent* UActorPortrait::GetSkyLightComponent() const
{
	return ViewportWidget.IsValid() ? ViewportWidget->GetSkyLightComponent() : nullptr;
}

void UActorPortrait::SetColorAndOpacity(FLinearColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetColorAndOpacity(ColorAndOpacity);
	}
}

void UActorPortrait::SetOpacity(float InOpacity)
{
	ColorAndOpacity.A = InOpacity;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetColorAndOpacity(ColorAndOpacity);
	}
}

void UActorPortrait::SetRenderMaterial(UMaterialInterface* InRenderMaterial, FName InTexureParameter)
{
	RenderMaterial = InRenderMaterial;
	TexureParameter = InTexureParameter;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetRenderMaterial(InRenderMaterial, InTexureParameter);
	}
}

void UActorPortrait::SetTickWorld(bool bInTickWorld)
{
	bTickWorld = bInTickWorld;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetTickWorld(bInTickWorld);
	}
}

void UActorPortrait::SetIsRealTime(bool bInIsRealTime)
{
	bIsRealTime = bInIsRealTime;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetRealTime(bInIsRealTime);
	}
}

void UActorPortrait::SetCaptureSource(ESceneCaptureSource InCaptureSource)
{
	CaptureSource = InCaptureSource;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetCaptureSource(InCaptureSource);
	}
}

void UActorPortrait::SetPortraitActorClass(TSubclassOf<AActor> NewActorClass, bool bResetCamera)
{
	const bool bActorClassChanged = ActorClass != NewActorClass;
	ActorClass = NewActorClass;
	if (bActorClassChanged && ViewportWidget.IsValid()) 
	{
		ViewportWidget->RecreatePortraitActor(NewActorClass, ActorTransform, bResetCamera);
	}
}

void UActorPortrait::SetPortraitPostProcessSettings(FPostProcessSettings NewPostProcessingSettings)
{
	PostProcessingSettings = NewPostProcessingSettings;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetPostProcessSettings(PostProcessingSettings);
	}
}

void UActorPortrait::SetPortraitSkySphereClass(TSubclassOf<AActor> NewSkySphereClass)
{
	const bool SkySphereClassChanged = SkySphereClass != NewSkySphereClass;
	SkySphereClass = NewSkySphereClass;
	if (SkySphereClassChanged && ViewportWidget.IsValid())
	{
		ViewportWidget->RecreateSkySphere(SkySphereClass, true);
	}
}

void UActorPortrait::SetPortraitUserDataClass(TSubclassOf<UObject> InUserDataClass)
{
	UserDataClass = InUserDataClass;

	if ((UserData && UserData->GetClass() != UserDataClass.Get()) || (!UserData && UserDataClass.Get()))
	{
		RecreateUserData();
	}

	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetPortraitUserData(UserData);
	}
}

void UActorPortrait::SetPortraitBackgroundWorld(TSoftObjectPtr<UWorld> NewBackgroundWorldAsset)
{
	const bool bBackgroundWorldChanged = BackgroundWorldAsset != NewBackgroundWorldAsset;
	BackgroundWorldAsset = NewBackgroundWorldAsset;

	if (bBackgroundWorldChanged && ViewportWidget.IsValid())
	{
		ViewportWidget->RecreatePortraitScene(BackgroundWorldAsset, ActorClass, ActorTransform, SkySphereClass, DirectionalLightComponentTemplate, SkyLightComponentTemplate, GetOwningGameInstance());
	}
}

void UActorPortrait::ApplyUserData()
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetPortraitUserData(UserData);
	}
}

void UActorPortrait::SetLockMouseDuringCapture(bool bInLockMouseDuringCapture)
{
	bLockMouseDuringCapture = bInLockMouseDuringCapture;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetLockDuringCapture(bInLockMouseDuringCapture);
	}
}

void UActorPortrait::SetUseDefaultInput(bool bInUseDefaultInput)
{
	bUseDefaultInput = bInUseDefaultInput;
}

void UActorPortrait::SetPortraitCameraSettings(FPortraitCameraSettings InPortraitCameraSettings, bool bResetCamera)
{
	PortraitCameraSettings = InPortraitCameraSettings;
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetPortraitCameraSettings(InPortraitCameraSettings, bResetCamera);
	}
}

void UActorPortrait::SetCameraTransform(FVector Location, FRotator Rotation)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetCameraTransform(FTransform(Rotation, Location));
	}
}

FVector UActorPortrait::GetCameraOrbitOrigin() const
{
	if (ViewportWidget.IsValid())
	{
		return ViewportWidget->GetCameraOrbitOrigin();
	}

	return FVector::ZeroVector;
}

void UActorPortrait::SetCameraOrbitOrigin(FVector NewOrbitOrigin) const
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetCameraOrbitOrigin(NewOrbitOrigin);
	}
}

void UActorPortrait::OrbitCamera(float OrbitX, float OrbitY)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->OrbitCamera(OrbitX, OrbitY);
	}
}

void UActorPortrait::ZoomCamera(float DeltaZoom)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->ZoomCamera(DeltaZoom);
	}
}

void UActorPortrait::SetCameraZoom(float NewZoom)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetCameraZoom(NewZoom);
	}
}

void UActorPortrait::ResetCamera()
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->ResetCamera();
	}
}

void UActorPortrait::RotateActor(float RotateX, float RotateY)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->RotateActor(RotateX, RotateY);
	}
}

bool UActorPortrait::IsInPortraitScene(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!IsValid(World))
		return false;

	return SActorPortrait::IsPortraitWorld(World);
}

USceneCaptureComponent2D* UActorPortrait::GetPortraitCaptureComponent(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!IsValid(World))
		return nullptr;

	TWeakPtr<SActorPortrait> WeakActorPortrait = SActorPortrait::FindPortraitWidgetFromWorld(World);
	if (WeakActorPortrait.IsValid())
	{
		TSharedPtr<SActorPortrait> ActorPortrait = WeakActorPortrait.Pin();
		return ActorPortrait->GetCaptureComponent();
	}

	return nullptr;
}

UDirectionalLightComponent* UActorPortrait::GetPortraitDirectionalLightComponent(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!IsValid(World))
		return nullptr;

	TWeakPtr<SActorPortrait> WeakActorPortrait = SActorPortrait::FindPortraitWidgetFromWorld(World);
	if (WeakActorPortrait.IsValid())
	{
		TSharedPtr<SActorPortrait> ActorPortrait = WeakActorPortrait.Pin();
		return ActorPortrait->GetDirectionalLightComponent();
	}

	return nullptr;
}

USkyLightComponent* UActorPortrait::GetPortraitSkyLightComponent(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!IsValid(World))
		return nullptr;

	TWeakPtr<SActorPortrait> WeakActorPortrait = SActorPortrait::FindPortraitWidgetFromWorld(World);
	if (WeakActorPortrait.IsValid())
	{
		TSharedPtr<SActorPortrait> ActorPortrait = WeakActorPortrait.Pin();
		return ActorPortrait->GetSkyLightComponent();
	}

	return nullptr;
}

void UActorPortrait::PostInitProperties()
{
	Super::PostInitProperties();

	if (DirectionalLightComponentTemplate == nullptr && !HasAnyFlags(RF_ClassDefaultObject))
	{
		DirectionalLightComponentTemplate = ActorPortraitHelpers::NewInstancedSubObj<UDirectionalLightComponent>(this);
		DirectionalLightComponentTemplate->Intensity = 1.f;
		DirectionalLightComponentTemplate->LightColor = FColor::White;
		DirectionalLightComponentTemplate->Mobility = EComponentMobility::Movable;
		DirectionalLightComponentTemplate->SetRelativeRotation_Direct(FRotator(45.f, -45.f, 0.f));
	}

	if (SkyLightComponentTemplate == nullptr && !HasAnyFlags(RF_ClassDefaultObject))
	{
		SkyLightComponentTemplate = ActorPortraitHelpers::NewInstancedSubObj<USkyLightComponent>(this);
		SkyLightComponentTemplate->bLowerHemisphereIsBlack = false;
		SkyLightComponentTemplate->SourceType = ESkyLightSourceType::SLS_CapturedScene;
		SkyLightComponentTemplate->Intensity = 1.f;
		SkyLightComponentTemplate->Mobility = EComponentMobility::Movable;
	}

	if ((UserData == nullptr || UserData->GetClass() != UserDataClass.Get()) && !HasAnyFlags(RF_ClassDefaultObject))
	{
		RecreateUserData();
	}
}

#if WITH_EDITOR
void UActorPortrait::PreEditChange(FEditPropertyChain& EditPropertyChain)
{
	Super::PreEditChange(EditPropertyChain);

	FProperty* MemberProperty = EditPropertyChain.GetHead() ? EditPropertyChain.GetHead()->GetValue() : nullptr;

#define HAS_MEMBER_PROPERTY_CHANGED(ClassName, MemberName) \
	((void)sizeof(UEAsserts_Private::GetMemberNameCheckedJunk(((ClassName*)0)->MemberName)), \
	MemberProperty != nullptr && \
	ClassName::StaticClass()->FindPropertyByName(TEXT(#MemberName)) == MemberProperty)

	if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, DirectionalLightComponentTemplate)
	 || HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, SkyLightComponentTemplate))
	{
		DirtyFlags.bLightTemplatesDirty = true;
		if (FProperty* StructProperty = EditPropertyChain.GetTail() ? EditPropertyChain.GetTail()->GetValue() : nullptr)
		{
			const FName StructPropertyName = StructProperty->GetFName();
			DirtyFlags.bCubemapDirty |= StructPropertyName == GET_MEMBER_NAME_CHECKED(USkyLightComponent, SourceType);
			DirtyFlags.bCubemapDirty |= StructPropertyName == GET_MEMBER_NAME_CHECKED(USkyLightComponent, Cubemap);
			DirtyFlags.bCubemapDirty |= StructPropertyName == GET_MEMBER_NAME_CHECKED(USkyLightComponent, SourceCubemapAngle);
			DirtyFlags.bCubemapDirty |= StructPropertyName == GET_MEMBER_NAME_CHECKED(USkyLightComponent, CubemapResolution);
			DirtyFlags.bCubemapDirty |= StructPropertyName == GET_MEMBER_NAME_CHECKED(USkyLightComponent, SkyDistanceThreshold);
			DirtyFlags.bCubemapDirty |= StructPropertyName == GET_MEMBER_NAME_CHECKED(USkyLightComponent, bCaptureEmissiveOnly);
		}
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, UserDataClass)
		  || HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, UserData))
	{
		DirtyFlags.bUserDataDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, BackgroundWorldAsset))
	{
		DirtyFlags.bBackgroundWorldDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, ActorTransform))
	{
		DirtyFlags.bActorTransformDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, PortraitCameraSettings))
	{
		DirtyFlags.bCameraSettingsDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, PostProcessingSettings))
	{
		DirtyFlags.bPostProcessingDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, BackgroundWorldAsset))
	{
		DirtyFlags.bBackgroundWorldDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, ActorClass))
	{
		DirtyFlags.bActorClassDirty = true;
	}
	else if (HAS_MEMBER_PROPERTY_CHANGED(UActorPortrait, SkySphereClass))
	{
		DirtyFlags.bSkySphereClassDirty = true;
	}
}
#endif

void UActorPortrait::OnSlotAdded(UPanelSlot* InSlot)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UActorPortrait::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->SetContent(SNullWidget::NullWidget);
	}
}

TSharedRef<SWidget> UActorPortrait::RebuildWidget()
{
	if (IsDesignTime() && !bShowInDesigner)
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Portrait", "Portrait"))
			];
	}

	SAssignNew(ViewportWidget, SActorPortrait)
		.WorldAsset(BackgroundWorldAsset)
		.PortraitActorClass(ActorClass)
		.PortraitActorTransform(ActorTransform)
		.DirectionalLightTemplate(DirectionalLightComponentTemplate)
		.SkyLightTemplate(SkyLightComponentTemplate)
		.SkySphereClass(SkySphereClass)
		.OwningGameInstance(GetOwningGameInstance())
		.PortraitUserData(UserData)
		.ColorAndOpacity(ColorAndOpacity)
		.PortraitCameraSettings(FPortraitCameraSettings::MergePortraitCameraSettings(PortraitCameraSettings, FPortraitCameraSettings::DefaultCameraSettings()))
		.PostProcessingSettings(PostProcessingSettings)
		.RenderMaterial(RenderMaterial)
		.RenderMaterialTextureParameter(TexureParameter)
		.PortraitSize(PortraitSize)
		.RenderResolutionOverride(bOverride_RenderResolutionOverride ? RenderResolutionOverride : TOptional<FIntPoint>())
		.ResolutionScale(ResolutionScale)
		.bLockDuringCapture(bLockMouseDuringCapture)
		.bRealTime(bIsRealTime)
		.bShouldShowMouseCursor_Lambda([&]()->bool
		{
			UWorld* World = GetWorld();
			if (World && World->IsGameWorld() && World->GetGameInstance())
			{
				APlayerController* PC = GetOwningPlayer();
				if (!PC)
				{
					World->GetGameInstance()->GetFirstLocalPlayerController();
				}
				return PC && PC->ShouldShowMouseCursor();
			}

			return true;
		})
		.bTickWorld(bTickWorld)
		.CaptureSource(CaptureSource)
		
		.OnInputTouchEvent_Lambda([&](const FGeometry& Geometry, const FPointerEvent& PointerEvent)->FReply
		{
			return OnInputTouchEvent.IsBound() ? OnInputTouchEvent.Execute(Geometry, PointerEvent).NativeReply : FReply::Unhandled();
		})
		.OnTouchGestureEvent_Lambda([&](const FGeometry& Geometry, const FPointerEvent& PointerEvent)->FReply
		{
			return OnTouchGestureEvent.IsBound() ? OnTouchGestureEvent.Execute(Geometry, PointerEvent).NativeReply : FReply::Unhandled();
		})
		.OnInputMotionEvent_Lambda([&](const FGeometry& Geometry, const FMotionEvent& MotionEvent)->FReply
		{
			return OnInputMotionEvent.IsBound() ? OnInputMotionEvent.Execute(Geometry, MotionEvent).NativeReply : FReply::Unhandled();
		})
		.OnInputKeyEvent_Lambda([&](const FGeometry& Geometry, const FKeyEvent& KeyEvent, EInputEvent InputEvent)->FReply
		{
			return OnInputKeyEvent.IsBound() ? OnInputKeyEvent.Execute(Geometry, KeyEvent, InputEvent).NativeReply : FReply::Unhandled();
		})
		.OnInputAxisEvent_Lambda([&](const FGeometry& Geometry, const FAnalogInputEvent& AnalogInputEvent)->FReply
		{
			if (OnInputAxisEvent.IsBound())
			{
				const auto Reply = OnInputAxisEvent.IsBound() ? OnInputAxisEvent.Execute(Geometry, AnalogInputEvent).NativeReply : FReply::Unhandled();
				if (Reply.IsEventHandled())
					return Reply;
			}
			
			if (bUseDefaultInput)
			{
				// Default axis input
				const float Delta = AnalogInputEvent.GetAnalogValue();
				const float DeltaTime = FApp::GetDeltaTime();
				const FKey Key = AnalogInputEvent.GetKey();

				const FPortraitAxisMapping* OrbitXMapping = PortraitInputSettings.OrbitXAxisMapping.FindByPredicate([&](const FPortraitAxisMapping& AxisMapping) { return AxisMapping.Key == Key; });
				const FPortraitAxisMapping* OrbitYMapping = PortraitInputSettings.OrbitYAxisMapping.FindByPredicate([&](const FPortraitAxisMapping& AxisMapping) { return AxisMapping.Key == Key; });
				if (OrbitXMapping || OrbitYMapping)
				{
					const float OrbitX = OrbitXMapping ? Delta * DeltaTime * OrbitXMapping->Scale * PortraitInputSettings.OrbitSensitivity : 0.f;
					const float OrbitY = OrbitYMapping ? Delta * DeltaTime * OrbitYMapping->Scale * PortraitInputSettings.OrbitSensitivity : 0.f;
					if (bRotateActor)
					{
						RotateActor(-OrbitX, -OrbitY);
					}
					else
					{
						OrbitCamera(OrbitX, OrbitY);
					}
					
					return FReply::Handled();
				}

				const FPortraitAxisMapping* ZoomMapping = PortraitInputSettings.ZoomAxisMapping.FindByPredicate([&](const FPortraitAxisMapping& AxisMapping) { return AxisMapping.Key == Key; });
				if (ZoomMapping)
				{
					ZoomCamera(Delta * DeltaTime * ZoomMapping->Scale * PortraitInputSettings.ZoomSpeed);
					return FReply::Handled();
				}
			}

			return FReply::Unhandled();
		})
		.OnReceiveFocusEvent_Lambda([&](const FGeometry& Geometry, const FFocusEvent& FocusEvent)->FReply
		{
			return OnReceiveFocusEvent.IsBound() ? OnReceiveFocusEvent.Execute(Geometry, FocusEvent).NativeReply : FReply::Unhandled();
		})
		.OnLostFocusEvent_Lambda([&](const FGeometry& Geometry, const FFocusEvent& FocusEvent)
		{
			OnLostFocusEvent.ExecuteIfBound(Geometry, FocusEvent);
		})
		.OnCapturedMouseMoveEvent_Lambda([&](const FGeometry& Geometry, const FPointerEvent& PointerEvent)
		{
			OnCapturedMouseMoveEvent.ExecuteIfBound(Geometry, PointerEvent);
		})
		.OnMouseMoveEvent_Lambda([&](const FGeometry& Geometry, const FPointerEvent& PointerEvent)
		{
			OnMouseMoveEvent.ExecuteIfBound(Geometry, PointerEvent);
		})
		.OnMouseEnterEvent_Lambda([&](const FGeometry& Geometry, const FPointerEvent& PointerEvent)
		{
			OnMouseEnterEvent.ExecuteIfBound(Geometry, PointerEvent);
		})
		.OnMouseLeaveEvent_Lambda([&](const FGeometry& Geometry, const FPointerEvent& PointerEvent)
		{
			OnMouseLeaveEvent.ExecuteIfBound(Geometry, PointerEvent);
		})
		.OnSpawnPortraitActorEvent_Lambda([&](AActor* Actor)
		{
			OnSpawnPortraitActorEvent.Broadcast(Actor);
		})
		.PreSpawnPortraitActorEvent_Lambda([&](AActor* Actor)
		{
			PreSpawnPortraitActorEvent.Broadcast(Actor);
		})
		.PostCameraResetEvent_Lambda([&]()
		{
			PostCameraResetEvent.Broadcast();
		});

	if (GetChildrenCount() > 0)
	{
		ViewportWidget->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}

	return ViewportWidget.ToSharedRef();
}

void UActorPortrait::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (ViewportWidget.IsValid())
	{
		// Update attribute bindings
		ViewportWidget->SetColorAndOpacity(PROPERTY_BINDING(FSlateColor, ColorAndOpacity));
		ViewportWidget->SetLockDuringCapture(bLockMouseDuringCapture);
		ViewportWidget->SetRealTime(bIsRealTime);
		ViewportWidget->SetTickWorld(bTickWorld);
		ViewportWidget->SetCaptureSource(CaptureSource);
		ViewportWidget->SetPortraitSize(PortraitSize);
		ViewportWidget->SetRenderResolutionOverride(bOverride_RenderResolutionOverride ? RenderResolutionOverride : TOptional<FIntPoint>());
		ViewportWidget->SetResolutionScale(ResolutionScale);
		ViewportWidget->SetRenderMaterial(RenderMaterial, TexureParameter);
	
		if (DirtyFlags.bCameraSettingsDirty)
		{
			ViewportWidget->SetPortraitCameraSettings(FPortraitCameraSettings::MergePortraitCameraSettings(PortraitCameraSettings, FPortraitCameraSettings::DefaultCameraSettings()), true);
		}

		if (DirtyFlags.bPostProcessingDirty)
		{
			ViewportWidget->SetPostProcessSettings(PostProcessingSettings);
		}

		// Update default arguments
		if ((UserData && UserData->GetClass() != UserDataClass.Get()) || (!UserData && UserDataClass.Get()))
		{
			RecreateUserData();
		}

		ViewportWidget->SetPortraitUserData(UserData, false);

		if (DirtyFlags.bBackgroundWorldDirty)
		{
			ViewportWidget->RecreatePortraitScene(BackgroundWorldAsset, ActorClass, ActorTransform, SkySphereClass, DirectionalLightComponentTemplate, SkyLightComponentTemplate, GetOwningGameInstance());
		}
		else
		{
			if (DirtyFlags.bActorClassDirty)
			{
				ViewportWidget->RecreatePortraitActor(ActorClass, ActorTransform, false);
			}
			else if (DirtyFlags.bActorTransformDirty)
			{
				ViewportWidget->SetPortraitActorTransform(ActorTransform, false);
			}

			if (DirtyFlags.bSkySphereClassDirty)
			{
				ViewportWidget->RecreateSkySphere(SkySphereClass, false);
			}

			if (DirtyFlags.bLightTemplatesDirty)
			{
				ViewportWidget->ApplyDirectionalLightTemplate(DirectionalLightComponentTemplate);
				ViewportWidget->ApplySkyLightTemplate(SkyLightComponentTemplate);
			}

			if (DirtyFlags.IsSkySphereDirty())
			{
				ViewportWidget->RecaptureSky();
			}

			if (DirtyFlags.CameraNeedsReset())
			{
				ViewportWidget->ResetCamera();
			}
		}
	}

	DirtyFlags.ClearFlags();
}

void UActorPortrait::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ViewportWidget.Reset();
}

#if WITH_EDITOR
const FText UActorPortrait::GetPaletteCategory()
{
	return LOCTEXT("Advanced", "Advanced");
}

void UActorPortrait::OnDesignerChanged(const FDesignerChangedEventArgs& EventArgs)
{
	if (ViewportWidget.IsValid())
	{
		ViewportWidget->MarkCameraNeedsReset();
	}
}
#endif

void UActorPortrait::RecreateUserData()
{
	if (IsValid(UserData))
	{
		UserData->MarkAsGarbage();
		UserData = nullptr;
	}

	if (UserDataClass.Get())
		UserData = ActorPortraitHelpers::NewInstancedSubObj<UObject>(this, UserDataClass.Get());
}

UGameInstance* UActorPortrait::GetOwningGameInstance() const
{
	return GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
}

#undef LOCTEXT_NAMESPACE