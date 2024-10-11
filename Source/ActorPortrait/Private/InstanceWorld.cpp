// Copyright Mans Isaksson. All Rights Reserved.

#include "InstanceWorld.h"

#include "AI/NavigationSystemBase.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureCube.h"
#include "Engine/LevelStreaming.h"
#include "Engine/WorldComposition.h"
#include "ShaderCompiler.h"
#include "AudioDevice.h"
#include "UObject/Package.h"

#include "ActorPortraitModule.h"
#include "ActorPortraitCompatibilityLayer.h"

DEFINE_LOG_CATEGORY_STATIC(LogInstanceWorld, Log, All);

struct FInstanceWorldHelpers
{
	static const FString InstancePrefix;

	/** Package names currently being duplicated, needed by FixupForInstance */
	static TSet<FName> InstancePackageNames;

	static FString ConvertToInstancePackageName(const FString& PackageName, int32 InstanceID)
	{
		const FString PackageAssetName = FPackageName::GetLongPackageAssetName(PackageName);

		if (HasInstancePrefix(PackageAssetName))
		{
			return PackageName;
		}
		else
		{
			check(InstanceID != -1);
			const FString PackageAssetPath = FPackageName::GetLongPackagePath(PackageName);
			const FString PackagePrefix = BuildInstancePackagePrefix(InstanceID);
			return FString::Printf(TEXT("%s/%s%s"), *PackageAssetPath, *PackagePrefix, *PackageAssetName);
		}
	}

	static FString BuildInstancePackagePrefix(int32 InstanceID)
	{
		return FString::Printf(TEXT("%s_%d_"), *InstancePrefix, InstanceID);
	}

	static void AddInstancePackageName(FName NewPIEPackageName)
	{
		InstancePackageNames.Add(NewPIEPackageName);
	}

	static FString StripPrefixFromPackageName(const FString& PrefixedName, const FString& Prefix)
	{
		FString ResultName;
		if (HasInstancePrefix(PrefixedName))
		{
			const FString ShortPrefixedName = FPackageName::GetLongPackageAssetName(PrefixedName);
			const FString NamePath = FPackageName::GetLongPackagePath(PrefixedName);
			ResultName = NamePath + "/" + ShortPrefixedName.RightChop(Prefix.Len());
		}
		else
		{
			ResultName = PrefixedName;
		}

		return ResultName;
	}

	static bool HasInstancePrefix(const FString& PackageName)
	{
		const FString ShortPrefixedName = FPackageName::GetLongPackageAssetName(PackageName);
		return ShortPrefixedName.StartsWith(FString::Printf(TEXT("%s_"), *InstancePrefix), ESearchCase::CaseSensitive);
	}

	static void ReinitializeWorldCompositionForInstance(UWorldComposition* WorldComposition, int32 InstanceID)
	{
		UWorldComposition::FTilesList& Tiles = WorldComposition->GetTilesList();
		for (int32 TileIdx = 0; TileIdx < Tiles.Num(); ++TileIdx)
		{
			FWorldCompositionTile& Tile = Tiles[TileIdx];

			FString InstancePackageName = FInstanceWorldHelpers::ConvertToInstancePackageName(Tile.PackageName.ToString(), InstanceID);
			Tile.PackageName = FName(*InstancePackageName);
			FInstanceWorldHelpers::AddInstancePackageName(Tile.PackageName);
			for (FName& LODPackageName : Tile.LODPackageNames)
			{
				FString InstanceLODPackageName = FInstanceWorldHelpers::ConvertToInstancePackageName(LODPackageName.ToString(), InstanceID);
				LODPackageName = FName(*InstanceLODPackageName);
				FInstanceWorldHelpers::AddInstancePackageName(LODPackageName);
			}
		}
	}

