// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Subsystem/MssSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

DEFINE_LOG_CATEGORY(MultiplayerSessionSubsystemLog);

UMssSubsystem::UMssSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionCompleteCallback)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsCompleteCallback)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionCompleteCallback)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionCompleteCallback)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionCompleteCallback))
{
	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::UMssSubsystem No Online Subsystem detected! Ensure a valid subsystem (e.g., Steam) is enabled in DefaultEngine.ini."));
		return;
	}

	SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::UMssSubsystem Online Subsystem does not support sessions! Check subsystem configuration."));
	}
	
	FCoreDelegates::OnPreExit.AddUObject(this, &UMssSubsystem::HandleAppExit);
}

void UMssSubsystem::Deinitialize()
{
	Super::Deinitialize();
	
	UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::Deinitialize called"));

	HandleAppExit();
}

void UMssSubsystem::HandleAppExit()
{	
	UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::HandleAppExit - Application exiting, destroying session"));

	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::HandleAppExit Active session detected during shutdown. Calling DestroySession()..."));
		DestroySession();
	}
}

#pragma region Session Operations

void UMssSubsystem::CreateSession(const FTempCustomSessionSettings& InCustomSessionSettings)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::CreateSession Called"));

	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::CreateSession SessionInterface is INVALID. Check Online Subsystem.")); 

		MultiplayerSessionsOnCreateSessionComplete.Broadcast(false);
		return;
	}
	
	/**
	 * If a session is already active and players requests to create a new session
	 * Then destroy the old session first and then call to create a new session
	 */
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{		
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::CreateSession NAME_GameSession already exists, destroying it before creating a new one"));
		
		bCreateSessionOnDestroy = true;

		/** Store the session creation data to use later when creating a new session after destroying the current session */
		SessionSettingsForTheSessionToCreateAfterDestruction = InCustomSessionSettings;
		
		DestroySession();
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

#pragma region Session Settings
	
	int32 NumPublicConnections = 2;
	if (InCustomSessionSettings.Players == "2v2")
		NumPublicConnections = 4;
	else if (InCustomSessionSettings.Players == "4v4")
		NumPublicConnections = 8;
	
	const TSharedPtr<FOnlineSessionSettings> OnlineSessionSettings = MakeShareable(new FOnlineSessionSettings());
	OnlineSessionSettings->bIsLANMatch = false;
	OnlineSessionSettings->NumPublicConnections = NumPublicConnections;
	OnlineSessionSettings->bAllowJoinInProgress = true;
	OnlineSessionSettings->bAllowJoinViaPresence = true;
	OnlineSessionSettings->bShouldAdvertise = true;
	OnlineSessionSettings->bUsesPresence = true;
	OnlineSessionSettings->bUseLobbiesIfAvailable = true;
	OnlineSessionSettings->Set(SETTING_FILTERSEED, SETTING_FILTERSEED_VALUE, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(SETTING_MAPNAME, InCustomSessionSettings.MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(SETTING_GAMEMODE, InCustomSessionSettings.GameMode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(SETTING_NUMPLAYERSREQUIRED, InCustomSessionSettings.Players, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(SETTING_SESSIONKEY, GenerateSessionUniqueCode(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

#pragma endregion Session Settings
	
	if (!SessionInterface->CreateSession(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), NAME_GameSession, *OnlineSessionSettings))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::CreateSession Failed to execute create session"));

		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		MultiplayerSessionsOnCreateSessionComplete.Broadcast(false);
	}
}

void UMssSubsystem::FindSessions()
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::FindSessions Called"));
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::FindSessions SessionInterface is INVALID. Check Online Subsystem."));

		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
	
	if (bFindSessionsInPProgress)
	{
		CancelFindSessions();
	}
	
	bFindSessionsInPProgress = true;
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	LastCreatedSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastCreatedSessionSearch->MaxSearchResults = 10000;
	LastCreatedSessionSearch->bIsLanQuery = false;;
	LastCreatedSessionSearch->QuerySettings.Set(SETTING_FILTERSEED, SETTING_FILTERSEED_VALUE, EOnlineComparisonOp::Equals);
	LastCreatedSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	if (!SessionInterface->FindSessions(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), LastCreatedSessionSearch.ToSharedRef()))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::FindSessions failed to execute find sessions"));
		
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		bFindSessionsInPProgress = false;
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMssSubsystem::CancelFindSessions()
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::CancelFindSessions Called"));

	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::CancelFindSessions SessionInterface is INVALID. Check Online Subsystem."));
		return;
	}

	bFindSessionsInPProgress = false;
	
	UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::CancelFindSessions - Aborting search."));

	// Clear delegate
	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

	// Broadcast an empty result so UI knows search ended
	MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
}

void UMssSubsystem::JoinSessions(FOnlineSessionSearchResult& InSessionToJoin)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::JoinSessions Called"));
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::JoinSessions SessionInterface is INVALID. Check Online Subsystem."));

		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	
	if (IsSessionInState(EOnlineSessionState::Creating) || 
		IsSessionInState(EOnlineSessionState::Starting) || 
		IsSessionInState(EOnlineSessionState::Ending))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("JoinSession blocked: session busy"));
		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	InSessionToJoin.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	InSessionToJoin.Session.SessionSettings.bUsesPresence = true;
	
	if (!SessionInterface->JoinSession(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), NAME_GameSession, InSessionToJoin))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::JoinSessions failed to execute join sessions"));
		
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMssSubsystem::DestroySession()
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::DestroySession Called"));
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::DestroySession SessionInterface is INVALID. Check Online Subsystem."));

		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
		return;
	}

	if (!IsSessionInState(EOnlineSessionState::Pending) &&
		!IsSessionInState(EOnlineSessionState::InProgress) &&
		!IsSessionInState(EOnlineSessionState::Ended))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("DestroySession failed: no session to destroy"));
		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::DestroySession failed to execute destroy sessions"));

		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
	}
}

