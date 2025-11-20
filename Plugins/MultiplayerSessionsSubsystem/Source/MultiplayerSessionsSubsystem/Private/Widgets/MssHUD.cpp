// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Widgets/MssHUD.h"

#include "OnlineSessionSettings.h"
#include "Widgets/MssSessionDataWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

UMssHUD::UMssHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	{
		static ConstructorHelpers::FClassFinder<UUserWidget> Asset(TEXT("/MultiplayerSessionsSubsystem/Blueprints/Widgets/WBP_SessionData_Mss.WBP_SessionData_Mss_C"));
		if (Asset.Succeeded())
			SessionDataWidgetClass = Asset.Class;
	}
}

bool UMssHUD::Initialize()
{
	if (!Super::Initialize())
		return false;

	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	
	if (const UWorld* World = GetWorld())
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	
	if (const UGameInstance* GameInstance = GetGameInstance())
		MssSubsystem = GameInstance->GetSubsystem<UMssSubsystem>();

	if (MssSubsystem)
	{
		MssSubsystem->MultiplayerSessionsOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnSessionCreatedCallback);
		MssSubsystem->MultiplayerSessionsOnFindSessionsComplete.AddUObject(this, &ThisClass::OnSessionsFoundCallback);
		MssSubsystem->MultiplayerSessionsOnJoinSessionsComplete.AddUObject(this, &ThisClass::OnSessionJoinedCallback);
		MssSubsystem->MultiplayerSessionsOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnSessionDestroyedCallback);
		MssSubsystem->MultiplayerSessionsOnStartSessionComplete.AddDynamic(this, &ThisClass::OnSessionStartedCallback);
	}
	
	return true;
}

#pragma region Multiplayer Sessions Callbacks

void UMssHUD::OnSessionCreatedCallback(bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::OnSessionCreatedCallback Called"));
	
	if (!bWasSuccessful)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssHUD::OnSessionCreatedCallback is not successful"));
		ShowMessage(FString("Failed to Create Session"), true);
		return;
	}

	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::OnSessionCreatedCallback server travel to path: %s"), *(LobbyMapPath + FString("?listen")));
		
	if (UWorld* World = GetWorld())
		World->ServerTravel(LobbyMapPath + FString("?listen"));
}

void UMssHUD::OnSessionsFoundCallback(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::OnSessionsFoundCallback Called"));
	
	if (!MssSubsystem)
	{		
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssHUD::OnSessionsFoundCallback MultiplayerSessionsSubsystem is INVALID"));
		
		bJoinSessionViaCode = false;
		ShowMessage(FString("Unknown Error"), true);
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
		// Continue displaying the throbber
		return;
	}
	
	if (SessionResults.IsEmpty())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssHUD::OnSessionsFoundCallback no active sessions found"));

		bJoinSessionViaCode = false;
		// ShowMessage(FString("No Active Sessions Found"), true);
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
		return;
	}

	if (!bWasSuccessful)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssHUD::OnSessionsFoundCallback is not successful"));

		bJoinSessionViaCode = false;
		// ShowMessage(FString("Failed to Find Session"), true);
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
		return;
	}

	if (bJoinSessionViaCode)
	{
		JoinSessionViaSessionCode(SessionResults);
	}
	else
	{
		AddSessionSearchResultsToScrollBox(SessionResults);
	}
}

void UMssHUD::OnSessionJoinedCallback(EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::OnSessionJoinedCallback Called"));

	if (EOnJoinSessionCompleteResult::Type::UnknownError == Result)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssHUD::OnSessionJoinedCallback UnknownError"));
		ShowMessage(FString("Unknown Error"), true);
		return;
	}
	
	if (const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString AddressOfSessionToJoin;
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, AddressOfSessionToJoin))
			{
				if (APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController())
				{
					PlayerController->ClientTravel(AddressOfSessionToJoin, ETravelType::TRAVEL_Absolute);	
				}
			}
			else
			{
				UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssHUD::OnSessionJoinedCallback Failed to resolve connect string for session"));
				ShowMessage(FString("Failed to Join Session"), true);
			}
		}
	}
}

void UMssHUD::OnSessionDestroyedCallback(bool bWasSuccessful)
{
}

