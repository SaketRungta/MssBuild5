// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Widgets/MssHUD.h"

#include "OnlineSessionSettings.h"
#include "Widgets/MssSessionDataWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h" // For GEngine & on-screen debug

// ---------- Local Helper for On-Screen Debug ----------

static void ShowOnScreenDebugMessage(const FString& Message, const FColor& Color)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, Color, Message);
	}
}

// ---------- Logging Macros (Log + On-Screen Colors) ----------

#define HUD_LOG(Format, ...) \
	do { \
		const FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
		UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("%s"), *Msg); \
		ShowOnScreenDebugMessage(Msg, FColor::Green); \
	} while (0)

#define HUD_WARNING(Format, ...) \
	do { \
		const FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("%s"), *Msg); \
		ShowOnScreenDebugMessage(Msg, FColor::Yellow); \
	} while (0)

#define HUD_ERROR(Format, ...) \
	do { \
		const FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("%s"), *Msg); \
		ShowOnScreenDebugMessage(Msg, FColor::Red); \
	} while (0)

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
	HUD_LOG(TEXT("UMssHUD::OnSessionCreatedCallback Called"));
	
	if (!bWasSuccessful)
	{
		HUD_ERROR(TEXT("UMssHUD::OnSessionCreatedCallback is not successful"));
		ShowMessage(FString("Failed to Create Session"), true);
		return;
	}

	const FString TravelPath = LobbyMapPath + FString("?listen");
	HUD_LOG(TEXT("UMssHUD::OnSessionCreatedCallback server travel to path: %s"), *TravelPath);
		
	if (UWorld* World = GetWorld())
		World->ServerTravel(TravelPath);
}

