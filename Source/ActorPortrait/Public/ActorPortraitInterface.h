// Copyright Mans Isaksson. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActorPortraitSettings.h"
#include "ActorPortraitInterface.generated.h"

UINTERFACE(MinimalAPI)
class UActorPortraitInterface : public UInterface
{
	GENERATED_BODY()
};

class ACTORPORTRAIT_API IActorPortraitInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Actor Portrait")
	void OnUpdatePortraitScene(class UObject* UserData);

	UFUNCTION(BlueprintNativeEvent, Category = "Actor Portrait")
	void PreSpawnActorInPortraitScene();

	UFUNCTION(BlueprintNativeEvent, Category = "Actor Portrait")
	void PostSpawnActorInPortraitScene();

};
