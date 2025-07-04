// Copyright Mans Isaksson. All Rights Reserved.

#include "ActorPortraitScene.h"

#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/ReflectionCaptureComponent.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/Package.h"
#include "SceneRenderBuilderInterface.h"

FActorPortraitScene::FActorPortraitScene(const TSoftObjectPtr<UWorld> &WorldAsset, UDirectionalLightComponent* DirLightTemplate, USkyLightComponent* SkyLightTemplate, bool bShouldTick, UGameInstance* OwningGameInstance)
	: FInstanceWorld(FInstanceWorld::ConstructionValues().SetShouldTickWorld(bShouldTick).SetWorldAsset(WorldAsset).SetOwningGameInstance(OwningGameInstance))
{
	check(IsInGameThread());

	if (GetWorld() == nullptr)
	{
		return;
	}

	DirectionalLightComponent = NewObject<UDirectionalLightComponent>(GetTransientPackage(), DirLightTemplate ? DirLightTemplate->GetClass() : UDirectionalLightComponent::StaticClass(), NAME_None, RF_Transient, DirLightTemplate);
	AddComponentToWorld(DirectionalLightComponent);

	SkyLightComponent = NewObject<USkyLightComponent>(GetTransientPackage(), SkyLightTemplate ? SkyLightTemplate->GetClass() : USkyLightComponent::StaticClass(), NAME_None, RF_Transient, SkyLightTemplate);
	AddComponentToWorld(SkyLightComponent);

	CaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	CaptureComponent->bCaptureEveryFrame           = false;
	CaptureComponent->bCaptureOnMovement           = false;
	CaptureComponent->PostProcessBlendWeight       = 1.f;
	CaptureComponent->CompositeMode                = ESceneCaptureCompositeMode::SCCM_Overwrite;
	CaptureComponent->CaptureSource                = ESceneCaptureSource::SCS_FinalColorHDR;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->bConsiderUnrenderedOpaquePixelAsFullyTranslucent = true;
	AddComponentToWorld(CaptureComponent);

	// HACK: Since all worlds share the same GameInstance, they will all share the same LatentActionManager and TimerManager.
	// We therefore use the OnWorldTickStart and OnWorldTickEnd events to override the LatentActionManager and TimerManager during our
	// portrait scene tick to avoid double-ticking the LatentActionManager and TimerManager.
	FWorldDelegates::OnWorldTickStart.AddRaw(this, &FActorPortraitScene::OnWorldTickStart);
	FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FActorPortraitScene::OnWorldTickEnd);
}

FActorPortraitScene::~FActorPortraitScene()
{
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
	FWorldDelegates::OnWorldTickEnd.RemoveAll(this);
}

void FActorPortraitScene::ApplyDirectionalLightTemplate(UDirectionalLightComponent* DirLightTemplate)
{
	if (!IsValid(DirectionalLightComponent)
		|| (!IsValid(DirLightTemplate) && DirectionalLightComponent->GetClass() != UDirectionalLightComponent::StaticClass())
		|| (IsValid(DirLightTemplate) && DirectionalLightComponent->GetClass() != DirLightTemplate->GetClass()))
	{
		if (DirectionalLightComponent)
			DirectionalLightComponent->DestroyComponent();

		DirectionalLightComponent = NewObject<UDirectionalLightComponent>(GetTransientPackage(), DirLightTemplate ? DirLightTemplate->GetClass() : UDirectionalLightComponent::StaticClass(), NAME_None, RF_Transient, DirLightTemplate);
		AddComponentToWorld(DirectionalLightComponent);
	}
	else if (DirLightTemplate)
	{
		CopyObjectProperties(DirectionalLightComponent, DirLightTemplate, PropertyBlacklist());

		// Update Transform (Mainly rotation)
		DirectionalLightComponent->SetRelativeTransform(DirLightTemplate->GetRelativeTransform());
	}

	if (DirectionalLightComponent)
	{
		DirectionalLightComponent->UpdateColorAndBrightness();
		DirectionalLightComponent->MarkRenderStateDirty();
	}
}

