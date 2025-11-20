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

	bCreateSessionInProgress = true;
	
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
	OnlineSessionSettings->Set(FName("RANDOMSEED"), FString("afs65d4"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(FName("MapName"), InCustomSessionSettings.MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(FName("GameMode"), InCustomSessionSettings.GameMode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(FName("Players"), InCustomSessionSettings.Players, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	OnlineSessionSettings->Set(FName("SessionCode"), GenerateSessionUniqueCode(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
#pragma endregion Session Settings
	
	if (!SessionInterface->CreateSession(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), NAME_GameSession, *OnlineSessionSettings))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::CreateSession Failed to execute create session"));

		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		bCreateSessionInProgress = false;
		MultiplayerSessionsOnCreateSessionComplete.Broadcast(false);
		return;
	}
	
	// === Start timeout timer ===
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CreateSessionTimeoutHandle,
			this,
			&UMssSubsystem::HandleCreateSessionTimeout,
			CreateSessionTimeoutSeconds,
			false
		);
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
	
	bFindSessionsInProgress = true;
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	LastCreatedSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastCreatedSessionSearch->MaxSearchResults = 10000;
	LastCreatedSessionSearch->bIsLanQuery = false;;
	// LastCreatedSessionSearch->QuerySettings.Set(FName("RANDOMSEED"), FString("afs65d4"), EOnlineComparisonOp::Equals);
	LastCreatedSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	if (!SessionInterface->FindSessions(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), LastCreatedSessionSearch.ToSharedRef()))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::FindSessions failed to execute find sessions"));
		
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		bFindSessionsInProgress = false;
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FindSessionsTimeoutHandle,
			this,
			&UMssSubsystem::HandleFindSessionsTimeout,
			FindSessionsTimeoutSeconds,
			false
		);
	}
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

	bJoinSessionInProgress = true;
    
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	InSessionToJoin.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	InSessionToJoin.Session.SessionSettings.bUsesPresence = true;
	
	if (!SessionInterface->JoinSession(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), NAME_GameSession, InSessionToJoin))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::JoinSessions failed to execute join sessions"));
		
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		bJoinSessionInProgress = false;
		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			JoinSessionTimeoutHandle,
			this,
			&UMssSubsystem::HandleJoinSessionTimeout,
			JoinSessionTimeoutSeconds,
			false
		);
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

	bDestroySessionInProgress = true;

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::DestroySession failed to execute destroy sessions"));

		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		bDestroySessionInProgress = false;
		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
		return;
	}
	
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DestroySessionTimeoutHandle,
			this,
			&UMssSubsystem::HandleDestroySessionTimeout,
			DestroySessionTimeoutSeconds,
			false
		);
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

	bStartSessionInProgress = true;

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::StartSession Failed to execute StartSession"));

		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		bStartSessionInProgress = false;
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StartSessionTimeoutHandle,
			this,
			&UMssSubsystem::HandleStartSessionTimeout,
			StartSessionTimeoutSeconds,
			false
		);
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

	if (!bCreateSessionInProgress)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::OnCreateSessionComplete received late callback after timeout. Ignoring."));
		return;
	}

	bCreateSessionInProgress = false;

	if (const UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(CreateSessionTimeoutHandle);

	if (SessionInterface)
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle); 

	if (const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession); bWasSuccessful)
	{
		FString SessionCode;
		Session->SessionSettings.Get(FName("SessionCode"), SessionCode);
		GEngine->AddOnScreenDebugMessage(-1, 6000, FColor::Green, *SessionCode);
	}
	
	MultiplayerSessionsOnCreateSessionComplete.Broadcast(true);	
}

void UMssSubsystem::OnFindSessionsCompleteCallback(bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnFindSessionsCompleteCallback Completed finding sessions bWasSuccessful::%s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (!bFindSessionsInProgress)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::OnFindSessionsCompleteCallback late callback after timeout. Ignoring."));
		return;
	}

	bFindSessionsInProgress = false;

	if (const UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(FindSessionsTimeoutHandle);

	if (SessionInterface)
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

	if (!LastCreatedSessionSearch.IsValid() || LastCreatedSessionSearch->SearchResults.IsEmpty())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::OnFindSessionsCompleteCallback search result array is empty 0 active sessions found"));

		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	MultiplayerSessionsOnFindSessionsComplete.Broadcast(LastCreatedSessionSearch->SearchResults, bWasSuccessful);
}

