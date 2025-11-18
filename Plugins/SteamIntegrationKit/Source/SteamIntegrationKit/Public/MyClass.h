// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MyClass.generated.h"


class UTexture2D;

USTRUCT()
struct FMyStruct
{
	GENERATED_BODY()
	
	/** Enable or disable Steam integration globally */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableSteam = true;

	/** Your Steam App ID (used for initializing Steam API) */
	UPROPERTY(EditAnywhere, Category = "sdfasdf")
	int32 SteamAppID = 480; // Default = Spacewar demo app ID

	/** Whether to use the Steam overlay */
	UPROPERTY(EditAnywhere, Category = "sdfgsdfg")
	bool bEnableOverlay = true;

	/** Logging verbosity for Steam integration */
	UPROPERTY(EditAnywhere, Category = "nbvmnbvm")
	bool bEnableVerboseLogging = false;

	UPROPERTY(EditAnywhere, Category = "34564536")
	TObjectPtr<UTexture2D> temp;
};


/**
 * 
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Steam Integration Kit"))
class STEAMINTEGRATIONKIT_API UMyClass : public UDeveloperSettings
{
	GENERATED_BODY()

	UMyClass();

	UPROPERTY(EditAnywhere, Category = "Debug")
	TArray<FMyStruct> MyStructs;
};
