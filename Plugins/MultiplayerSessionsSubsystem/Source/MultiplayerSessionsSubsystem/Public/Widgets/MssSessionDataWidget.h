// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OnlineSessionSettings.h"
#include "MssSessionDataWidget.generated.h"

struct FTempCustomSessionSettings;
class UTextBlock;
class UButton;
class UMssHUD;

/**
 * Class to show the session data in the scroll box
 * Stores and displays all the session related info in the scroll box
 ******************************************************************************************/
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Widgets))
class MULTIPLAYERSESSIONSSUBSYSTEM_API UMssSessionDataWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/** Initializes all the widgets */
	virtual bool Initialize() override;
	
private:
	/** Text to show the map name */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MapName;

	/** Text to show the players count */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Players;

	/** Text to show the game mode */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GameMode;

	/** Button to let user join this session */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> JoinSessionButton;

	/** Ref to the main menu widget set via setter, for when user joins this session we can call the main menu widget to join this session */
	TWeakObjectPtr<UMssHUD> MssHUDRef;
	
	/** Stores the session search result for this widget */
	FOnlineSessionSearchResult SessionSearchResultRef;

	/** Join button clicked callback, calls the main menu widget to join this session */
	UFUNCTION()
	void OnJoinSessionButtonClicked();
	
public:
	/** Called from UMssHUD::AddSessionSearchResultsToScrollBox upon adding this widget to the scroll box to fill it with necessary information */
	void SetSessionInfo(const FOnlineSessionSearchResult& InSessionSearchResultRef, 
		const FTempCustomSessionSettings& SessionSettings);

	/** Called from UMssHUD::AddSessionSearchResultsToScrollBox upon adding this widget to the scroll box to set the ref to main menu widget */
	void SetMssHUDRef(UMssHUD* InMssHUD);
	
};