	static void RenameSteamingLevelForInstance(ULevelStreaming* StreamingLevel, int32 InstanceID)
	{
		// Apply instance prefix to this level references
		if (StreamingLevel->PackageNameToLoad == NAME_None)
		{
			// TODO: We always load a fresh level, should not need to Strip the Prefix
			FString NonPrefixedName = FInstanceWorldHelpers::StripPrefixFromPackageName(StreamingLevel->GetWorldAssetPackageName(), FInstanceWorldHelpers::BuildInstancePackagePrefix(InstanceID));
			StreamingLevel->PackageNameToLoad = FName(*NonPrefixedName);
		}

		FName PlayWorldStreamingPackageName = FName(*FInstanceWorldHelpers::ConvertToInstancePackageName(StreamingLevel->GetWorldAssetPackageName(), InstanceID));
		FInstanceWorldHelpers::AddInstancePackageName(PlayWorldStreamingPackageName);
		StreamingLevel->SetWorldAssetByPackageName(PlayWorldStreamingPackageName);

		// Rename LOD levels if any
		if (StreamingLevel->LODPackageNames.Num() > 0)
		{
			StreamingLevel->LODPackageNamesToLoad.Reset(StreamingLevel->LODPackageNames.Num());
			for (FName& LODPackageName : StreamingLevel->LODPackageNames)
			{
				// Store LOD level original package name
				StreamingLevel->LODPackageNamesToLoad.Add(LODPackageName);

				// Apply Instance prefix to package name			
				const FName NonPrefixedLODPackageName = LODPackageName;
				LODPackageName = FName(*FInstanceWorldHelpers::ConvertToInstancePackageName(LODPackageName.ToString(), InstanceID));
				FInstanceWorldHelpers::AddInstancePackageName(LODPackageName);
			}
		}
	}

	static void RedirectObjectSoftReferencesToInstance(UObject* Object, int32 InstanceID)
	{
		struct FSoftPathInstanceFixupSerializer : public FArchiveUObject
		{
			int32 InstanceID;

			FSoftPathInstanceFixupSerializer(int32 InInstanceID)
				: InstanceID(InInstanceID)
			{
				this->SetIsSaving(true);
			}

			static void FixupForInstance(FSoftObjectPath& SoftPath, int32 InstanceID)
			{
				if (InstanceID != INDEX_NONE && !SoftPath.IsNull())
				{
					const FString Path = SoftPath.ToString();

					// Determine if this reference has already been fixed up for instance
					if (!HasInstancePrefix(Path))
					{
						// Name of the ULevel subobject of UWorld, set in InitializeNewWorld
						const bool bIsChildOfLevel = SoftPath.GetSubPathString().StartsWith(TEXT("PersistentLevel."));

						const FString ShortPackageOuterAndName = FPackageName::GetLongPackageAssetName(Path);

						FString InstancePath;
						if (GEngine->IsEditor())
						{
							// NOTE: For some reason, we get the INST_X prefix on both the package and the object in the editor so we need to some some extra fiddling with the path.
							FString PackageName;
							FString ObjectName;
							ShortPackageOuterAndName.Split(".", &PackageName, &ObjectName);
							const FString PrefixedPackageName = FString::Printf(TEXT("%s%s"), *FInstanceWorldHelpers::BuildInstancePackagePrefix(InstanceID), *PackageName);
							const FString PrefixedObjectName = FString::Printf(TEXT("%s%s"), *FInstanceWorldHelpers::BuildInstancePackagePrefix(InstanceID), *ObjectName);
							const FString PrefixedShortPackageOuterAndName = FString::Printf(TEXT("%s.%s"), *PrefixedPackageName, *PrefixedObjectName);
							InstancePath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(Path), *PrefixedShortPackageOuterAndName);
						}
						else
						{
							InstancePath = FString::Printf(TEXT("%s/%s%s"), *FPackageName::GetLongPackagePath(Path), *FInstanceWorldHelpers::BuildInstancePackagePrefix(InstanceID), *ShortPackageOuterAndName);
						}

						const FName InstancePackage = (!bIsChildOfLevel ? FName(*FPackageName::ObjectPathToPackageName(InstancePath)) : NAME_None);

						// Duplicate if this an already registered Instance package or this looks like a level subobject reference
						if (bIsChildOfLevel || InstancePackageNames.Contains(InstancePackage))
						{
							// Need to prepend Instance prefix, as we're in a Instance world and this refers to an object in an Instance package
							SoftPath.SetPath(MoveTemp(InstancePath));
						}
					}
				}
			};

			FArchive& operator<<(FSoftObjectPath& Value)
			{
				FixupForInstance(Value, InstanceID);
				return *this;
			}
		};

