// Copyright Mans Isaksson. All Rights Reserved.

#pragma once
#include "InstanceWorld.h"
#include "ActorPortraitInterface.h"

class FActorPortraitScene : public FInstanceWorld
{
private:
	TObjectPtr<class UDirectionalLightComponent> DirectionalLightComponent = nullptr;
	TObjectPtr<class USkyLightComponent> SkyLightComponent = nullptr;
	TObjectPtr<class USceneCaptureComponent2D> CaptureComponent = nullptr;

	struct FLatentActionManager* LatentActionManagerToRestore = nullptr;
	class FTimerManager* TimerManagerToRestore = nullptr;

public:
	FActorPortraitScene(const TSoftObjectPtr<UWorld> &WorldAsset, UDirectionalLightComponent* DirLightTemplate, USkyLightComponent* SkyLightTemplate, bool bShouldTick, UGameInstance* OwningGameInstance);

	virtual ~FActorPortraitScene();

	void ApplyDirectionalLightTemplate(UDirectionalLightComponent* DirLightTemplate);

	void ApplySkyLightTemplate(USkyLightComponent* SkyLightTemplate);

	void UpdateSkyCaptureContents();

	void UpdateCaptureComponentCaptureContents();

#if WITH_EDITOR
	void EditorTick(float DeltaTime);
#endif

	FORCEINLINE class UDirectionalLightComponent* GetDirectionalLightComponent() const { return DirectionalLightComponent; }
	FORCEINLINE class USkyLightComponent* GetSkyLightComponent() const { return SkyLightComponent; }
	FORCEINLINE class USceneCaptureComponent2D* GetCaptureComponent() const { return CaptureComponent; }

	template<typename T> 
	T* SpawnPortraitActor(const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(SpawnPortraitActor(T::StaticClass(), FTransform::Identity, SpawnParameters), ECastCheckedType::NullAllowed);
	}

	template<typename T> 
	T* SpawnPortraitActor(UClass* Class, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(SpawnPortraitActor(Class, FTransform::Identity, SpawnParameters), ECastCheckedType::NullAllowed);
	}

	AActor* SpawnPortraitActor(UClass* Class, const FTransform& Transform = FTransform::Identity, FActorSpawnParameters SpawnParameters = FActorSpawnParameters())
	{
		UWorld* PortraitWorld = GetWorld();
		if (!IsValid(PortraitWorld))
		{
			return nullptr;
		}

		const bool bShouldFinishSpawning = !SpawnParameters.bDeferConstruction;
		SpawnParameters.bDeferConstruction = true;

		AActor* SpawnedActor = PortraitWorld->SpawnActor(Class, &Transform, SpawnParameters);
		if (SpawnedActor != nullptr && SpawnedActor->Implements<UActorPortraitInterface>())
		{
			IActorPortraitInterface::Execute_PreSpawnActorInPortraitScene(SpawnedActor);
		}

		if (bShouldFinishSpawning && SpawnedActor)
		{
			FinishSpawningPortraitActor(SpawnedActor, Transform);
		}

		return SpawnedActor;
	}

	void FinishSpawningPortraitActor(AActor* Actor, const FTransform& Transform = FTransform::Identity, bool bIsDefaultTransform = false)
	{
		check(Actor && Actor->GetWorld() == GetWorld());

		Actor->FinishSpawning(Transform, bIsDefaultTransform);

		if (Actor->Implements<UActorPortraitInterface>())
		{
			IActorPortraitInterface::Execute_PostSpawnActorInPortraitScene(Actor);
		}
	}

	// ~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// ~End FGCObject interface

private:

	static const TSet<FName>& PropertyBlacklist();

	static void CopyObjectProperties(UObject* Dest, UObject* Src, const TSet<FName>& PropertyBlackList);

	void OnWorldTickStart(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
	void OnWorldTickEnd(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
};