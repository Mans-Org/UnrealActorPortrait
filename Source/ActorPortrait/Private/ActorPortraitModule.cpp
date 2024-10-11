// Mans Isaksson 2020

#include "ActorPortraitModule.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "Editor.h"
#include "GameDelegates.h"
#endif

DEFINE_LOG_CATEGORY(LogActorPortrait);

void FActorPortraitModule::StartupModule()
{
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FActorPortraitModule::OnApplicationQuit);
#if WITH_EDITOR
	FEditorDelegates::PrePIEEnded.AddRaw(this, &FActorPortraitModule::OnPrePIEEnded);
	FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FActorPortraitModule::OnEndPlayMap);
#endif
}

void FActorPortraitModule::ShutdownModule()
{
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
#if WITH_EDITOR
	FEditorDelegates::PrePIEEnded.RemoveAll(this);
	FGameDelegates::Get().GetEndPlayMapDelegate().RemoveAll(this);
#endif
}

bool FActorPortraitModule::IsShuttingDown()
{
	return FModuleManager::GetModuleChecked<FActorPortraitModule>(TEXT("ActorPortrait")).bIsShuttingDown;
}

bool FActorPortraitModule::IsEndingPlay()
{
	return FModuleManager::GetModuleChecked<FActorPortraitModule>(TEXT("ActorPortrait")).bIsEndingPlay;
}

void FActorPortraitModule::OnApplicationQuit()
{
	bIsShuttingDown = true;
}

void FActorPortraitModule::OnPrePIEEnded(bool bIsSimulatingInEditor)
{
	bIsEndingPlay = true;
}

void FActorPortraitModule::OnEndPlayMap()
{
	bIsEndingPlay = false;
}

IMPLEMENT_MODULE(FActorPortraitModule, ActorPortrait)