		/*
			TODO: Do we want to recursively fix up all sub-objects?
		*/

		FSoftPathInstanceFixupSerializer FixupSerializer(InstanceID);

		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(Object, SubObjects);
		for (UObject* SubObject : SubObjects)
			SubObject->Serialize(FixupSerializer);

		Object->Serialize(FixupSerializer);
	}

	static void RenameToInstanceWorld(UWorld* World, int32 InstanceID)
	{
		if (World->WorldComposition)
		{
			FInstanceWorldHelpers::ReinitializeWorldCompositionForInstance(World->WorldComposition, InstanceID);
		}

		for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
		{
			RenameSteamingLevelForInstance(LevelStreaming, InstanceID);
		}

		FInstanceWorldHelpers::RedirectObjectSoftReferencesToInstance(World->PersistentLevel, InstanceID);
	}

	static void DeleteInstanceWorldPackage(UPackage* InstanceWorldPackage)
	{
		check(InstanceWorldPackage != nullptr);

		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithPackage(InstanceWorldPackage, ObjectsInPackage);

		for (UObject* ObjectToDestroy : ObjectsInPackage)
		{
			if (!IsValid(ObjectToDestroy))
			{
				ObjectToDestroy->MarkPackageDirty();

				ObjectToDestroy->ClearFlags(RF_Public | RF_Standalone);
				ObjectToDestroy->SetFlags(RF_Transient);
				ObjectToDestroy->MarkAsGarbage();

				if (ObjectToDestroy->IsRooted())
				{
					ObjectToDestroy->RemoveFromRoot();
				}
			}
		}

		// Mark its package as dirty as we're going to delete it.
		InstanceWorldPackage->MarkPackageDirty();

		// Remove standalone flag so garbage collection can delete the object.
		InstanceWorldPackage->ClearFlags(RF_Standalone);

		InstanceWorldPackage->MarkAsGarbage();
	}

	static UWorld* CreateInstanceWorldByLoadingFromPackage(const FSoftObjectPath& WorldAsset, int32 InstanceID)
	{
		const FString WorldPackageName = UWorld::RemovePIEPrefix(FPackageName::ObjectPathToPackageName(WorldAsset.ToString()));
		const FString InstancePackageName = FInstanceWorldHelpers::ConvertToInstancePackageName(WorldPackageName, InstanceID);

		// Set the world type in the static map, so that UWorld::PostLoad can set the world type
		UWorld::WorldTypePreLoadMap.FindOrAdd(*InstancePackageName) = EWorldType::GamePreview;
		FInstanceWorldHelpers::AddInstancePackageName(*InstancePackageName);

		UPackage* ExistingPackage = FindPackage(nullptr, *InstancePackageName);
		checkf(ExistingPackage == nullptr, TEXT("Instance world package already exists. Package: %s"), *InstancePackageName);

		// Loads the contents of "WorldPackageName" into a new package "NewWorldPackage"
		UPackage* InstancePackage = CreatePackage(*InstancePackageName);
		InstancePackage->SetFlags(EObjectFlags::RF_Transient);
		UPackage* NewWorldPackage = LoadPackage(InstancePackage, *WorldPackageName, LOAD_None);

		// Clean up the world type list now that PostLoad has occurred
		UWorld::WorldTypePreLoadMap.Remove(*InstancePackageName);

		if (NewWorldPackage == nullptr)
		{
			UE_LOG(LogInstanceWorld, Error, TEXT("Failed to load world package %s"), *WorldPackageName);
			DeleteInstanceWorldPackage(InstancePackage);
			return nullptr;
		}

		UWorld* NewWorld = UWorld::FindWorldInPackage(NewWorldPackage);

		// If the world was not found, follow a redirector if there is one.
		if (!NewWorld)
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(NewWorldPackage);
			if (NewWorld)
			{
				NewWorldPackage = NewWorld->GetOutermost();
				DeleteInstanceWorldPackage(InstancePackage);
			}
		}

		if (!NewWorld)
		{
			UE_LOG(LogInstanceWorld, Error, TEXT("Could not find a world in package %s"), *InstancePackageName);
			DeleteInstanceWorldPackage(InstancePackage);
			return nullptr;
		}

		if (!NewWorld->PersistentLevel)
		{
			UE_LOG(LogInstanceWorld, Error, TEXT("Loaded world does not contain a PersistentLevel %s"), *WorldAsset.ToString());
			DeleteInstanceWorldPackage(InstancePackage);
			return nullptr;
		}

		FInstanceWorldHelpers::RenameToInstanceWorld(NewWorld, InstanceID);

		return NewWorld;
	}
};