void UMssSubsystem::OnJoinSessionCompleteCallback(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnJoinSessionComplete Completed joining session"));

	if (!bJoinSessionInProgress)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::OnJoinSessionComplete late callback after timeout. Ignoring."));
		return;
	}

	bJoinSessionInProgress = false;

	if (const UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(JoinSessionTimeoutHandle);

	if (SessionInterface)
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

	if ((Result == EOnJoinSessionCompleteResult::UnknownError || 
		Result == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress || 
		Result == EOnJoinSessionCompleteResult::SessionIsFull) && 
		JoinRetryCounter < MaxJoinRetries)
	{
		JoinRetryCounter += 1;

		UE_LOG(MultiplayerSessionSubsystemLog, Warning,
			TEXT("JoinSession failed (%d). Retrying... (%d/%d)"),
			(int32)Result,
			JoinRetryCounter,
			MaxJoinRetries
		);

		// Retry after slight delay
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([this]()
			{
				UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("Retrying JoinSession..."));
				this->JoinSessions(this->LastCreatedSessionSearch->SearchResults[0]);
			});
		}

		return;
	}
	
	MultiplayerSessionsOnJoinSessionsComplete.Broadcast(Result);
}

void UMssSubsystem::OnDestroySessionCompleteCallback(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("UMssSubsystem::OnJoinSessionComplete Completed destroying session bWasSuccessful::%s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (!bDestroySessionInProgress)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::OnDestroySessionComplete late callback after timeout. Ignoring."));
		return;
	}

	bDestroySessionInProgress = false;

	if (const UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(DestroySessionTimeoutHandle);

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

	if (!bStartSessionInProgress)
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("UMssSubsystem::OnStartSessionCompleteCallback late callback after timeout. Ignoring."));
		return;
	}

	bStartSessionInProgress = false;

	if (const UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(StartSessionTimeoutHandle);

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

#pragma region Timeout Handling
	
void UMssSubsystem::HandleCreateSessionTimeout()
{
    if (!bCreateSessionInProgress)
        return;

    UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::HandleCreateSessionTimeout CreateSession timed out."));

    bCreateSessionInProgress = false;

    if (SessionInterface.IsValid())
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

    MultiplayerSessionsOnCreateSessionComplete.Broadcast(false);
}

void UMssSubsystem::HandleFindSessionsTimeout()
{
    if (!bFindSessionsInProgress)
        return;

    UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::HandleFindSessionsTimeout FindSessions timed out."));

    bFindSessionsInProgress = false;

    if (SessionInterface.IsValid())
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

    MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
}

void UMssSubsystem::HandleJoinSessionTimeout()
{
    if (!bJoinSessionInProgress)
        return;

    UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::HandleJoinSessionTimeout JoinSession timed out."));

    bJoinSessionInProgress = false;

    if (SessionInterface.IsValid())
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

    MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
}

void UMssSubsystem::HandleDestroySessionTimeout()
{
    if (!bDestroySessionInProgress)
        return;

    UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::HandleDestroySessionTimeout DestroySession timed out."));

    bDestroySessionInProgress = false;

    if (SessionInterface.IsValid())
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);

    MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
}

void UMssSubsystem::HandleStartSessionTimeout()
{
    if (!bStartSessionInProgress)
        return;

    UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::HandleStartSessionTimeout StartSession timed out."));

    bStartSessionInProgress = false;

    if (SessionInterface.IsValid())
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);

    MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
}

#pragma endregion Timeout Handling

bool UMssSubsystem::IsSessionInState(EOnlineSessionState::Type State) const
{
	if (!SessionInterface.IsValid())
		return false;

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session)
		return false;

	return Session->SessionState == State;
}
