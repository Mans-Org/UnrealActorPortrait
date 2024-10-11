// Copyright Mans Isaksson. All Rights Reserved.

#include "ActorPortraitSettings.h"
#include "Engine/TextureRenderTarget2D.h"

FPortraitInputSettings::FPortraitInputSettings()
{
	OrbitSensitivity = 1.f;
	ZoomSpeed = 1000.f;

	OrbitXAxisMapping = 
	{
		FPortraitAxisMapping{ 1.f, EKeys::MouseX },
		FPortraitAxisMapping{ 1.f, EKeys::Gamepad_LeftX },
		FPortraitAxisMapping{ 1.f, EKeys::Gamepad_RightX }
	};
	
	OrbitYAxisMapping =
	{
		FPortraitAxisMapping{ -1.f, EKeys::MouseY },
		FPortraitAxisMapping{ -1.f, EKeys::Gamepad_LeftY },
		FPortraitAxisMapping{ -1.f, EKeys::Gamepad_RightY }
	};

	ZoomAxisMapping =
	{
		FPortraitAxisMapping{ -1.f, EKeys::MouseWheelAxis },
		FPortraitAxisMapping{ 1.f, EKeys::Gamepad_RightShoulder },
		FPortraitAxisMapping{ 1.f, EKeys::Gamepad_LeftShoulder }
	};
}

FPortraitCameraSettings FPortraitCameraSettings::MergePortraitCameraSettings(const FPortraitCameraSettings& DefaultSettings, const FPortraitCameraSettings& OverrideSettings)
{
	FPortraitCameraSettings OutSettings;

	struct FPropertyMemberAddr
	{
		FBoolProperty* OverrideBoolProperty;
		FProperty* Property;
	};
	static TArray<FPropertyMemberAddr, TInlineAllocator<32>> OverrideAndPropertyMemberValueAddr;
	static bool bAreValueAddrInitialized = false;
	static FCriticalSection CriticalSection;

	if (!bAreValueAddrInitialized)
	{
		FScopeLock Lock(&CriticalSection);
		if (!bAreValueAddrInitialized)
		{
			// Save property pointer locations on first merge for major performance improvements
			if (OverrideAndPropertyMemberValueAddr.Num() == 0)
			{
				TMap<FName, FProperty*> OverrideProperties;
				OverrideProperties.Reserve(64);
				for (FProperty* Property = FPortraitCameraSettings::StaticStruct()->PropertyLink; Property; Property = Property->PropertyLinkNext)
				{
					const FString PropertyName = Property->GetName();
					if (PropertyName.StartsWith("bOverride_"))
					{
						OverrideProperties.Add(*PropertyName, Property);
					}
					else if (FProperty** OverrideProperty = OverrideProperties.Find(*FString::Printf(TEXT("bOverride_%s"), *PropertyName)))
					{
						OverrideAndPropertyMemberValueAddr.Add(FPropertyMemberAddr { CastFieldChecked<FBoolProperty>(*OverrideProperty), Property });
					}
				}
			}

			bAreValueAddrInitialized = true;
		}
	}

	for (const FPropertyMemberAddr& It : OverrideAndPropertyMemberValueAddr)
	{
		if (It.OverrideBoolProperty->GetPropertyValue(It.OverrideBoolProperty->ContainerPtrToValuePtr<void>(&OverrideSettings))) // Is bOverride_ set in OverrideSettings
		{
			It.OverrideBoolProperty->SetPropertyValue(It.OverrideBoolProperty->ContainerPtrToValuePtr<void>((void*)&OutSettings), true);
			It.Property->CopyCompleteValue(
				It.Property->ContainerPtrToValuePtr<void>((void*)&OutSettings), 
				It.Property->ContainerPtrToValuePtr<void>(&OverrideSettings)
			);
		}
		else if (It.OverrideBoolProperty->GetPropertyValue(It.OverrideBoolProperty->ContainerPtrToValuePtr<void>(&DefaultSettings))) // Is bOverride_ set in DefaultSettings
		{
			It.OverrideBoolProperty->SetPropertyValue(It.OverrideBoolProperty->ContainerPtrToValuePtr<void>((void*)&OutSettings), true);
			It.Property->CopyCompleteValue(
				It.Property->ContainerPtrToValuePtr<void>((void*)&OutSettings), 
				It.Property->ContainerPtrToValuePtr<void>(&DefaultSettings)
			);
		}
	}
	
	return OutSettings;
}

UPortraitEnvironmentSettings::UPortraitEnvironmentSettings()
{
	EnvironmentColor = FLinearColor(1.f, 1.f, 1.f);
	EnvironmentCubeMap = TSoftObjectPtr<UTextureCube>(FSoftObjectPath("/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube"));
	EnvironmentRotation = 0.f;
}

const FPortraitCameraSettings& FPortraitCameraSettings::DefaultCameraSettings()
{
	static FPortraitCameraSettings DefaultSettings;
	return DefaultSettings;
}