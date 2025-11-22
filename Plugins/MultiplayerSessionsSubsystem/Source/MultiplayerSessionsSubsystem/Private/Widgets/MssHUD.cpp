// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Widgets/MssHUD.h"

#include "OnlineSessionSettings.h"
#include "Widgets/MssSessionDataWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "System/MssLogger.h"
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
	
	if (!IsValid(GetMssSubsystem()))
	{
		LOG_ERROR(TEXT("Invalid GameInstance"));
		return true;
	}
	
	MssSubsystem->MultiplayerSessionsOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnSessionCreatedCallback);
	MssSubsystem->MultiplayerSessionsOnFindSessionsComplete.AddUObject(this, &ThisClass::OnSessionsFoundCallback);
	MssSubsystem->MultiplayerSessionsOnJoinSessionsComplete.AddUObject(this, &ThisClass::OnSessionJoinedCallback);
	MssSubsystem->MultiplayerSessionsOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnSessionDestroyedCallback);
	MssSubsystem->MultiplayerSessionsOnStartSessionComplete.AddDynamic(this, &ThisClass::OnSessionStartedCallback);
	
	return true;
}

void UMssHUD::HostGame(const FTempCustomSessionSettings& InSessionSettings)
{
	LOG_INFO(TEXT("Called"));
	
	ShowMessage(FString("Hosting Game"));

	if (GetMssSubsystem())
	{
		MssSubsystem->CreateSession(InSessionSettings);
	}
}

void UMssHUD::FindGame()
{
	LOG_INFO(TEXT("Called"));
	
	if (GetMssSubsystem())
	{
		MssSubsystem->FindSessions();
	}
}

void UMssHUD::EnterCode(const FText& InSessionCode)
{
	LOG_INFO(TEXT("Called session Code Entered : %s"), *InSessionCode.ToString());
	
	bJoinSessionViaCode = true;
	SessionCodeToJoin = InSessionCode.ToString();

	if (SessionCodeToJoin.Len() < 7)
	{
		ShowMessage(FString("Session code must be 7 digits long"), true);		
		return;
	}
	
	ShowMessage(FString("Joining Game"));
	
	if (GetMssSubsystem())
	{
		MssSubsystem->FindSessions();
	}
}

#pragma region Multiplayer Sessions Callbacks

void UMssHUD::OnSessionCreatedCallback(bool bWasSuccessful)
{
	LOG_INFO(TEXT("Session created : %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"));
	
	if (!bWasSuccessful)
	{
		ShowMessage(FString("Failed to Create Session"), true);
		return;
	}

	const FString TravelPath = LobbyMapPath + FString("?listen");
	LOG_INFO(TEXT("Server travel to path: %s"), *TravelPath);
		
	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(TravelPath);
	}
}

void UMssHUD::OnSessionsFoundCallback(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	LOG_INFO(TEXT("Session found : %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"));
	
	if (!GetMssSubsystem())
	{		
		LOG_ERROR(TEXT("UMssHUD::OnSessionsFoundCallback MultiplayerSessionsSubsystem is INVALID"));
		
		bJoinSessionViaCode = false;
		ShowMessage(FString("Unknown Error"), true);
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
		
		return;
	}
	
	if (!bWasSuccessful)
	{
		bJoinSessionViaCode = false;
		ShowMessage(FString("Failed to Find Session"), true);
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
		
		return;
	}

	if (bJoinSessionViaCode)
	{
		JoinSessionViaSessionCode(SessionResults);
	}
	else
	{
		UpdateSessionsList(SessionResults);
	}
}

