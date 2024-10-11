// Mans Isaksson 2020

#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "Camera/CameraTypes.h"
#include "InputCoreTypes.h"

#include "ActorPortraitSettings.generated.h"

USTRUCT(BlueprintType)
struct FPortraitAxisMapping
{
	GENERATED_BODY()
public:
	/** Multiplier to use for the mapping when accumulating the axis value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	float Scale;

	/** Key to bind it to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	FKey Key;
};

USTRUCT(BlueprintType)
struct FPortraitInputSettings
{
	GENERATED_BODY()
public:
	FPortraitInputSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	float OrbitSensitivity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	float ZoomSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TArray<FPortraitAxisMapping> OrbitXAxisMapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TArray<FPortraitAxisMapping> OrbitYAxisMapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TArray<FPortraitAxisMapping> ZoomAxisMapping;
};

UENUM(BlueprintType)
enum class EPortraitCameraFitMode : uint8
{
	Fill   UMETA(DisplayName="Fill"),
	Fit    UMETA(DisplayName="Fit"),
	FitX   UMETA(DisplayName="Fit X"),
	FitY   UMETA(DisplayName="Fit Y"),
};

USTRUCT(BlueprintType, meta=(HiddenByDefault))
struct FPortraitCameraSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_bResetCameraOnViewportResize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_ProjectionType:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraFOV:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraOrbitOriginOffset:1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraOrbitOriginOverride:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_MinZoomDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_MaxZoomDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraOrbitRotation:1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraFitMode:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraDistanceOffset:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraDistanceOverride:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_OrthoWidthOffset:1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_OrthoWidthOverride:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_ComponentBoundsBlacklist:1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_bIncludeHiddenComponentsInBounds:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CustomActorBounds:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraPositionOffset:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CameraRotationOffset:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CustomCameraLocation:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CustomCameraRotation:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CustomOrthoWidth:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_bDrawDebug:1;

	FPortraitCameraSettings()
		: bOverride_bResetCameraOnViewportResize(0)
		, bOverride_ProjectionType(0)
		, bOverride_CameraFOV(0)
		, bOverride_CameraOrbitOriginOffset(0)
		, bOverride_CameraOrbitOriginOverride(0)
		, bOverride_MinZoomDistance(0)
		, bOverride_MaxZoomDistance(0)
		, bOverride_CameraOrbitRotation(0)
		, bOverride_CameraFitMode(0)
		, bOverride_CameraDistanceOffset(0)
		, bOverride_CameraDistanceOverride(0)
		, bOverride_OrthoWidthOffset(0)
		, bOverride_OrthoWidthOverride(0)
		, bOverride_ComponentBoundsBlacklist(0)
		, bOverride_bIncludeHiddenComponentsInBounds(0)
		, bOverride_CustomActorBounds(0)
		, bOverride_CameraPositionOffset(0)
		, bOverride_CameraRotationOffset(0)
		, bOverride_CustomCameraLocation(0)
		, bOverride_CustomCameraRotation(0)
		, bOverride_CustomOrthoWidth(0)
		, bOverride_bDrawDebug(0)
	{
	}

	// Whether to reset the camera using the camera settins on viewport resize.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_bResetCameraOnViewportResize"))
	bool bResetCameraOnViewportResize = false;

	// Type of camera projection to use for this portrait (Perspective/Orthographic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_ProjectionType"))
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionType = ECameraProjectionMode::Perspective;

	// Camera field of view (in degrees). (Ignored in Orthographic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_CameraFOV", UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0"))
	float CameraFOV = 45.f;

	// Offset the auto-calculated location the camera will orbit around.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_CameraOrbitOriginOffset"))
	FVector CameraOrbitOriginOffset = FVector::ZeroVector;

	// Override the auto-calculated location the camera will orbit around.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_CameraOrbitOriginOverride"))
	FVector CameraOrbitOriginOverride = FVector::ZeroVector;

	// The maximum distance from the origin point the user can zoom in
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_MinZoomDistance", UIMin = "0.01", ClampMin = "0.0001"))
	float MinZoomDistance = 0.1f;

	// The maximum distance from the origin point the user can zoom out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings", meta=(EditCondition = "bOverride_MaxZoomDistance", UIMin = "0.01", ClampMin = "0.0001"))
	float MaxZoomDistance = 500.f;

	// Amount (in degrees) the camera will orbit around the portrait actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CameraOrbitRotation"))
	FRotator CameraOrbitRotation = FRotator::ZeroRotator;

	// How the automatic framing will best try and fit the actor in frame
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CameraFitMode"))
	EPortraitCameraFitMode CameraFitMode = EPortraitCameraFitMode::Fit;

	// Distance offset (in cm) from the automatically calculated distance. (Ignored in Orthographic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CameraDistanceOffset"))
	float CameraDistanceOffset = -20.f;

	// Override the automatically calculated distance (in cm). (Ignored in Orthographic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CameraDistanceOverride"))
	float CameraDistanceOverride = 0.f;

	// Offset from the auto-calculated OrthoWidth. (Ingored in Perspective)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_OrthoWidthOffset"))
	float OrthoWidthOffset = 0.f;

	// Override the auto-calculated OrthoWidth. (Ignored in Perspective)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_OrthoWidthOverride"))
	float OrthoWidthOverride = 0.f;

	// Components of this type will be ignored when calculating the actor bounding box for framing (Ignored when using custom camera transform)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_ComponentBoundsBlacklist"))
	TSet<UClass*> ComponentBoundsBlacklist;

	// Whether to include hidden components when calculating the actor bounding box for framing (Ignored when using custom camera transform)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_bIncludeHiddenComponentsInBounds"))
	bool bIncludeHiddenComponentsInBounds = false;

	// Custom bounds that can be used instead of pulling the bounds from the Actor. Useful for actors which does not have bounds of their own such as particle effects.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CustomActorBounds"))
	FBox CustomActorBounds = FBox(EForceInit::ForceInit);

	// Location offset in camera space from the automatically calculated position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CameraPositionOffset"))
	FVector CameraPositionOffset = FVector::ZeroVector;

	// Rotation offset in camera space from the automatically calculated rotation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Auto Frame", meta=(EditCondition = "bOverride_CameraRotationOffset"))
	FRotator CameraRotationOffset = FRotator::ZeroRotator;

	// Directly set the location of the camera. Enabling this will disable auto framing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Custom", meta=(EditCondition = "bOverride_CustomCameraLocation"))
	FVector CustomCameraLocation = FVector::ZeroVector;

	// Directly set the rotation of the camera. Enabling this will disable auto framing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Custom", meta=(EditCondition = "bOverride_CustomCameraRotation"))
	FRotator CustomCameraRotation = FRotator::ZeroRotator;

	// Directly set the OrthoWidth. Enabling this will disable auto framing. (Ingored in Perspective)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Custom", meta=(EditCondition = "bOverride_CustomOrthoWidth"))
	float CustomOrthoWidth = 0.f;

	// Whether to draw debug information regarding the auto-framing such as bounds and camera orbit origin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Camera Settings|Advanced", meta=(EditCondition = "bOverride_bDrawDebug"))
	bool bDrawDebug = false;

	static FPortraitCameraSettings MergePortraitCameraSettings(const FPortraitCameraSettings& DefaultSettings, const FPortraitCameraSettings& OverrideSettings);

	static const FPortraitCameraSettings& DefaultCameraSettings();
};

UCLASS(BlueprintType, Blueprintable)
class ACTORPORTRAIT_API UPortraitEnvironmentSettings : public UObject
{
	GENERATED_BODY()
public:
	UPortraitEnvironmentSettings();

	// The color of the portrait scene environment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Environment")
	FLinearColor EnvironmentColor;

	// The cube map to use for the portrait scene environment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Environment")
	TSoftObjectPtr<UTextureCube> EnvironmentCubeMap;

	// The rotation (Yaw) of the portrait scene environment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait|Environment")
	float EnvironmentRotation;
};
