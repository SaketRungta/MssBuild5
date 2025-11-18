// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Subsystem/MssSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"

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
	OnlineSessionSettings->bIsDedicated = false;
	OnlineSessionSettings->bUsesPresence = true;
	OnlineSessionSettings->bAllowJoinInProgress = true;
	OnlineSessionSettings->NumPublicConnections = NumPublicConnections;
	OnlineSessionSettings->bAllowJoinViaPresence = true;
	OnlineSessionSettings->bShouldAdvertise = true;
	OnlineSessionSettings->bUseLobbiesIfAvailable = true;
	OnlineSessionSettings->Set(FName("MapName"), InCustomSessionSettings.MapName, EOnlineDataAdvertisementType::ViaOnlineService);
	OnlineSessionSettings->Set(FName("GameMode"), InCustomSessionSettings.GameMode, EOnlineDataAdvertisementType::ViaOnlineService);
	OnlineSessionSettings->Set(FName("Players"), InCustomSessionSettings.Players, EOnlineDataAdvertisementType::ViaOnlineService);
	OnlineSessionSettings->Set(FName("SessionCode"), GenerateSessionUniqueCode(), EOnlineDataAdvertisementType::ViaOnlineService);

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
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastCreatedSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastCreatedSessionSearch->MaxSearchResults = 10'000;
	LastCreatedSessionSearch->bIsLanQuery = false;
	LastCreatedSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	
	if (!SessionInterface->FindSessions(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), LastCreatedSessionSearch.ToSharedRef()))
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::FindSessions failed to execute find sessions"));
		
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
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

	if (const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession))
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

	if (SessionInterface)
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

	if (LastCreatedSessionSearch->SearchResults.IsEmpty())
	{
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("UMssSubsystem::OnFindSessionsCompleteCallback search result array is empty 0 active sessions found"));

		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	MultiplayerSessionsOnFindSessionsComplete.Broadcast(LastCreatedSessionSearch->SearchResults, bWasSuccessful);
}

void UMssSubsystem::OnJoinSessionCompleteCallback(FName SessionName,	EOnJoinSessionCompleteResult::Type Result)
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
}

#pragma endregion Session Operations On Completion Delegates Callbacks