const FString FInstanceWorldHelpers::InstancePrefix     = TEXT("INST");
TSet<FName> FInstanceWorldHelpers::InstancePackageNames = {};

void UInstanceWorldLevelStreamingFixer::SetStreamingLevel(ULevelStreaming* InLevelStreaming, int32 InInstanceID)
{
	if (!IsValid(InLevelStreaming))
	{
		UE_LOG(LogInstanceWorld, Error, TEXT("UInstanceWorldLevelStreamingFixer::SetStreamingLevel, received invalid LevelStreaming"));
		return;
	}

	if (InInstanceID == INDEX_NONE)
	{
		UE_LOG(LogInstanceWorld, Error, TEXT("UInstanceWorldLevelStreamingFixer::SetStreamingLevel, received invalid instance world index"));
		return;
	}

	LevelStreaming = InLevelStreaming;
	InstanceID = InInstanceID;
	LevelStreaming->OnLevelShown.AddDynamic(this, &UInstanceWorldLevelStreamingFixer::OnLevelShown);
}

void UInstanceWorldLevelStreamingFixer::OnLevelShown()
{
	if (!IsValid(LevelStreaming))
	{
		UE_LOG(LogInstanceWorld, Error, TEXT("UInstanceWorldLevelStreamingFixer::OnLevelShown, LevelStreaming has been destroyed"));
		return;
	}

	FInstanceWorldHelpers::RedirectObjectSoftReferencesToInstance(LevelStreaming->GetLoadedLevel(), InstanceID);
}

FInstanceWorld::FInstanceWorld(const ConstructionValues& CVS)
{
	InstanceId = GenerateUniqueInstanceWorldId();

	double StartTime = 0.f;

	const FSoftObjectPath& WorldAssetPath = CVS.WorldAsset.ToSoftObjectPath();
	bWorldWasLoadedFromPackage = WorldAssetPath.IsValid();

	if (bWorldWasLoadedFromPackage)
	{
		StartTime = FPlatformTime::Seconds();
		World = CreateWorldFromAsset(WorldAssetPath, InstanceId);

		if (!World)
		{
			UE_LOG(LogInstanceWorld, Error, TEXT("Failed to create instanced world from %s"), *WorldAssetPath.ToString());
			return;
		}
	}
	else
	{
		World = CreateEmptyWorld(InstanceId);
	}

	InitWorld(World, bWorldWasLoadedFromPackage, CVS);

	if (bWorldWasLoadedFromPackage)
	{
		double StopTime = FPlatformTime::Seconds();
		UE_LOG(LogInstanceWorld, Verbose, TEXT("Took %f seconds to LoadMap(%s)"), StopTime - StartTime, *WorldAssetPath.GetAssetName());
	}
}

