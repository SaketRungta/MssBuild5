// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "Subsystem/MssSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

DEFINE_LOG_CATEGORY(MultiplayerSessionSubsystemLog);

// ---------- Local Helper for On-Screen Debug ----------

static void ShowSubsystemOnScreenMessage(const FString& Message, const FColor& Color)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			3.f,
			Color,
			FString("[SUBSYSTEM] ") + Message
		);
	}
}

// ---------- Logging Macros (Log + On-Screen Colors) ----------

#define SUBSYS_LOG(Format, ...) \
	do { \
		const FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
		UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("%s"), *Msg); \
		ShowSubsystemOnScreenMessage(Msg, FColor::Green); \
	} while (0)

#define SUBSYS_WARNING(Format, ...) \
	do { \
		const FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
		UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("%s"), *Msg); \
		ShowSubsystemOnScreenMessage(Msg, FColor::Yellow); \
	} while (0)

#define SUBSYS_ERROR(Format, ...) \
	do { \
		const FString Msg = FString::Printf(Format, ##__VA_ARGS__); \
		UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("%s"), *Msg); \
		ShowSubsystemOnScreenMessage(Msg, FColor::Red); \
	} while (0)

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
		SUBSYS_ERROR(TEXT("UMssSubsystem::UMssSubsystem No Online Subsystem detected! Ensure a valid subsystem is enabled."));
		return;
	}

	SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("UMssSubsystem::UMssSubsystem Online Subsystem does not support sessions!"));
	}

	FCoreDelegates::OnPreExit.AddUObject(this, &UMssSubsystem::HandleAppExit);
}

void UMssSubsystem::Deinitialize()
{
	Super::Deinitialize();
	
	SUBSYS_WARNING(TEXT("UMssSubsystem::Deinitialize called"));

	HandleAppExit();
}

void UMssSubsystem::HandleAppExit()
{	
	SUBSYS_WARNING(TEXT("UMssSubsystem::HandleAppExit - Application exiting, destroying session"));

	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SUBSYS_WARNING(TEXT("UMssSubsystem::HandleAppExit Active session detected during shutdown. Destroying..."));
		DestroySession();
	}
	
	if (bFindSessionsInProgress)
	{
		CancelFindSessions();
	}
}

#pragma region Session Operations

void UMssSubsystem::CreateSession(const FTempCustomSessionSettings& InCustomSessionSettings)
{
	SUBSYS_LOG(TEXT("UMssSubsystem::CreateSession Called"));

	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("CreateSession SessionInterface is INVALID"));
		MultiplayerSessionsOnCreateSessionComplete.Broadcast(false);
		return;
	}
	
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{		
		SUBSYS_ERROR(TEXT("NAME_GameSession already exists, destroying before creating a new one"));
		
		bCreateSessionOnDestroy = true;
		SessionSettingsForTheSessionToCreateAfterDestruction = InCustomSessionSettings;
		
		DestroySession();
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	int32 NumPublicConnections = 2;
	if (InCustomSessionSettings.Players == "2v2") NumPublicConnections = 4;
	else if (InCustomSessionSettings.Players == "4v4") NumPublicConnections = 8;
	
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

	if (!SessionInterface->CreateSession(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), NAME_GameSession, *OnlineSessionSettings))
	{
		SUBSYS_ERROR(TEXT("CreateSession failed to execute create session"));

		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		MultiplayerSessionsOnCreateSessionComplete.Broadcast(false);
	}
}

void UMssSubsystem::FindSessions()
{
	SUBSYS_LOG(TEXT("UMssSubsystem::FindSessions Called"));
	
	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("FindSessions SessionInterface is INVALID"));
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
	
	if (bFindSessionsInProgress)
	{
		CancelFindSessions();
	}
	
	bFindSessionsInProgress = true;
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	LastCreatedSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastCreatedSessionSearch->MaxSearchResults = 10000;
	LastCreatedSessionSearch->bIsLanQuery = false;
	LastCreatedSessionSearch->QuerySettings.Set(SETTING_FILTERSEED, SETTING_FILTERSEED_VALUE, EOnlineComparisonOp::Equals);
	LastCreatedSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	if (!GetWorld() || GetWorld()->bIsTearingDown)
	{
		SUBSYS_WARNING(TEXT("FindSessions aborted – world is tearing down"));
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	if (!SessionInterface->FindSessions(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), LastCreatedSessionSearch.ToSharedRef()))
	{
		SUBSYS_ERROR(TEXT("FindSessions failed to execute find sessions"));
		
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		bFindSessionsInProgress = false;
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMssSubsystem::CancelFindSessions()
{
	SUBSYS_LOG(TEXT("UMssSubsystem::CancelFindSessions Called"));

	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("CancelFindSessions SessionInterface is INVALID"));
		return;
	}

	bFindSessionsInProgress = false;
	
	SUBSYS_WARNING(TEXT("CancelFindSessions - Aborting search."));

	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), true);
}