void FActorPortraitScene::ApplySkyLightTemplate(USkyLightComponent* SkyLightTemplate)
{
	if (!IsValid(SkyLightComponent)
		|| (!IsValid(SkyLightTemplate) && SkyLightComponent->GetClass() != USkyLightComponent::StaticClass())
		|| (IsValid(SkyLightTemplate) && SkyLightComponent->GetClass() != SkyLightTemplate->GetClass()))
	{
		if (SkyLightComponent)
			SkyLightComponent->DestroyComponent();

		SkyLightComponent = NewObject<USkyLightComponent>(GetTransientPackage(), SkyLightTemplate ? SkyLightTemplate->GetClass() : USkyLightComponent::StaticClass(), NAME_None, RF_Transient, SkyLightTemplate);
		AddComponentToWorld(SkyLightComponent);
	}
	else if (SkyLightTemplate)
	{
		CopyObjectProperties(SkyLightComponent, SkyLightTemplate, PropertyBlacklist());
	}

	if (SkyLightComponent)
	{
		// Temp hack to force refresh some settings
		{
			const auto Constrast = SkyLightComponent->Contrast;
			SkyLightComponent->SetOcclusionContrast(Constrast + 1.f);
			SkyLightComponent->SetOcclusionContrast(Constrast);

			const auto Intensity = SkyLightComponent->Intensity;
			SkyLightComponent->SetIntensity(Intensity + 1.f);
			SkyLightComponent->SetIntensity(Intensity);
		}
		SkyLightComponent->MarkRenderStateDirty();
	}
}

void FActorPortraitScene::UpdateSkyCaptureContents()
{
	if (IsValid(SkyLightComponent))
		SkyLightComponent->SetCaptureIsDirty();

	USkyLightComponent::UpdateSkyCaptureContents(GetWorld());
	UReflectionCaptureComponent::UpdateReflectionCaptureContents(GetWorld());
}

void FActorPortraitScene::UpdateCaptureComponentCaptureContents()
{
	if (IsValid(CaptureComponent))
	{
		TUniquePtr<ISceneRenderBuilder> SceneRenderBuilder = ISceneRenderBuilder::Create(GetWorld()->Scene);
		CaptureComponent->UpdateSceneCaptureContents(GetWorld()->Scene, *SceneRenderBuilder);
		SceneRenderBuilder->Execute();
	}
}

#if WITH_EDITOR
void FActorPortraitScene::EditorTick(float DeltaTime)
{
	// We have to tick the world manually in the editor. Ticking is handled by UGameEngine::Tick in non-editor builds

	if (!GetWorld() || !GetWorld()->ShouldTick())
	{
		return;
	}

	struct FActorPortraitSceneEditorTicker
	{
		static TArray<TWeakObjectPtr<UWorld>>& LatentTickQueue()
		{
			static TArray<TWeakObjectPtr<UWorld>> OutQueue;
			return OutQueue;
		}

		// Mini version of UGameEngine::Tick world ticking which only ticks parts relevant to the portrait scene
		static void TickWorld(UWorld* World, float DeltaTime)
		{
			if (!World->ShouldTick())
				return;

			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_Editor_TickPortrait)
				World->Tick(ELevelTick::LEVELTICK_All, DeltaTime);
			}

			if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_Editor_CheckPortraitCaptures);
				// Update sky light first because it's considered direct lighting, sky diffuse will be visible in reflection capture indirect specular
				USkyLightComponent::UpdateSkyCaptureContents(World);
				UReflectionCaptureComponent::UpdateReflectionCaptureContents(World);
			}
		}

		static void PostEditorTick(float DeltaTime)
		{
			for (auto & World : LatentTickQueue())
			{
				if (!World.IsValid())
					continue;

				FActorPortraitSceneEditorTicker::TickWorld(World.Get(), DeltaTime);
			}
			LatentTickQueue().Empty(LatentTickQueue().Num());
		}

		static void QueueLatentWorldTick(UWorld* World)
		{
			static bool bNeedsInit = true;
			if (bNeedsInit)
			{
				if (GEngine)
					GEngine->OnPostEditorTick().AddStatic(&FActorPortraitSceneEditorTicker::PostEditorTick);
			}

			LatentTickQueue().Add(World);
		}
	};

	static auto IsAnyWorldTicking = []() 
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts()) 
		{
			if (UWorld* World = WorldContext.World())
			{
				if (World->bInTick)
					return true;
			}
		}
		return false;
	};
		
	// We can't tick more than one world at a time, in cases where we are ticked through the level tick (such as 3D Widget Components)
	// we need to delay the tick until the end of the frame.
	if (!IsAnyWorldTicking())
	{
		FActorPortraitSceneEditorTicker::TickWorld(GetWorld(), DeltaTime);
	}
	else
	{
		FActorPortraitSceneEditorTicker::QueueLatentWorldTick(GetWorld());
	}
}
#endif

void FActorPortraitScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FInstanceWorld::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(DirectionalLightComponent);
	Collector.AddReferencedObject(SkyLightComponent);
	Collector.AddReferencedObject(CaptureComponent);
}

const TSet<FName>& FActorPortraitScene::PropertyBlacklist()
{
	const static TSet<FName> Blacklist =
	{
		TEXT("WholeSceneDynamicShadowRadius"),
		TEXT("ShadowMapChannel"),
		TEXT("MinRoughness"),
		TEXT("InverseSquaredFalloff"),
		TEXT("LightGuid"),
		TEXT("Brightness"),
		TEXT("StaticEditorTexture"),
		TEXT("StaticEditorTextureScale"),
		TEXT("DynamicEditorTexture"),
		TEXT("DynamicEditorTextureScale")
	};
	return Blacklist;
}

void FActorPortraitScene::CopyObjectProperties(UObject* Dest, UObject* Src, const TSet<FName>& PropertyBlackList)
{
	// This only works on identical classes
	check(Src->GetClass() == Dest->GetClass());

	FProperty* DestProperty = Dest->GetClass()->PropertyLink;
	FProperty* SrcProperty = Src->GetClass()->PropertyLink;
	while (DestProperty && SrcProperty)
	{
		if (!PropertyBlackList.Contains(SrcProperty->GetFName()))
		{
			DestProperty->CopyCompleteValue(
				DestProperty->ContainerPtrToValuePtr<void>(Dest),
				SrcProperty->ContainerPtrToValuePtr<void>(Src)
			);
		}

		DestProperty = DestProperty->PropertyLinkNext;
		SrcProperty = SrcProperty->PropertyLinkNext;
	}
}

void FActorPortraitScene::OnWorldTickStart(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	UWorld* ThisWorld = GetWorld();
	if (InWorld == ThisWorld)
	{
		if (UGameInstance* OwningGameInstance = ThisWorld->GetGameInstance())
		{
			ThisWorld->SetGameInstance(nullptr); // Will force GetLatentActionManager and GetTimerManager to fetch the UWorld versions of the managers

			LatentActionManagerToRestore = OwningGameInstance->LatentActionManager;
			OwningGameInstance->LatentActionManager = &ThisWorld->GetLatentActionManager();

			TimerManagerToRestore = OwningGameInstance->TimerManager;
			OwningGameInstance->TimerManager = &ThisWorld->GetTimerManager();

			ThisWorld->SetGameInstance(OwningGameInstance);
		}
	}
}

void FActorPortraitScene::OnWorldTickEnd(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	UWorld* ThisWorld = GetWorld();
	if (InWorld == ThisWorld)
	{
		if (UGameInstance* OwningGameInstance = ThisWorld->GetGameInstance())
		{
			if (LatentActionManagerToRestore)
			{
				OwningGameInstance->LatentActionManager = LatentActionManagerToRestore;
				LatentActionManagerToRestore = nullptr;
			}

			if (TimerManagerToRestore)
			{
				OwningGameInstance->TimerManager = TimerManagerToRestore;
				TimerManagerToRestore = nullptr;
			}
		}
	}
}