FInstanceWorld::~FInstanceWorld()
{
	for (UActorComponent* ActorComponent : Components)
	{
		ActorComponent->DestroyComponent();
	}
	Components.Empty();
	CleanupWorld(World, bWorldWasLoadedFromPackage);
}

void FInstanceWorld::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(World);
	Collector.AddReferencedObjects(Components);
}

FString FInstanceWorld::GetReferencerName() const
{
	return FString::Printf(TEXT("FInstanceWorld_%d"), InstanceId);
}

UWorld* FInstanceWorld::CreateWorldFromAsset(const FSoftObjectPath& InWorldAssetPath, int32 InstanceId)
{
	UWorld* World = FInstanceWorldHelpers::CreateInstanceWorldByLoadingFromPackage(InWorldAssetPath, InstanceId);
	if (!World)
	{
		return nullptr;
	}

	// Register LevelStreamingFixers (Will fixup the sub-level references one the levels are loaded/shown)
	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		if (UInstanceWorldLevelStreamingFixer* LevelStreamingFixer = NewObject<UInstanceWorldLevelStreamingFixer>(LevelStreaming))
			LevelStreamingFixer->SetStreamingLevel(LevelStreaming, InstanceId);
	}

	return World;
}

UWorld* FInstanceWorld::CreateEmptyWorld(int32 InstanceId)
{
	UObject* Outer = GetTransientPackage();
	const FName WorldName = *FString::Printf(TEXT("%sEmptyInstanceWorld"), *FInstanceWorldHelpers::BuildInstancePackagePrefix(InstanceId));
	UWorld* OutWorld = NewObject<UWorld>(Outer, MakeUniqueObjectName(Outer, UWorld::StaticClass(), WorldName));
	OutWorld->WorldType = EWorldType::GamePreview;
	return OutWorld;
}

void FInstanceWorld::AddComponentToWorld(UActorComponent* Component)
{
	if (!ensureMsgf(IsValid(GetWorld()), TEXT("Cannot add components to invalid instance world")))
	{
		return;
	}

	if (ensure(IsValid(Component) && !Component->IsRegistered()))
	{
		Component->RegisterComponentWithWorld(GetWorld());
		Components.AddUnique(Component);
	}
}

void FInstanceWorld::RemoveComponentFromWorld(UActorComponent* Component)
{
	if (!ensureMsgf(IsValid(GetWorld()), TEXT("Cannot remove components from invalid instance world")))
	{
		return;
	}

	if (ensure(Component != nullptr && Component->IsRegistered()))
	{
		Component->UnregisterComponent();
	}

	Components.Remove(Component);
}