void UMssHUD::OnSessionStartedCallback(bool bWasSuccessful)
{
}

#pragma endregion Multiplayer Sessions Callbacks

void UMssHUD::HostGame(const FTempCustomSessionSettings& InSessionSettings)
{
	ShowMessage(FString("Hosting Game"));

	MssSubsystem->CreateSession(InSessionSettings);
}

void UMssHUD::FindGame(const FTempCustomSessionSettings& InSessionSettings)
{
	FilterSessionSettings = InSessionSettings;

	ClearSessionsScrollBox();

	SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
	
	MssSubsystem->FindSessions();
}

void UMssHUD::EnterCode(const FText& InSessionCode)
{
	bJoinSessionViaCode = true;
	SessionCodeToJoin = InSessionCode.ToString();

	if (SessionCodeToJoin.Len() < 7)
	{
		ShowMessage(FString("Session code must be 7 digits long"), true);		
		return;
	}
	
	ShowMessage(FString("Joining Game"));
	
	ClearSessionDataScrollBox();
	
	MssSubsystem->FindSessions();
}

void UMssHUD::JoinSessionViaSessionCode(const TArray<FOnlineSessionSearchResult>& SessionSearchResults)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::JoinSessionViaSessionCode Called"));
	
	ShowMessage(FString("Joining Session"));
	
	for (FOnlineSessionSearchResult CurrentSessionSearchResult : SessionSearchResults)
	{
		FString CurrentSessionResultSessionCode = FString(""); 
		CurrentSessionSearchResult.Session.SessionSettings.Get(FName("SessionCode"), CurrentSessionResultSessionCode);
		if (CurrentSessionResultSessionCode == SessionCodeToJoin)
		{
			UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::JoinSessionViaSessionCode Found session with code %s joining it"), *SessionCodeToJoin);
	
			MssSubsystem->JoinSessions(CurrentSessionSearchResult);
			return;
		}
	}
	
	ShowMessage(FString::Printf(TEXT("Wrong Session Code Entered: %s"), *SessionCodeToJoin), true);
	bJoinSessionViaCode = false;
}

