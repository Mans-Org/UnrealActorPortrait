// Mans Isaksson 2020

#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Templates/SubclassOf.h"
#include "UObject/GCObject.h"
#include "InstanceWorld.generated.h"

class FSceneInterface;
class UActorComponent;

UCLASS()
class UInstanceWorldLevelStreamingFixer : public UObject
{
	GENERATED_BODY()
private:
	
	UPROPERTY()
	class ULevelStreaming* LevelStreaming;

	int32 InstanceID;

public:
	void SetStreamingLevel(ULevelStreaming* InLevelStreaming, int32 InInstanceID);

	UFUNCTION()
	void OnLevelShown();
};

class FInstanceWorld : public FGCObject
{
private:
	TObjectPtr<UWorld> World = nullptr;
	int32 InstanceId = 0;
	bool bWorldWasLoadedFromPackage = false;
	TArray<TObjectPtr<UActorComponent>> Components;

public:
	struct ConstructionValues
	{
		ConstructionValues()
			: bAllowAudioPlayback(false)
			, bCreatePhysicsScene(true)
			, bShouldSimulatePhysics(false)
			, bShouldTickWorld(true)
			, bCreateFXSystem(true)
		{
		}

		uint32 bAllowAudioPlayback:1;
		uint32 bCreatePhysicsScene:1;
		uint32 bShouldSimulatePhysics:1;
		uint32 bShouldTickWorld:1;
		uint32 bCreateFXSystem:1;

		TSubclassOf<class AGameModeBase> DefaultGameMode;
		class UGameInstance* OwningGameInstance = nullptr;

		TSoftObjectPtr<UWorld> WorldAsset;

		ConstructionValues& AllowAudioPlayback(const bool bAllow) { bAllowAudioPlayback = bAllow; return *this; }
		ConstructionValues& SetCreatePhysicsScene(const bool bCreate) { bCreatePhysicsScene = bCreate; return *this; }
		ConstructionValues& ShouldSimulatePhysics(const bool bInShouldSimulatePhysics) { bShouldSimulatePhysics = bInShouldSimulatePhysics; return *this; }
		ConstructionValues& SetShouldTickWorld(const bool bInShouldTickWorld) { bShouldTickWorld = bInShouldTickWorld; return *this; }
		ConstructionValues& SetCreateFXSystem(const bool bInCreateFXSystem) { bCreateFXSystem = bInCreateFXSystem; return *this; }

		ConstructionValues& SetDefaultGameMode(TSubclassOf<class AGameModeBase> GameMode) { DefaultGameMode = GameMode; return *this; }
		ConstructionValues& SetOwningGameInstance(class UGameInstance* InGameInstance) { OwningGameInstance = InGameInstance; return *this; }
		ConstructionValues& SetWorldAsset(TSoftObjectPtr<UWorld> InWorldAsset) { WorldAsset = InWorldAsset; return *this; }
	};

	FInstanceWorld(const ConstructionValues& CVS = ConstructionValues());
	virtual ~FInstanceWorld();

	// ~Begin FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	// ~End FGCObject interface

	FORCEINLINE UWorld* GetWorld() const { return World; }

	FORCEINLINE FSceneInterface* GetScene() const { return World ? World->Scene : nullptr; }

	virtual void AddComponentToWorld(class UActorComponent* Component);

	virtual void RemoveComponentFromWorld(class UActorComponent* Component);

private:

	static UWorld* CreateWorldFromAsset(const FSoftObjectPath& InWorldAssetPath, int32 InstanceId);

	static UWorld* CreateEmptyWorld(int32 InstanceId);

	static void InitWorld(UWorld* InWorld, bool bWorldLoadedFromPackage, const ConstructionValues& CVS);

	static void CleanupWorld(TObjectPtr<UWorld>& InWorld, bool bWorldLoadedFromPackage);

	static int32 GenerateUniqueInstanceWorldId();
};