void UMssHUD::OnSessionJoinedCallback(EOnJoinSessionCompleteResult::Type Result)
{
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		LOG_INFO(TEXT("Success"));
		break;
	case EOnJoinSessionCompleteResult::SessionIsFull:
		LOG_INFO(TEXT("SessionIsFull"));
		break;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		LOG_INFO(TEXT("SessionDoesNotExist"));
		break;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		LOG_INFO(TEXT("CouldNotRetrieveAddress"));
		break;
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		LOG_INFO(TEXT("AlreadyInSession"));
		break;
	case EOnJoinSessionCompleteResult::UnknownError:
		LOG_INFO(TEXT("UnknownError"));
		break;
	}

	if (Result != EOnJoinSessionCompleteResult::Type::Success)
	{
		ShowMessage(FString::Printf(TEXT("%s"), LexToString(Result)), true);
		bJoinSessionViaCode = false;
		
		return;
	}
	
	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		LOG_ERROR(TEXT("OnlineSubsystem is NULL"));
	}
	
	const IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		LOG_ERROR(TEXT("SessionInterface is INVALID"));
	}
	
	if (FString AddressOfSessionToJoin; 
			SessionInterface->GetResolvedConnectString(NAME_GameSession, AddressOfSessionToJoin))
	{
		if (APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController())
		{
			PlayerController->ClientTravel(AddressOfSessionToJoin, ETravelType::TRAVEL_Absolute);	
		}
	}
	else
	{
		LOG_ERROR(TEXT("Failed to find the address of the session to join"));
		
		ShowMessage(FString("Failed to Join Session"), true);
		bJoinSessionViaCode = false;
	}
}

void UMssHUD::OnSessionDestroyedCallback(bool bWasSuccessful)
{
}

void UMssHUD::OnSessionStartedCallback(bool bWasSuccessful)
{
}

#pragma endregion Multiplayer Sessions Callbacks

void UMssHUD::JoinSessionViaSessionCode(const TArray<FOnlineSessionSearchResult>& SessionSearchResults)
{
	LOG_INFO(TEXT("Called"));
	
	ShowMessage(FString("Joining Session"));
	
	for (FOnlineSessionSearchResult CurrentSessionSearchResult : SessionSearchResults)
	{
		FString CurrentSessionResultSessionCode = FString(""); 
		CurrentSessionSearchResult.Session.SessionSettings.Get(SETTING_SESSIONKEY, CurrentSessionResultSessionCode);
		
		if (CurrentSessionResultSessionCode != SessionCodeToJoin)
		{
			continue;	
		}
		
		LOG_INFO(TEXT("Found session with code %s joining it"), *SessionCodeToJoin);
	
		if (GetMssSubsystem())
		{
			MssSubsystem->JoinSessions(CurrentSessionSearchResult);
		}
			
		return;
	}
	
	LOG_INFO(TEXT("Wrong Session Code Entered: %s"), *SessionCodeToJoin);
	
	ShowMessage(FString::Printf(TEXT("Wrong Session Code Entered: %s"), *SessionCodeToJoin), true);
	
	bJoinSessionViaCode = false;
}