void FInstanceWorld::InitWorld(UWorld* InWorld, bool bWorldLoadedFromPackage, const ConstructionValues& CVS)
{
	// Mimics the initialization that ocurs in UEngine::LoadMap with some modifications made to make it work with non-game worlds.

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::GamePreview);
	WorldContext.SetCurrentWorld(InWorld);

	FURL URL;
	if (bWorldLoadedFromPackage)
	{
		URL.Map = InWorld->GetOutermost()->GetName();
	}
	WorldContext.LastURL = URL;

	if (!WorldContext.World()->bIsWorldInitialized)
	{
		UWorld::InitializationValues IVS = UWorld::InitializationValues()
			.InitializeScenes(true)
			.AllowAudioPlayback(CVS.bAllowAudioPlayback)
			.RequiresHitProxies(false) // Only Need hit proxies in an editor scene
			.CreatePhysicsScene(CVS.bCreatePhysicsScene)
			.CreateNavigation(false)
			.CreateAISystem(false)
			.ShouldSimulatePhysics(CVS.bShouldSimulatePhysics)
			.EnableTraceCollision(false)
			.CreateFXSystem(CVS.bCreateFXSystem);

		if (bWorldLoadedFromPackage)
		{
			WorldContext.World()->InitWorld(IVS);
		}
		else
		{
			WorldContext.World()->InitializeNewWorld(IVS);
		}
	}

	if (CVS.OwningGameInstance != nullptr)
	{
		InWorld->SetGameInstance(CVS.OwningGameInstance);
		WorldContext.OwningGameInstance = CVS.OwningGameInstance;
	}

	if (FAudioDevice* AudioDevice = WorldContext.World()->GetAudioDeviceRaw())
	{
		AudioDevice->SetDefaultBaseSoundMix(WorldContext.World()->GetWorldSettings()->DefaultBaseSoundMix);
	}

	if (bWorldLoadedFromPackage)
	{
		const TCHAR* MutatorString = URL.GetOption(TEXT("Mutator="), TEXT(""));
		if (MutatorString)
		{
			TArray<FString> Mutators;
			FString(MutatorString).ParseIntoArray(Mutators, TEXT(","), true);

			for (int32 MutatorIndex = 0; MutatorIndex < Mutators.Num(); MutatorIndex++)
			{
				GEngine->LoadPackagesFully(WorldContext.World(), FULLYLOAD_Mutator, Mutators[MutatorIndex]);
			}
		}

		// Process global shader results before we try to render anything
		// Do this before we register components, as USkinnedMeshComponents require the GPU skin cache global shaders when creating render state.
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->ProcessAsyncResults(false, true);
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_LoadInstanceMap_LoadPackagesFully);

			// load any per-map packages
			check(WorldContext.World()->PersistentLevel);
			GEngine->LoadPackagesFully(WorldContext.World(), FULLYLOAD_Map, WorldContext.World()->PersistentLevel->GetOutermost()->GetName());

			// TODO: Causing the game to freeze in shipping
			// Make sure "always loaded" sub-levels are fully loaded
			//WorldContext.World()->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);

			if (!GIsEditor && !IsRunningDedicatedServer())
			{
				// If requested, duplicate dynamic levels here after the source levels are created.
				WorldContext.World()->DuplicateRequestedLevels(FName(*URL.Map));
			}
		}
	}

	// Note that AI system will be created only if ai-system-creation conditions are met
	WorldContext.World()->CreateAISystem();

	// Initialize gameplay for the level.
	{
		// NOTE: We cannot use the FRegisterComponentContext since the Process function is not exposed.
		// The only thing FRegisterComponentContext does is to batch all the Scene->AddPrimitive calls and then executs them in parallel.
		// This could perhaps be a nice optimization but we need to figure out how to call FRegisterComponentContext::Process.
		//FRegisterComponentContext Context(WorldContext.World());
		//WorldContext.World()->InitializeActorsForPlay(URL, true, &Context);
		//Context.Process();

		WorldContext.World()->InitializeActorsForPlay(URL, true, nullptr);
	}

	// calling it after InitializeActorsForPlay has been called to have all potential bounding boxed initialized
	FNavigationSystem::AddNavigationSystemToWorld(*WorldContext.World(), FNavigationSystemRunMode::GameMode);

	// Remember the URL. Put this before spawning player controllers so that
	// a player controller can get the map name during initialization and
	// have it be correct
	WorldContext.LastURL = URL;
	WorldContext.LastURL.Map = URL.Map;
	WorldContext.LastRemoteURL = URL;

	WorldContext.World()->BeginPlay();

	// Copy from AWorldSettings::NotifyBeginPlay
	{
		if (!WorldContext.World()->GetBegunPlay())
		{
			for (FActorIterator It(WorldContext.World()); It; ++It)
			{
				It->DispatchBeginPlay(bWorldLoadedFromPackage);
			}

			WorldContext.World()->SetBegunPlay(true);
		}
	}

	if (bWorldLoadedFromPackage)
	{
		WorldContext.World()->bWorldWasLoadedThisTick = true;
		WorldContext.World()->UpdateAllSkyCaptures();
	}
	
	WorldContext.World()->SetShouldTick(CVS.bShouldTickWorld);

	// TODO: Does it make sense to call this?
	// WorldContext.OwningGameInstance->LoadComplete(StopTime - StartTime, *URL.Map);
}