void UMssHUD::OnSessionsFoundCallback(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	HUD_LOG(TEXT("UMssHUD::OnSessionsFoundCallback Called"));
	
	if (!MssSubsystem)
	{		
		HUD_ERROR(TEXT("UMssHUD::OnSessionsFoundCallback MultiplayerSessionsSubsystem is INVALID"));
		
		bJoinSessionViaCode = false;
		ShowMessage(FString("Unknown Error"), true);
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
		return;
	}
	
	if (!bWasSuccessful)
	{
		HUD_WARNING(TEXT("UMssHUD::OnSessionsFoundCallback is not successful"));

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
	HUD_LOG(TEXT("UMssHUD::OnSessionJoinedCallback Called"));

	if (EOnJoinSessionCompleteResult::Type::UnknownError == Result)
	{
		HUD_ERROR(TEXT("UMssHUD::OnSessionJoinedCallback UnknownError"));
		ShowMessage(FString("Unknown Error"), true);
		return;
	}
	
	if (const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
	{
		if (const IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface(); 
			SessionInterface.IsValid())
		{
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
				HUD_ERROR(TEXT("UMssHUD::OnSessionJoinedCallback Failed to resolve connect string for session"));
				ShowMessage(FString("Failed to Join Session"), true);
			}
		}
	}
}

void UMssHUD::OnSessionDestroyedCallback(bool bWasSuccessful)
{
	// You can add logs here too if you like:
	// HUD_LOG(TEXT("UMssHUD::OnSessionDestroyedCallback Called. Success: %d"), bWasSuccessful);
}

void UMssHUD::OnSessionStartedCallback(bool bWasSuccessful)
{
	// You can add logs here too:
	// HUD_LOG(TEXT("UMssHUD::OnSessionStartedCallback Called. Success: %d"), bWasSuccessful);
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
	
	MssSubsystem->FindSessions();
}

void UMssHUD::JoinSessionViaSessionCode(const TArray<FOnlineSessionSearchResult>& SessionSearchResults)
{
	HUD_LOG(TEXT("UMssHUD::JoinSessionViaSessionCode Called"));
	
	ShowMessage(FString("Joining Session"));
	
	for (FOnlineSessionSearchResult CurrentSessionSearchResult : SessionSearchResults)
	{
		FString CurrentSessionResultSessionCode = FString(""); 
		CurrentSessionSearchResult.Session.SessionSettings.Get(SETTING_SESSIONKEY, CurrentSessionResultSessionCode);
		if (CurrentSessionResultSessionCode == SessionCodeToJoin)
		{
			HUD_LOG(TEXT("UMssHUD::JoinSessionViaSessionCode Found session with code %s joining it"), *SessionCodeToJoin);
	
			MssSubsystem->JoinSessions(CurrentSessionSearchResult);
			return;
		}
	}
	
	ShowMessage(FString::Printf(TEXT("Wrong Session Code Entered: %s"), *SessionCodeToJoin), true);
	bJoinSessionViaCode = false;
}

void UMssHUD::UpdateSessionsList(const TArray<FOnlineSessionSearchResult>& Results)
{
	HUD_LOG(TEXT("UMssHUD::UpdateSessionsList called"));

	TSet<FString> NewSessionKeys;

	bool bAnySessionExists = false;
	
	// --- FIRST PASS: Add new sessions + update existing ones
	for (const FOnlineSessionSearchResult& Result : Results)
	{
		if (Result.Session.NumOpenPublicConnections <= 0)
			continue;

		FTempCustomSessionSettings CurrentSessionSettings;
		Result.Session.SessionSettings.Get(SETTING_MAPNAME, CurrentSessionSettings.MapName);
		Result.Session.SessionSettings.Get(SETTING_GAMEMODE, CurrentSessionSettings.GameMode);
		Result.Session.SessionSettings.Get(SETTING_NUMPLAYERSREQUIRED, CurrentSessionSettings.Players);

		const FString Key = Result.GetSessionIdStr();
		NewSessionKeys.Add(Key);

		// --- UPDATE EXISTING WIDGET ---
		if (UMssSessionDataWidget** ExistingWidgetPtr = ActiveSessionWidgets.Find(Key))
		{
			(*ExistingWidgetPtr)->SetSessionInfo(Result, CurrentSessionSettings);
			bAnySessionExists = true;
			continue; 
		}

		if (!SessionDataWidgetClass)
		{
			HUD_ERROR(TEXT("SessionDataWidgetClass is NULL!"));
			return;
		}

		// --- ADD A NEW WIDGET ---
		UMssSessionDataWidget* NewWidget = CreateWidget<UMssSessionDataWidget>(GetWorld(), SessionDataWidgetClass);
		NewWidget->SetSessionInfo(Result, CurrentSessionSettings);
		NewWidget->SetMssHUDRef(this);

		AddSessionDataWidget(NewWidget);

		ActiveSessionWidgets.Add(Key, NewWidget);

		bAnySessionExists = true;
		
		HUD_LOG(TEXT("Added NEW session widget: %s"), *Key);
	}	
	
	for (const FString& CurrentKey : LastSessionKeys)
	{
		if (NewSessionKeys.Contains(CurrentKey))
			continue;

		if (UMssSessionDataWidget** WidgetPtr = ActiveSessionWidgets.Find(CurrentKey))
		{
			if (UMssSessionDataWidget* CurrentSessionDataWidget = *WidgetPtr)
			{
				CurrentSessionDataWidget->RemoveFromParent();
			}
		}
	}
	
	LastSessionKeys = MoveTemp(NewSessionKeys);
	
	// If no session exists, according to the user's filter, then show the no active sessions text
	if (bAnySessionExists)
	{
		SetFindSessionsThrobberVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
	}
	
	if (bCanFindNewSessions)
	{
		HUD_LOG(TEXT("bCanFindNewSessions true calling to find new game"));
		
		if (!IsValid(this) || !GetWorld() || GetWorld()->bIsTearingDown)
		{
			HUD_LOG(TEXT("UpdateSessionsList aborted – world is tearing down"));
			return;
		}

		if (!GetWorld()->bIsTearingDown)
		{
			FindGame(FilterSessionSettings);
		}
	}
}

void UMssHUD::JoinTheGivenSession(FOnlineSessionSearchResult& InSessionToJoin)
{
	HUD_LOG(TEXT("UMssHUD::JoinTheGivenSession Called"));
	
	if (!MssSubsystem)
	{		
		HUD_ERROR(TEXT("UMssHUD::JoinTheGivenSession MultiplayerSessionsSubsystem is INVALID"));
		return;
	}
	
	if (!InSessionToJoin.IsValid())
	{
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

void UMssHUD::StartFindingSessions()
{
	HUD_LOG(TEXT("UMssHUD::StartFindingSessions Called"));
		
	bCanFindNewSessions = true;
	
	ActiveSessionWidgets.Empty();
	
	LastSessionKeys.Empty();
	
	SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
	
	FindGame(FilterSessionSettings);
}

void UMssHUD::StopFindingSessions()
{	
	HUD_LOG(TEXT("UMssHUD::StopFindingSessions Called"));
		
	bCanFindNewSessions = false;
	
	ActiveSessionWidgets.Empty();
	
	LastSessionKeys.Empty();
	
	SetFindSessionsThrobberVisibility(ESlateVisibility::Visible);
}