void UMssHUD::UpdateSessionsList(const TArray<FOnlineSessionSearchResult>& Results)
{
	LOG_INFO(TEXT("Called"));

	TSet<FString> NewSessionKeys;
	bool bAnySessionExists = false;

	// Filter settings
	const auto [MapName, GameMode, Players] = GetCurrentSessionsFilter();
	const bool bShowAllMap = MapName == "Any";
	const bool bShowAllGameMode = GameMode == "Any";
	const bool bShowAllPlayers = Players == "Any";

	// --- FIRST PASS: Add/update only filtered sessions ---
	for (const FOnlineSessionSearchResult& Result : Results)
	{
		if (Result.Session.NumOpenPublicConnections <= 0)
			continue;

		FTempCustomSessionSettings CurrentSessionSettings;
		Result.Session.SessionSettings.Get(SETTING_MAPNAME, CurrentSessionSettings.MapName);
		Result.Session.SessionSettings.Get(SETTING_GAMEMODE, CurrentSessionSettings.GameMode);
		Result.Session.SessionSettings.Get(SETTING_NUMPLAYERSREQUIRED, CurrentSessionSettings.Players);

		if (!bShowAllMap && CurrentSessionSettings.MapName != MapName)
			continue;
		
		if (!bShowAllGameMode && CurrentSessionSettings.GameMode != GameMode)
			continue;
		
		if (!bShowAllPlayers && CurrentSessionSettings.Players != Players)
			continue;
		
		// Session matches the filter → handle it
		const FString Key = Result.GetSessionIdStr();
		NewSessionKeys.Add(Key);

		// --- UPDATE EXISTING WIDGET ---
		if (UMssSessionDataWidget** ExistingWidgetPtr = ActiveSessionWidgets.Find(Key))
		{
			(*ExistingWidgetPtr)->SetSessionInfo(Result, CurrentSessionSettings);
			bAnySessionExists = true;
			continue;
		}

		// --- ADD NEW WIDGET ---
		if (!SessionDataWidgetClass)
		{
			LOG_ERROR(TEXT("SessionDataWidgetClass is NULL!"));
			return;
		}

		UMssSessionDataWidget* NewWidget = CreateWidget<UMssSessionDataWidget>(GetWorld(), SessionDataWidgetClass);
		NewWidget->SetSessionInfo(Result, CurrentSessionSettings);
		NewWidget->SetMssHUDRef(this);

		AddSessionDataWidget(NewWidget);
		ActiveSessionWidgets.Add(Key, NewWidget);

		bAnySessionExists = true;
	}

	// --- SECOND PASS: REMOVE widgets NOT in filtered set ---
	for (const FString& CurrentKey : LastSessionKeys)
	{
		if (NewSessionKeys.Contains(CurrentKey))
			continue;

		if (UMssSessionDataWidget** WidgetPtr = ActiveSessionWidgets.Find(CurrentKey))
		{
			if (UMssSessionDataWidget* Widget = *WidgetPtr)
				Widget->RemoveFromParent();
		}

		ActiveSessionWidgets.Remove(CurrentKey);
	}

	LastSessionKeys = MoveTemp(NewSessionKeys);

	// UI status messaging
	if (bAnySessionExists)
	{
		SetFindSessionsThrobberVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
	}

	// Auto-refresh logic
	if (bCanFindNewSessions)
	{
		if (!IsValid(this) || !GetWorld() || GetWorld()->bIsTearingDown)
		{
			LOG_INFO(TEXT("UpdateSessionsList aborted – world is tearing down"));
			return;
		}

		FindGame();
	}
}

void UMssHUD::JoinTheGivenSession(FOnlineSessionSearchResult& InSessionToJoin)
{
	LOG_INFO(TEXT("Called"));
	
	if (!InSessionToJoin.IsValid())
	{
		LOG_ERROR(TEXT("InSessionToJoin is NULL!"));
		return;
	}
	
	ShowMessage(FString("Joining Session"));

	if (GetMssSubsystem())
	{
	MssSubsystem->JoinSessions(InSessionToJoin);
	}
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

void UMssHUD::StartFindingSessions()
{
	LOG_INFO(TEXT("Called"));
		
	ClearSessionsScrollBox();
	
	bCanFindNewSessions = true;
	
	ActiveSessionWidgets.Empty();
	
	LastSessionKeys.Empty();
	
	SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
	
	FindGame();
}

void UMssHUD::StopFindingSessions()
{	
	LOG_INFO(TEXT("Called"));
		
	ClearSessionsScrollBox();
		
	bCanFindNewSessions = false;
	
	ActiveSessionWidgets.Empty();
	
	LastSessionKeys.Empty();
	
	SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
}

TObjectPtr<UMssSubsystem> UMssHUD::GetMssSubsystem()
{
	if (IsValid(MssSubsystem))
	{
		return MssSubsystem;
	}
	
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return MssSubsystem = GameInstance->GetSubsystem<UMssSubsystem>();
	}

	LOG_ERROR(TEXT("Cannot validate MssSubsystem"));
	
	return nullptr;
}