void UMssSubsystem::StartSession()
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::StartSession Called"));

	if (!SessionInterface.IsValid())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::StartSession SessionInterface is INVALID"));
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
		return;
	}
	
	if (!IsSessionInState(EOnlineSessionState::Pending))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("StartSession called but session is NOT in Pending state"));
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
		return;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::StartSession Failed to execute StartSession"));

		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
	}
}

#pragma endregion Session Operations

FString UMssSubsystem::GenerateSessionUniqueCode() const
{
	/** Sums up the time when session creation is requested */
	const FDateTime CurrentTime = FDateTime::Now();
	const FString SumOfCurrentTime = FString::Printf(TEXT("%d%d%03d"), CurrentTime.GetMinute(), CurrentTime.GetSecond(), CurrentTime.GetMillisecond());

	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::GenerateSessionUniqueCode unique code for the currently created session '%s'"), *SumOfCurrentTime);

	return SumOfCurrentTime;
}

#pragma region Session Operations On Completion Delegates Callbacks
	
void UMssSubsystem::OnCreateSessionCompleteCallback(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnCreateSessionComplete created a session bWasSuccessful::%s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (SessionInterface)
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle); 

	if (const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession); bWasSuccessful)
	{
		FString SessionCode;
		Session->SessionSettings.Get(SETTING_SESSIONKEY, SessionCode);
		GEngine->AddOnScreenDebugMessage(-1, 6000, FColor::Green, *SessionCode);
	}
	
	MultiplayerSessionsOnCreateSessionComplete.Broadcast(bWasSuccessful);	
}

void UMssSubsystem::OnFindSessionsCompleteCallback(bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnFindSessionsCompleteCallback Completed finding sessions bWasSuccessful::%s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	bFindSessionsInPProgress = false;
	
	if (SessionInterface)
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

	if (!LastCreatedSessionSearch.IsValid() || LastCreatedSessionSearch->SearchResults.IsEmpty())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::OnFindSessionsCompleteCallback search result array is empty 0 active sessions found"));

		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), bWasSuccessful);
		return;
	}

	MultiplayerSessionsOnFindSessionsComplete.Broadcast(LastCreatedSessionSearch->SearchResults, bWasSuccessful);
}

void UMssSubsystem::OnJoinSessionCompleteCallback(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnJoinSessionComplete Completed joining session"));

	if (SessionInterface)
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

	MultiplayerSessionsOnJoinSessionsComplete.Broadcast(Result);
}

void UMssSubsystem::OnDestroySessionCompleteCallback(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnJoinSessionComplete Completed destroying session bWasSuccessful::%s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (SessionInterface.IsValid())
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);

	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(SessionSettingsForTheSessionToCreateAfterDestruction);
	}
	
	MultiplayerSessionsOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMssSubsystem::OnStartSessionCompleteCallback(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnStartSessionCompleteCallback SessionName: %s | Success: %s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (SessionInterface.IsValid())
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);

	MultiplayerSessionsOnStartSessionComplete.Broadcast(bWasSuccessful);

	if (!bWasSuccessful)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::OnStartSessionCompleteCallback StartSession failed."));
		return;
	}

    // optional: additional logic when StartSession succeeds
}

#pragma endregion Session Operations On Completion Delegates Callbacks

bool UMssSubsystem::IsSessionInState(EOnlineSessionState::Type State) const
{
	if (!SessionInterface.IsValid())
		return false;

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session)
		return false;

	return Session->SessionState == State;
}