void UMssHUD::AddSessionSearchResultsToScrollBox(const TArray<FOnlineSessionSearchResult>& SessionSearchResults)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox Called"));

	GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, FString::Printf(TEXT("Adding Session Search Results %d"), SessionSearchResults.Num()));
	
	FTempCustomSessionSettings SessionSettingsToLookFor;
	SessionSettingsToLookFor.MapName = FilterSessionSettings.MapName;
	SessionSettingsToLookFor.GameMode = FilterSessionSettings.GameMode;
	SessionSettingsToLookFor.Players = FilterSessionSettings.Players;

	// True if the user has selected the option as any 
	const bool bShowAllMapName = SessionSettingsToLookFor.MapName == "Any";
	const bool bShowAllGameMode = SessionSettingsToLookFor.GameMode == "Any";
	const bool bShowAllPlayers = SessionSettingsToLookFor.Players == "Any";

	// If all three options are selected as any then show all the search results
	if (bShowAllMapName && bShowAllGameMode && bShowAllPlayers)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox No filters selected, adding all sessions"));
		for (const FOnlineSessionSearchResult& CurrentSessionSearchResult : SessionSearchResults)
		{
			if (CurrentSessionSearchResult.Session.NumOpenPublicConnections <= 0)
			{
				continue;
			}

			FTempCustomSessionSettings CurrentSessionSettings;
			CurrentSessionSearchResult.Session.SessionSettings.Get(FName("MapName"), CurrentSessionSettings.MapName);
			CurrentSessionSearchResult.Session.SessionSettings.Get(FName("GameMode"), CurrentSessionSettings.GameMode);
			CurrentSessionSearchResult.Session.SessionSettings.Get(FName("Players"), CurrentSessionSettings.Players);

			UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox Adding session with MapName: %s, GameMode: %s, Players: %s"), *CurrentSessionSettings.MapName, *CurrentSessionSettings.GameMode, *CurrentSessionSettings.Players);

			if (!SessionDataWidgetClass->IsValidLowLevel())
			{
				UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox SessionDataWidgetClass is not valid. Please open WBP_HUD_Mss go to 'class defaults' and initialize SessionDataWidgetClass to WBP_SessionData_Mss"));
				return;
			}
			
			const TObjectPtr<UMssSessionDataWidget> CreatedSessionDataWidget = CreateWidget<UMssSessionDataWidget>(GetWorld(), SessionDataWidgetClass);
			CreatedSessionDataWidget->SetSessionInfo(CurrentSessionSearchResult, CurrentSessionSettings);
			CreatedSessionDataWidget->SetMssHUDRef(this);

			AddSessionDataWidget(CreatedSessionDataWidget);
		}
		return;
	}

	bool bAnySessionExists = false;
	
	for (const FOnlineSessionSearchResult& CurrentSessionSearchResult : SessionSearchResults)
	{
		if (CurrentSessionSearchResult.Session.NumOpenPublicConnections <= 0)
		{
			continue;
		}
		
		FTempCustomSessionSettings CurrentSessionSettings;
		CurrentSessionSearchResult.Session.SessionSettings.Get(FName("MapName"), CurrentSessionSettings.MapName);
		CurrentSessionSearchResult.Session.SessionSettings.Get(FName("GameMode"), CurrentSessionSettings.GameMode);
		CurrentSessionSearchResult.Session.SessionSettings.Get(FName("Players"), CurrentSessionSettings.Players);

		UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox Checking session with MapName: %s, GameMode: %s, Players: %s"), *CurrentSessionSettings.MapName, *CurrentSessionSettings.GameMode, *CurrentSessionSettings.Players);

		// If a particular map is selected, then check if this session has that map if false then continue to the next search result
		if (!bShowAllMapName && CurrentSessionSettings.MapName != SessionSettingsToLookFor.MapName)
			continue;
		
		// If a particular game mode is selected, then check if this session has that game mode if false then continue to the next search result
		if (!bShowAllGameMode && CurrentSessionSettings.GameMode != SessionSettingsToLookFor.GameMode)
			continue;
		
		// If a particular players count is selected, then check if this session has that players count if false then continue to the next search result
		if (!bShowAllPlayers && CurrentSessionSettings.Players != SessionSettingsToLookFor.Players)
			continue;
		
		UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox Adding session with matching settings."));

		if (!SessionDataWidgetClass->IsValidLowLevel())
		{
			UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox SessionDataWidgetClass is not valid. Please open WBP_HUD_Mss go to 'class defaults' and initialize SessionDataWidgetClass to WBP_SessionData_Mss"));
			return;
		}
			
		const TObjectPtr<UMssSessionDataWidget> CreatedSessionDataWidget = CreateWidget<UMssSessionDataWidget>(GetWorld(), SessionDataWidgetClass);
		CreatedSessionDataWidget->SetSessionInfo(CurrentSessionSearchResult, CurrentSessionSettings);
		CreatedSessionDataWidget->SetMssHUDRef(this);

		AddSessionDataWidget(CreatedSessionDataWidget);

		bAnySessionExists = true;
	}

	// If no session exists, according to the user's filter, then show the no active sessions text
	if (!bAnySessionExists)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::AddSessionSearchResultsToScrollBox FoundSessionScrollBox->GetChildrenCount() == 0"));

		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
	}
}

void UMssHUD::JoinTheGivenSession(FOnlineSessionSearchResult& InSessionToJoin)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssHUD::JoinTheGivenSession Called"));
	
	if (!MssSubsystem)
	{		
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssHUD::JoinTheGivenSession MultiplayerSessionsSubsystem is INVALID"));
		return;
	}
	
	ShowMessage(FString("Joining Session"));

	MssSubsystem->JoinSessions(InSessionToJoin);
}

FText UMssHUD::OnEnteredSessionCodeChanged(const FText& InCode)
{
	FString EnteredText = InCode.ToString();
	FString FilteredText;

	// Filter out non-numeric characters
	for (const TCHAR Character : EnteredText)
	{
		if (FChar::IsDigit(Character)) // Check if the character is a digit
		{
			FilteredText.AppendChar(Character);
		}
	}

	// Truncate if the length exceeds the limit
	if (FilteredText.Len() > 7)
	{
		FilteredText = FilteredText.Left(7);
	}

	// Update the text if it has changed
	if (FilteredText != EnteredText)
	{
		return FText::FromString(FilteredText);
	}

	return InCode;
}