void UMssSubsystem::JoinSessions(FOnlineSessionSearchResult& InSessionToJoin)
{
	SUBSYS_LOG(TEXT("UMssSubsystem::JoinSessions Called"));
	
	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("JoinSessions SessionInterface is INVALID"));
		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	
	if (IsSessionInState(EOnlineSessionState::Creating) ||
		IsSessionInState(EOnlineSessionState::Starting) ||
		IsSessionInState(EOnlineSessionState::Ending))
	{
		SUBSYS_ERROR(TEXT("JoinSession blocked: session busy"));
		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	InSessionToJoin.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	InSessionToJoin.Session.SessionSettings.bUsesPresence = true;
	
	if (!SessionInterface->JoinSession(*GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId(), NAME_GameSession, InSessionToJoin))
	{
		SUBSYS_ERROR(TEXT("JoinSessions failed to execute join sessions"));
		
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerSessionsOnJoinSessionsComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMssSubsystem::DestroySession()
{
	SUBSYS_LOG(TEXT("UMssSubsystem::DestroySession Called"));
	
	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("DestroySession SessionInterface is INVALID"));
		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
		return;
	}

	if (!IsSessionInState(EOnlineSessionState::Pending) &&
		!IsSessionInState(EOnlineSessionState::InProgress) &&
		!IsSessionInState(EOnlineSessionState::Ended))
	{
		SUBSYS_ERROR(TEXT("DestroySession failed: no session to destroy"));
		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SUBSYS_ERROR(TEXT("DestroySession failed to execute destroy sessions"));

		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerSessionsOnDestroySessionComplete.Broadcast(false);
	}
}

void UMssSubsystem::StartSession()
{
	SUBSYS_LOG(TEXT("UMssSubsystem::StartSession Called"));

	if (!SessionInterface.IsValid())
	{
		SUBSYS_ERROR(TEXT("StartSession SessionInterface is INVALID"));
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
		return;
	}
	
	if (!IsSessionInState(EOnlineSessionState::Pending))
	{
		SUBSYS_ERROR(TEXT("StartSession called but session is NOT in Pending state"));
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
		return;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SUBSYS_ERROR(TEXT("StartSession failed to execute StartSession"));

		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		MultiplayerSessionsOnStartSessionComplete.Broadcast(false);
	}
}

#pragma endregion Session Operations

FString UMssSubsystem::GenerateSessionUniqueCode() const
{
	const FDateTime CurrentTime = FDateTime::Now();
	const FString SumOfCurrentTime = FString::Printf(TEXT("%d%d%03d"), CurrentTime.GetMinute(), CurrentTime.GetSecond(), CurrentTime.GetMillisecond());

	SUBSYS_LOG(TEXT("GenerateSessionUniqueCode generated code '%s'"), *SumOfCurrentTime);

	return SumOfCurrentTime;
}

#pragma region Session Operations On Completion Delegates Callbacks
	
void UMssSubsystem::OnCreateSessionCompleteCallback(FName SessionName, bool bWasSuccessful)
{
	SUBSYS_LOG(TEXT("OnCreateSessionComplete created session: %s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

	if (SessionInterface)
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle); 

	if (const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession); bWasSuccessful)
	{
		FString SessionCode;
		Session->SessionSettings.Get(SETTING_SESSIONKEY, SessionCode);
		ShowSubsystemOnScreenMessage(SessionCode, FColor::Green);
	}
	
	MultiplayerSessionsOnCreateSessionComplete.Broadcast(bWasSuccessful);	
}

void UMssSubsystem::OnFindSessionsCompleteCallback(bool bWasSuccessful)
{
	SUBSYS_LOG(TEXT("OnFindSessionsCompleteCallback Completed. Success: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"));

	bFindSessionsInProgress = false;
	
	if (SessionInterface)
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

	if (!LastCreatedSessionSearch.IsValid())
	{
		SUBSYS_ERROR(TEXT("LastCreatedSessionSearch Invalid"));
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), bWasSuccessful);
		return;
	}
		
	if (LastCreatedSessionSearch->SearchResults.IsEmpty())
	{
		SUBSYS_ERROR(TEXT("LastCreatedSessionSearch empty"));
		MultiplayerSessionsOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), bWasSuccessful);
		return;
	}

	MultiplayerSessionsOnFindSessionsComplete.Broadcast(LastCreatedSessionSearch->SearchResults, bWasSuccessful);
}

void UMssSubsystem::OnJoinSessionCompleteCallback(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	SUBSYS_LOG(TEXT("OnJoinSessionComplete Completed"));

	if (SessionInterface)
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

	MultiplayerSessionsOnJoinSessionsComplete.Broadcast(Result);
}

void UMssSubsystem::OnDestroySessionCompleteCallback(FName SessionName, bool bWasSuccessful)
{
	SUBSYS_LOG(TEXT("OnDestroySessionComplete Completed. Success: %s"), bWasSuccessful ? TEXT("success") : TEXT("failed"));

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
	SUBSYS_LOG(TEXT("OnStartSessionCompleteCallback SessionName: %s | Success: %s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (SessionInterface.IsValid())
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);

	MultiplayerSessionsOnStartSessionComplete.Broadcast(bWasSuccessful);

	if (!bWasSuccessful)
	{
		SUBSYS_ERROR(TEXT("OnStartSessionCompleteCallback StartSession FAILED"));
		return;
	}
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
