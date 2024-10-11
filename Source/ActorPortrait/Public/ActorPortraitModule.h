// Mans Isaksson 2020

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogActorPortrait, Log, All);

class FActorPortraitModule : public IModuleInterface
{
private:
	bool bIsShuttingDown = false;
	bool bIsEndingPlay = false;

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static bool IsShuttingDown();
	static bool IsEndingPlay();

	void OnApplicationQuit();
	void OnPrePIEEnded(bool bIsSimulatingInEditor);
	void OnEndPlayMap();
};