void FInstanceWorld::CleanupWorld(TObjectPtr<UWorld>& InWorld, bool bWorldLoadedFromPackage)
{
	// Mimics the cleanup that ocurs in UEngine::LoadMap with some modifications made to make it work with non-game worlds.
	
	if (!GEngine || !IsValid(InWorld))
	{
		return;
	}

	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(InWorld);

	if (bWorldLoadedFromPackage)
	{
		// clean up any per-map loaded packages for the map we are leaving
		if (WorldContext.World() && WorldContext.World()->PersistentLevel)
		{
			GEngine->CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Map, WorldContext.World()->PersistentLevel->GetOutermost()->GetName());
		}

		// cleanup the existing per-game pacakges
		// @todo: It should be possible to not unload/load packages if we are going from/to the same GameMode.
		//        would have to save the game pathname here and pass it in to SetGameMode below
		GEngine->CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Game_PreLoadClass, TEXT(""));
		GEngine->CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Game_PostLoadClass, TEXT(""));
		GEngine->CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Mutator, TEXT(""));
	}

	WorldContext.World()->BeginTearingDown();

	// Make sure there are no pending visibility requests.
	WorldContext.World()->bIsLevelStreamingFrozen = false;
	WorldContext.World()->SetShouldForceUnloadStreamingLevels(true);
	WorldContext.World()->FlushLevelStreaming();

	for (FActorIterator ActorIt(WorldContext.World()); ActorIt; ++ActorIt)
	{
		ActorIt->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
	}

	// Do this after destroying pawns/playercontrollers, in case that spawns new things (e.g. dropped weapons)
	WorldContext.World()->DestroyWorld(true);

	// Stop all audio to remove references to current level.
	if (FAudioDevice* AudioDevice = WorldContext.World()->GetAudioDeviceRaw())
	{
		AudioDevice->Flush(WorldContext.World());
		AudioDevice->SetTransientPrimaryVolume(1.0f);
	}

	if (bWorldLoadedFromPackage)
	{
		// Delete the instance world package if we've created one. This will delete all sub-objects of this package, including the world.
		FInstanceWorldHelpers::DeleteInstanceWorldPackage(WorldContext.World()->GetOutermost());
	}

	// Make sure we're cleaning up all the sub-/streaming levels
	{
		// If the world is not part of a package, mark everything else contained in the world to be deleted
		for (auto LevelIt(WorldContext.World()->GetLevelIterator()); LevelIt; ++LevelIt)
		{
			const ULevel* Level = *LevelIt;
			if (Level)
			{
				CastChecked<UWorld>(Level->GetOuter())->MarkObjectsPendingKill();
			}
		}

		for (ULevelStreaming* LevelStreaming : WorldContext.World()->GetStreamingLevels())
		{
			// If an unloaded levelstreaming still has a loaded level we need to mark its objects to be deleted as well
			if (LevelStreaming->GetLoadedLevel() && (!LevelStreaming->ShouldBeLoaded() || !LevelStreaming->ShouldBeVisible()))
			{
				CastChecked<UWorld>(LevelStreaming->GetLoadedLevel()->GetOuter())->MarkObjectsPendingKill();
			}
		}

		WorldContext.World()->MarkAsGarbage();
	}
	
	GEngine->DestroyWorldContext(InWorld);

	InWorld = nullptr;

	// Need to collect garbage to delete the old world so we don't accidentally create a new world with the same name as the one we juat destroyed,
	// because if it's still around by then the engine will assert.
	if (!IsGarbageCollecting() && !FActorPortraitModule::IsShuttingDown() && !FActorPortraitModule::IsEndingPlay())
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, false);
	}
	// GEngine->TrimMemory(); // TODO: Should we also clear render resources for this world by calling GEngine->TrimMemory instead?
}

int32 FInstanceWorld::GenerateUniqueInstanceWorldId()
{
	static int32 StaticInstanceId = 0;
	return ++StaticInstanceId;
}
